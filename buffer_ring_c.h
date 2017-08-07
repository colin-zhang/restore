#ifndef BUFFER_RING_H_
#define BUFFER_RING_H_ 

#include "define.h"
#include "atomic.h"

#define __ring_always_inline inline __attribute__((always_inline))

#define CACHE_LINE_SIZE 64
#if CACHE_LINE_SIZE < 128
#define PROD_ALIGN (CACHE_LINE_SIZE * 2)
#define CONS_ALIGN (CACHE_LINE_SIZE * 2)
#else
#define PROD_ALIGN CACHE_LINE_SIZE
#define CONS_ALIGN CACHE_LINE_SIZE
#endif

#define RING_F_SP_ENQ 0x0001 /**< The default enqueue is "single-producer". */
#define RING_F_SC_DEQ 0x0002 /**< The default dequeue is "single-consumer". */
/**
 * Ring is to hold exactly requested number of entries.
 * Without this flag set, the ring size requested must be a power of 2, and the
 * usable space will be that size - 1. With the flag, the requested size will
 * be rounded up to the next power of two, but the usable space will be exactly
 * that requested. Worst case, if a power-of-2 size is requested, half the
 * ring space will be wasted.
 */
#define RING_F_EXACT_SZ 0x0004
#define RING_SZ_MASK  (unsigned)(0x0fffffff) /**< Ring size mask */

/* @internal defines for passing to the enqueue dequeue worker functions */
#define __IS_SP 1
#define __IS_MP 0
#define __IS_SC 1
#define __IS_MC 0

enum ring_queue_behavior {
    RING_QUEUE_FIXED = 0, /* Enq/Deq a fixed number of items from a ring */
    RING_QUEUE_VARIABLE   /* Enq/Deq as many items as possible from ring */
};

/* structure to hold a pair of head/tail values and other metadata */
struct ring_headtail {
    volatile uint32_t head;  /**< Prod/consumer head. */
    volatile uint32_t tail;  /**< Prod/consumer tail. */
    uint32_t single;         /**< True if single prod/cons */
};

struct buffer_ring 
{
    char name[64];
    int flags; 
    uint32_t size;
    uint32_t mask;
    uint32_t capacity;
    struct ring_headtail prod  __attribute__((__aligned__(PROD_ALIGN)));
    struct ring_headtail cons __attribute__((__aligned__(CONS_ALIGN)));
};

/* the actual enqueue of pointers on the ring.
 * Placed here since identical code needed in both
 * single and multi producer enqueue functions */
#define ENQUEUE_PTRS(r, ring_start, prod_head, obj_table, n, obj_type) do { \
    unsigned int i; \
    const uint32_t size = (r)->size; \
    uint32_t idx = prod_head & (r)->mask; \
    obj_type *ring = (obj_type *)ring_start; \
    if (likely(idx + n < size)) { \
        for (i = 0; i < (n & ((~(unsigned)0x3))); i+=4, idx+=4) { \
            ring[idx] = obj_table[i]; \
            ring[idx+1] = obj_table[i+1]; \
            ring[idx+2] = obj_table[i+2]; \
            ring[idx+3] = obj_table[i+3]; \
        } \
        switch (n & 0x3) { \
        case 3: \
            ring[idx++] = obj_table[i++]; /* fallthrough */ \
        case 2: \
            ring[idx++] = obj_table[i++]; /* fallthrough */ \
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

/* the actual copy of pointers on the ring to obj_table.
 * Placed here since identical code needed in both
 * single and multi consumer dequeue functions */
#define DEQUEUE_PTRS(r, ring_start, cons_head, obj_table, n, obj_type) do { \
    unsigned int i; \
    uint32_t idx = cons_head & (r)->mask; \
    const uint32_t size = (r)->size; \
    obj_type *ring = (obj_type *)ring_start; \
    if (likely(idx + n < size)) { \
        for (i = 0; i < (n & (~(unsigned)0x3)); i+=4, idx+=4) {\
            obj_table[i] = ring[idx]; \
            obj_table[i+1] = ring[idx+1]; \
            obj_table[i+2] = ring[idx+2]; \
            obj_table[i+3] = ring[idx+3]; \
        } \
        switch (n & 0x3) { \
        case 3: \
            obj_table[i++] = ring[idx++]; /* fallthrough */ \
        case 2: \
            obj_table[i++] = ring[idx++]; /* fallthrough */ \
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

static __ring_always_inline void
update_tail(struct ring_headtail *ht, uint32_t old_val, uint32_t new_val,
        uint32_t single)
{
    if (!single)
        while (unlikely(ht->tail != old_val))
            Pause();
    ht->tail = new_val;
}

static __ring_always_inline unsigned int
__ring_move_prod_head(struct buffer_ring *r, int is_sp,
        unsigned int n, enum ring_queue_behavior behavior,
        uint32_t *old_head, uint32_t *new_head,
        uint32_t *free_entries)
{
    const uint32_t capacity = r->capacity;
    unsigned int max = n;
    int success;

    do {
        n = max;

        *old_head = r->prod.head;
        const uint32_t cons_tail = r->cons.tail;
        *free_entries = (capacity + cons_tail - *old_head);
        if (unlikely(n > *free_entries))
            n = (behavior == RING_QUEUE_FIXED) ?
                    0 : *free_entries;

        if (n == 0)
            return 0;

        *new_head = *old_head + n;
        if (is_sp)
            r->prod.head = *new_head, success = 1;
        else
            success = AtomicCAS(&r->prod.head, *old_head, *new_head);
    } while (unlikely(success == 0));
    return n;
}

static __ring_always_inline unsigned int
__ring_do_enqueue(struct buffer_ring *r, void * const *obj_table,
         unsigned int n, enum ring_queue_behavior behavior,
         int is_sp, unsigned int *free_space)
{
    uint32_t prod_head, prod_next;
    uint32_t free_entries;

    n = __ring_move_prod_head(r, is_sp, n, behavior,
            &prod_head, &prod_next, &free_entries);
    if (n == 0)
        goto end;

    ENQUEUE_PTRS(r, &r[1], prod_head, obj_table, n, void *);
    CompilerBarrier();

    update_tail(&r->prod, prod_head, prod_next, is_sp);
end:
    if (free_space != NULL)
        *free_space = free_entries - n;
    return n;
}

static __ring_always_inline unsigned int
__ring_move_cons_head(struct buffer_ring *r, int is_sc,
        unsigned int n, enum ring_queue_behavior behavior,
        uint32_t *old_head, uint32_t *new_head,
        uint32_t *entries)
{
    unsigned int max = n;
    int success;

    /* move cons.head atomically */
    do {
        /* Restore n as it may change every loop */
        n = max;

        *old_head = r->cons.head;
        const uint32_t prod_tail = r->prod.tail;
        /* The subtraction is done between two unsigned 32bits value
         * (the result is always modulo 32 bits even if we have
         * cons_head > prod_tail). So 'entries' is always between 0
         * and size(ring)-1. */
        *entries = (prod_tail - *old_head);

        /* Set the actual entries for dequeue */
        if (n > *entries)
            n = (behavior == RING_QUEUE_FIXED) ? 0 : *entries;

        if (unlikely(n == 0))
            return 0;

        *new_head = *old_head + n;
        if (is_sc)
            r->cons.head = *new_head, success = 1;
        else
            success = AtomicCAS(&r->cons.head, *old_head, *new_head);
    } while (unlikely(success == 0));
    return n;
}

static __ring_always_inline unsigned int
__ring_do_dequeue(struct buffer_ring *r, void **obj_table,
         unsigned int n, enum ring_queue_behavior behavior,
         int is_sc, unsigned int *available)
{
    uint32_t cons_head, cons_next;
    uint32_t entries;

    n = __ring_move_cons_head(r, is_sc, n, behavior,
            &cons_head, &cons_next, &entries);
    if (n == 0)
        goto end;

    DEQUEUE_PTRS(r, &r[1], cons_head, obj_table, n, void *);
    CompilerBarrier();

    update_tail(&r->cons, cons_head, cons_next, is_sc);

end:
    if (available != NULL)
        *available = entries - n;
    return n;
}

static __ring_always_inline unsigned int
ring_mp_enqueue_bulk(struct buffer_ring *r, void * const *obj_table,
             unsigned int n, unsigned int *free_space)
{
    return __ring_do_enqueue(r, obj_table, n, RING_QUEUE_FIXED,
            __IS_MP, free_space);
}

static __ring_always_inline unsigned int
ring_sp_enqueue_bulk(struct buffer_ring *r, void * const *obj_table,
             unsigned int n, unsigned int *free_space)
{
    return __ring_do_enqueue(r, obj_table, n, RING_QUEUE_FIXED,
            __IS_SP, free_space);
}

static __ring_always_inline unsigned int
ring_enqueue_bulk(struct buffer_ring *r, void * const *obj_table,
              unsigned int n, unsigned int *free_space)
{
    return __ring_do_enqueue(r, obj_table, n, RING_QUEUE_FIXED,
            r->prod.single, free_space);
}

static __ring_always_inline int
ring_mp_enqueue(struct buffer_ring *r, void *obj)
{
    return ring_mp_enqueue_bulk(r, &obj, 1, NULL) ? 0 : -ENOBUFS;
}

static __ring_always_inline int
ring_sp_enqueue(struct buffer_ring *r, void *obj)
{
    return ring_sp_enqueue_bulk(r, &obj, 1, NULL) ? 0 : -ENOBUFS;
}

static __ring_always_inline int
ring_enqueue(struct buffer_ring *r, void *obj)
{
    return ring_enqueue_bulk(r, &obj, 1, NULL) ? 0 : -ENOBUFS;
}

static __ring_always_inline unsigned int
ring_mc_dequeue_bulk(struct buffer_ring *r, void **obj_table,
        unsigned int n, unsigned int *available)
{
    return __ring_do_dequeue(r, obj_table, n, RING_QUEUE_FIXED,
            __IS_MC, available);
}

static __ring_always_inline unsigned int
ring_sc_dequeue_bulk(struct buffer_ring *r, void **obj_table,
        unsigned int n, unsigned int *available)
{
    return __ring_do_dequeue(r, obj_table, n, RING_QUEUE_FIXED,
            __IS_SC, available);
}

static __ring_always_inline unsigned int
ring_dequeue_bulk(struct buffer_ring *r, void **obj_table, unsigned int n,
        unsigned int *available)
{
    return __ring_do_dequeue(r, obj_table, n, RING_QUEUE_FIXED,
                r->cons.single, available);
}

static __ring_always_inline int
ring_mc_dequeue(struct buffer_ring *r, void **obj_p)
{
    return ring_mc_dequeue_bulk(r, obj_p, 1, NULL)  ? 0 : -ENOENT;
}

static __ring_always_inline int
ring_sc_dequeue(struct buffer_ring *r, void **obj_p)
{
    return ring_sc_dequeue_bulk(r, obj_p, 1, NULL) ? 0 : -ENOENT;
}

static __ring_always_inline int
ring_dequeue(struct buffer_ring *r, void **obj_p)
{
    return ring_dequeue_bulk(r, obj_p, 1, NULL) ? 0 : -ENOENT;
}

static inline unsigned
ring_count(const struct buffer_ring *r)
{
    uint32_t prod_tail = r->prod.tail;
    uint32_t cons_tail = r->cons.tail;
    uint32_t count = (prod_tail - cons_tail) & r->mask;
    return (count > r->capacity) ? r->capacity : count;
}

static inline unsigned
ring_free_count(const struct buffer_ring *r)
{
    return r->capacity - ring_count(r);
}

static inline int
ring_full(const struct buffer_ring *r)
{
    return ring_free_count(r) == 0;
}

static inline int
ring_empty(const struct buffer_ring *r)
{
    return ring_count(r) == 0;
}

static inline unsigned int
ring_get_size(const struct buffer_ring *r)
{
    return r->size;
}

static inline unsigned int
ring_get_capacity(const struct buffer_ring *r)
{
    return r->capacity;
}

#endif
