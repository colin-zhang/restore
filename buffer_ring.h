#ifndef BUFFER_RING_H_
#define BUFFER_RING_H_

#include "define.h"
#include "atomic.h"

static inline bool
IsPowerOf2(uint32_t n)
{
    return (n != 0 && ((n & (n - 1)) == 0));
}

static inline uint32_t
RoundupPowerOf2(uint32_t a)
{
    int i;
    if (IsPowerOf2(a))
        return a;
    uint32_t position = 0;
    for (i = a; i != 0; i >>= 1)
        position++;
    return (uint32_t)(1 << position);
}

struct RingHeadtail {
    mutable/*volatile*/ uint32_t head;
    mutable/*volatile*/ uint32_t tail;
    RingHeadtail() 
    :  head(0),
       tail(0) {
    }
};

#define ENQUEUE_PTRS(prod_head, obj_table, n) do { \
    uint32_t i; \
    const uint32_t size = size_; \
    uint32_t idx = prod_head & mask_; \
    T *ring = data_; \
    if (likely(idx + n < size)) { \
        for (i = 0; i < (n & ((~(uint32_t)0x3))); i+=4, idx+=4) { \
            ring[idx] = obj_table[i]; \
            ring[idx+1] = obj_table[i+1]; \
            ring[idx+2] = obj_table[i+2]; \
            ring[idx+3] = obj_table[i+3]; \
        } \
        switch (n & 0x3) { \
        case 3: \
            ring[idx++] = obj_table[i++]; \
        case 2: \
            ring[idx++] = obj_table[i++]; \
        case 1: \
            ring[idx++] = obj_table[i++]; \
        } \
    } else { \
        for (i = 0; idx < size; i++, idx++)\
            ring[idx] = obj_table[i]; \
        for (idx = 0; i < n; i++, idx++) \
            ring[idx] = obj_table[i]; \
    } \
} while (0)

#define DEQUEUE_PTRS(cons_head, obj_table, n) do { \
    uint32_t i; \
    uint32_t idx = cons_head & mask_; \
    const uint32_t size = size_; \
    T *ring = data_; \
    if (likely(idx + n < size)) { \
        for (i = 0; i < (n & (~(uint32_t)0x3)); i+=4, idx+=4) {\
            obj_table[i] = ring[idx]; \
            obj_table[i+1] = ring[idx+1]; \
            obj_table[i+2] = ring[idx+2]; \
            obj_table[i+3] = ring[idx+3]; \
        } \
        switch (n & 0x3) { \
        case 3: \
            obj_table[i++] = ring[idx++]; \
        case 2: \
            obj_table[i++] = ring[idx++]; \
        case 1: \
            obj_table[i++] = ring[idx++]; \
        } \
    } else { \
        for (i = 0; idx < size; i++, idx++) \
            obj_table[i] = ring[idx]; \
        for (idx = 0; i < n; i++, idx++) \
            obj_table[i] = ring[idx]; \
    } \
} while (0)


template <typename T>
class BuffRing
{
public:
    enum BuffRingBehavior {
        kRingQueueFixed=0,
        kRingQueueVariable=1,
    };
public:
    BuffRing(uint32_t size, 
             int behavior=kRingQueueFixed,
             bool single_producer = true,
             bool single_consumer = true
            )
      : size_(0), 
        behavior_(behavior),
        single_producer_(single_producer),
        single_consumer_(single_consumer)
    {
        size_ = RoundupPowerOf2(size);
        mask_ = size_ - 1;
        capacity_ = mask_;
        data_ = new T[size_];
    };

    ~BuffRing() {
        delete[] data_;
    }

    void UpdateTail(RingHeadtail* ht, uint32_t old_val, uint32_t new_val, uint32_t single)
    {
        if (!single) {
            while (unlikely(ht->tail != old_val)) {
                Pause();
            }
        }
        ht->tail = new_val;
    }

    uint32_t MoveProdHead(uint32_t n, uint32_t *old_head, uint32_t *new_head, uint32_t *free_entries)
    {
        const uint32_t capacity = capacity_;
        unsigned int max = n;
        int success;

        do {
            n = max;
            *old_head = prod_.head;
            const uint32_t cons_tail = cons_.tail;
            *free_entries = (capacity + cons_tail - *old_head);
            if (unlikely(n > *free_entries))
                n = (behavior_ == kRingQueueFixed) ? 0 : *free_entries;

            if (n == 0)
                return 0;

            *new_head = *old_head + n;
            if (single_producer_)
                prod_.head = *new_head, success = 1;
            else
                success = AtomicCAS(&prod_.head, *old_head, *new_head);
        } while (unlikely(success == 0));
        return n;
    }

    uint32_t MoveConsumerHead(uint32_t n, uint32_t *old_head, uint32_t *new_head, uint32_t *entries)
    {
        unsigned int max = n;
        int success;
        do {
            n = max;
            *old_head = cons_.head;
            const uint32_t prod_tail = prod_.tail;
            *entries = (prod_tail - *old_head);
            if (n > *entries)
                n = (behavior_ == kRingQueueFixed) ? 0 : *entries;

            if (unlikely(n == 0))
                return 0;

            *new_head = *old_head + n;
            if (single_consumer_)
                cons_.head = *new_head, success = 1;
            else
                success = AtomicCAS(&cons_.head, *old_head, *new_head);
        } while (unlikely(success == 0));
        return n;
    }
    
    uint32_t DoEnqueue(const T* obj, uint32_t n, uint32_t *free_space)
    {
        uint32_t prod_head, prod_next;
        uint32_t free_entries;
        do {
            n = MoveProdHead(n, &prod_head, &prod_next, &free_entries);
            if (n == 0) break;    
            ENQUEUE_PTRS(prod_head, obj, n);
            CompilerBarrier();

            UpdateTail(&prod_, prod_head, prod_next, single_producer_);
        } while(0);

        if (free_space != NULL)
            *free_space = free_entries - n;
        return n;
    }

    uint32_t DoEnqueue(const T& obj, uint32_t *free_space)
    {
        return DoEnqueue(&obj, 1, free_space);
    }

    uint32_t DoDequeue(T* obj, uint32_t n, uint32_t *available)
    {
        uint32_t cons_head, cons_next;
        uint32_t entries;
        do {
            n = MoveConsumerHead(n, &cons_head, &cons_next, &entries);
            if (n == 0) break;
            DEQUEUE_PTRS(cons_head, obj, n);
            CompilerBarrier();
            UpdateTail(&cons_, cons_head, cons_next, single_consumer_);
        } while(0);

        if (available != NULL)
            *available = entries - n;
        return n;
    }

    uint32_t RingCount()
    {
        uint32_t prod_tail = prod_.tail;
        uint32_t cons_tail = cons_.tail;
        uint32_t count = (prod_tail - cons_tail) & mask_;
        return (count > capacity_) ? capacity_ : count;
    }

    uint32_t RingFreeCount()
    {
        return capacity_ - RingCount();
    }

    bool RingFull()
    {
        return RingFreeCount() == 0;
    }

    bool RingEmpoty()
    {
        return RingCount() == 0;
    }

    uint32_t RingSize()
    {
        return size_;
    }

    uint32_t RingCapacity()
    {
        return capacity_;
    }

private:
    uint32_t size_;
    uint32_t mask_;
    uint32_t capacity_;
    int      behavior_;
    bool     single_producer_;
    bool     single_consumer_;
    struct   RingHeadtail prod_ __define_aligned(64);
    struct   RingHeadtail cons_ __define_aligned(64);
    T*       data_;
};

#endif
