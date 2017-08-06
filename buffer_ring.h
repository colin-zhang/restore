#ifndef BUFFER_RING_H_
#define BUFFER_RING_H_ 

#include "define.h"

#ifdef __SSE2__
#include <emmintrin.h>
/**
 * PAUSE instruction for tight loops (avoid busy waiting)
 */
static inline void
Pause(void)
{
    _mm_pause();
}
#endif

#define CompilerBarrier() do {             \
    asm volatile ("" : : : "memory");       \
} while(0)

#define AtomicAdd(a_ptr,a_count) __sync_fetch_and_add (a_ptr, a_count)
#define CAS(a_ptr, a_oldVal, a_newVal) __sync_bool_compare_and_swap(a_ptr, a_oldVal, a_newVal)

enum rte_ring_queue_behavior {
    RTE_RING_QUEUE_FIXED = 0, /* Enq/Deq a fixed number of items from a ring */
    RTE_RING_QUEUE_VARIABLE   /* Enq/Deq as many items as possible from ring */
};

#define RTE_RING_MZ_PREFIX "RG_"
/**< The maximum length of a ring name. */
#define RTE_RING_NAMESIZE (RTE_MEMZONE_NAMESIZE - \
               sizeof(RTE_RING_MZ_PREFIX) + 1)



#if RTE_CACHE_LINE_SIZE < 128
#define PROD_ALIGN (RTE_CACHE_LINE_SIZE * 2)
#define CONS_ALIGN (RTE_CACHE_LINE_SIZE * 2)
#else
#define PROD_ALIGN RTE_CACHE_LINE_SIZE
#define CONS_ALIGN RTE_CACHE_LINE_SIZE
#endif

#define __rte_always_inline inline __attribute__((always_inline))


/* structure to hold a pair of head/tail values and other metadata */
struct rte_ring_headtail {
    volatile uint32_t head;  /**< Prod/consumer head. */
    volatile uint32_t tail;  /**< Prod/consumer tail. */
    uint32_t single;         /**< True if single prod/cons */
};

/**
 * An RTE ring structure.
 *
 * The producer and the consumer have a head and a tail index. The particularity
 * of these index is that they are not between 0 and size(ring). These indexes
 * are between 0 and 2^32, and we mask their value when we access the ring[]
 * field. Thanks to this assumption, we can do subtractions between 2 index
 * values in a modulo-32bit base: that's why the overflow of the indexes is not
 * a problem.
 */
struct rte_ring {
    /*
     * Note: this field kept the RTE_MEMZONE_NAMESIZE size due to ABI
     * compatibility requirements, it could be changed to RTE_RING_NAMESIZE
     * next time the ABI changes
     */
    char name[64];// __rte_cache_aligned; /**< Name of the ring. */
    int flags;               /**< Flags supplied at creation. */
            /**< Memzone, if any, containing the rte_ring */
    uint32_t size;           /**< Size of ring. */
    uint32_t mask;           /**< Mask (size-1) of ring. */
    uint32_t capacity;       /**< Usable size of ring */

    /** Ring producer status. */
    struct rte_ring_headtail prod ;//__rte_aligned(PROD_ALIGN);

    /** Ring consumer status. */
    struct rte_ring_headtail cons ;//__rte_aligned(CONS_ALIGN);
};

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
#define RTE_RING_SZ_MASK  (unsigned)(0x0fffffff) /**< Ring size mask */

/* @internal defines for passing to the enqueue dequeue worker functions */
#define __IS_SP 1
#define __IS_MP 0
#define __IS_SC 1
#define __IS_MC 0


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

static __rte_always_inline void
update_tail(struct rte_ring_headtail *ht, uint32_t old_val, uint32_t new_val,
        uint32_t single)
{
    /*
     * If there are other enqueues/dequeues in progress that preceded us,
     * we need to wait for them to complete
     */
    if (!single)
        while (unlikely(ht->tail != old_val))
            Pause();

    ht->tail = new_val;
}

/**
 * @internal This function updates the producer head for enqueue
 *
 * @param r
 *   A pointer to the ring structure
 * @param is_sp
 *   Indicates whether multi-producer path is needed or not
 * @param n
 *   The number of elements we will want to enqueue, i.e. how far should the
 *   head be moved
 * @param behavior
 *   RTE_RING_QUEUE_FIXED:    Enqueue a fixed number of items from a ring
 *   RTE_RING_QUEUE_VARIABLE: Enqueue as many items as possible from ring
 * @param old_head
 *   Returns head value as it was before the move, i.e. where enqueue starts
 * @param new_head
 *   Returns the current/new head value i.e. where enqueue finishes
 * @param free_entries
 *   Returns the amount of free space in the ring BEFORE head was moved
 * @return
 *   Actual number of objects enqueued.
 *   If behavior == RTE_RING_QUEUE_FIXED, this will be 0 or n only.
 */
static __rte_always_inline unsigned int
__rte_ring_move_prod_head(struct rte_ring *r, int is_sp,
        unsigned int n, enum rte_ring_queue_behavior behavior,
        uint32_t *old_head, uint32_t *new_head,
        uint32_t *free_entries)
{
    const uint32_t capacity = r->capacity;
    unsigned int max = n;
    int success;

    do {
        /* Reset n to the initial burst count */
        n = max;

        *old_head = r->prod.head;
        const uint32_t cons_tail = r->cons.tail;
        /*
         *  The subtraction is done between two unsigned 32bits value
         * (the result is always modulo 32 bits even if we have
         * *old_head > cons_tail). So 'free_entries' is always between 0
         * and capacity (which is < size).
         */
        *free_entries = (capacity + cons_tail - *old_head);

        /* check that we have enough room in ring */
        if (unlikely(n > *free_entries))
            n = (behavior == RTE_RING_QUEUE_FIXED) ?
                    0 : *free_entries;

        if (n == 0)
            return 0;

        *new_head = *old_head + n;
        if (is_sp)
            r->prod.head = *new_head, success = 1;
        else
            success = CAS(&r->prod.head, *old_head, *new_head);
    } while (unlikely(success == 0));
    return n;
}

/**
 * @internal Enqueue several objects on the ring
 *
  * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to add in the ring from the obj_table.
 * @param behavior
 *   RTE_RING_QUEUE_FIXED:    Enqueue a fixed number of items from a ring
 *   RTE_RING_QUEUE_VARIABLE: Enqueue as many items as possible from ring
 * @param is_sp
 *   Indicates whether to use single producer or multi-producer head update
 * @param free_space
 *   returns the amount of space after the enqueue operation has finished
 * @return
 *   Actual number of objects enqueued.
 *   If behavior == RTE_RING_QUEUE_FIXED, this will be 0 or n only.
 */
static __rte_always_inline unsigned int
__rte_ring_do_enqueue(struct rte_ring *r, void * const *obj_table,
         unsigned int n, enum rte_ring_queue_behavior behavior,
         int is_sp, unsigned int *free_space)
{
    uint32_t prod_head, prod_next;
    uint32_t free_entries;

    n = __rte_ring_move_prod_head(r, is_sp, n, behavior,
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

/**
 * @internal This function updates the consumer head for dequeue
 *
 * @param r
 *   A pointer to the ring structure
 * @param is_sc
 *   Indicates whether multi-consumer path is needed or not
 * @param n
 *   The number of elements we will want to enqueue, i.e. how far should the
 *   head be moved
 * @param behavior
 *   RTE_RING_QUEUE_FIXED:    Dequeue a fixed number of items from a ring
 *   RTE_RING_QUEUE_VARIABLE: Dequeue as many items as possible from ring
 * @param old_head
 *   Returns head value as it was before the move, i.e. where dequeue starts
 * @param new_head
 *   Returns the current/new head value i.e. where dequeue finishes
 * @param entries
 *   Returns the number of entries in the ring BEFORE head was moved
 * @return
 *   - Actual number of objects dequeued.
 *     If behavior == RTE_RING_QUEUE_FIXED, this will be 0 or n only.
 */
static __rte_always_inline unsigned int
__rte_ring_move_cons_head(struct rte_ring *r, int is_sc,
        unsigned int n, enum rte_ring_queue_behavior behavior,
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
            n = (behavior == RTE_RING_QUEUE_FIXED) ? 0 : *entries;

        if (unlikely(n == 0))
            return 0;

        *new_head = *old_head + n;
        if (is_sc)
            r->cons.head = *new_head, success = 1;
        else
            success = CAS(&r->cons.head, *old_head, *new_head);
    } while (unlikely(success == 0));
    return n;
}

/**
 * @internal Dequeue several objects from the ring
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to pull from the ring.
 * @param behavior
 *   RTE_RING_QUEUE_FIXED:    Dequeue a fixed number of items from a ring
 *   RTE_RING_QUEUE_VARIABLE: Dequeue as many items as possible from ring
 * @param is_sc
 *   Indicates whether to use single consumer or multi-consumer head update
 * @param available
 *   returns the number of remaining ring entries after the dequeue has finished
 * @return
 *   - Actual number of objects dequeued.
 *     If behavior == RTE_RING_QUEUE_FIXED, this will be 0 or n only.
 */
static __rte_always_inline unsigned int
__rte_ring_do_dequeue(struct rte_ring *r, void **obj_table,
         unsigned int n, enum rte_ring_queue_behavior behavior,
         int is_sc, unsigned int *available)
{
    uint32_t cons_head, cons_next;
    uint32_t entries;

    n = __rte_ring_move_cons_head(r, is_sc, n, behavior,
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

/**
 * Enqueue several objects on the ring (multi-producers safe).
 *
 * This function uses a "compare and set" instruction to move the
 * producer index atomically.
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to add in the ring from the obj_table.
 * @param free_space
 *   if non-NULL, returns the amount of space in the ring after the
 *   enqueue operation has finished.
 * @return
 *   The number of objects enqueued, either 0 or n
 */
static __rte_always_inline unsigned int
rte_ring_mp_enqueue_bulk(struct rte_ring *r, void * const *obj_table,
             unsigned int n, unsigned int *free_space)
{
    return __rte_ring_do_enqueue(r, obj_table, n, RTE_RING_QUEUE_FIXED,
            __IS_MP, free_space);
}

/**
 * Enqueue several objects on a ring (NOT multi-producers safe).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to add in the ring from the obj_table.
 * @param free_space
 *   if non-NULL, returns the amount of space in the ring after the
 *   enqueue operation has finished.
 * @return
 *   The number of objects enqueued, either 0 or n
 */
static __rte_always_inline unsigned int
rte_ring_sp_enqueue_bulk(struct rte_ring *r, void * const *obj_table,
             unsigned int n, unsigned int *free_space)
{
    return __rte_ring_do_enqueue(r, obj_table, n, RTE_RING_QUEUE_FIXED,
            __IS_SP, free_space);
}

/**
 * Enqueue several objects on a ring.
 *
 * This function calls the multi-producer or the single-producer
 * version depending on the default behavior that was specified at
 * ring creation time (see flags).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to add in the ring from the obj_table.
 * @param free_space
 *   if non-NULL, returns the amount of space in the ring after the
 *   enqueue operation has finished.
 * @return
 *   The number of objects enqueued, either 0 or n
 */
static __rte_always_inline unsigned int
rte_ring_enqueue_bulk(struct rte_ring *r, void * const *obj_table,
              unsigned int n, unsigned int *free_space)
{
    return __rte_ring_do_enqueue(r, obj_table, n, RTE_RING_QUEUE_FIXED,
            r->prod.single, free_space);
}

/**
 * Enqueue one object on a ring (multi-producers safe).
 *
 * This function uses a "compare and set" instruction to move the
 * producer index atomically.
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj
 *   A pointer to the object to be added.
 * @return
 *   - 0: Success; objects enqueued.
 *   - -ENOBUFS: Not enough room in the ring to enqueue; no object is enqueued.
 */
static __rte_always_inline int
rte_ring_mp_enqueue(struct rte_ring *r, void *obj)
{
    return rte_ring_mp_enqueue_bulk(r, &obj, 1, NULL) ? 0 : -ENOBUFS;
}

/**
 * Enqueue one object on a ring (NOT multi-producers safe).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj
 *   A pointer to the object to be added.
 * @return
 *   - 0: Success; objects enqueued.
 *   - -ENOBUFS: Not enough room in the ring to enqueue; no object is enqueued.
 */
static __rte_always_inline int
rte_ring_sp_enqueue(struct rte_ring *r, void *obj)
{
    return rte_ring_sp_enqueue_bulk(r, &obj, 1, NULL) ? 0 : -ENOBUFS;
}

/**
 * Enqueue one object on a ring.
 *
 * This function calls the multi-producer or the single-producer
 * version, depending on the default behaviour that was specified at
 * ring creation time (see flags).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj
 *   A pointer to the object to be added.
 * @return
 *   - 0: Success; objects enqueued.
 *   - -ENOBUFS: Not enough room in the ring to enqueue; no object is enqueued.
 */
static __rte_always_inline int
rte_ring_enqueue(struct rte_ring *r, void *obj)
{
    return rte_ring_enqueue_bulk(r, &obj, 1, NULL) ? 0 : -ENOBUFS;
}

/**
 * Dequeue several objects from a ring (multi-consumers safe).
 *
 * This function uses a "compare and set" instruction to move the
 * consumer index atomically.
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects) that will be filled.
 * @param n
 *   The number of objects to dequeue from the ring to the obj_table.
 * @param available
 *   If non-NULL, returns the number of remaining ring entries after the
 *   dequeue has finished.
 * @return
 *   The number of objects dequeued, either 0 or n
 */
static __rte_always_inline unsigned int
rte_ring_mc_dequeue_bulk(struct rte_ring *r, void **obj_table,
        unsigned int n, unsigned int *available)
{
    return __rte_ring_do_dequeue(r, obj_table, n, RTE_RING_QUEUE_FIXED,
            __IS_MC, available);
}

/**
 * Dequeue several objects from a ring (NOT multi-consumers safe).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects) that will be filled.
 * @param n
 *   The number of objects to dequeue from the ring to the obj_table,
 *   must be strictly positive.
 * @param available
 *   If non-NULL, returns the number of remaining ring entries after the
 *   dequeue has finished.
 * @return
 *   The number of objects dequeued, either 0 or n
 */
static __rte_always_inline unsigned int
rte_ring_sc_dequeue_bulk(struct rte_ring *r, void **obj_table,
        unsigned int n, unsigned int *available)
{
    return __rte_ring_do_dequeue(r, obj_table, n, RTE_RING_QUEUE_FIXED,
            __IS_SC, available);
}

/**
 * Dequeue several objects from a ring.
 *
 * This function calls the multi-consumers or the single-consumer
 * version, depending on the default behaviour that was specified at
 * ring creation time (see flags).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects) that will be filled.
 * @param n
 *   The number of objects to dequeue from the ring to the obj_table.
 * @param available
 *   If non-NULL, returns the number of remaining ring entries after the
 *   dequeue has finished.
 * @return
 *   The number of objects dequeued, either 0 or n
 */
static __rte_always_inline unsigned int
rte_ring_dequeue_bulk(struct rte_ring *r, void **obj_table, unsigned int n,
        unsigned int *available)
{
    return __rte_ring_do_dequeue(r, obj_table, n, RTE_RING_QUEUE_FIXED,
                r->cons.single, available);
}

/**
 * Dequeue one object from a ring (multi-consumers safe).
 *
 * This function uses a "compare and set" instruction to move the
 * consumer index atomically.
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_p
 *   A pointer to a void * pointer (object) that will be filled.
 * @return
 *   - 0: Success; objects dequeued.
 *   - -ENOENT: Not enough entries in the ring to dequeue; no object is
 *     dequeued.
 */
static __rte_always_inline int
rte_ring_mc_dequeue(struct rte_ring *r, void **obj_p)
{
    return rte_ring_mc_dequeue_bulk(r, obj_p, 1, NULL)  ? 0 : -ENOENT;
}

/**
 * Dequeue one object from a ring (NOT multi-consumers safe).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_p
 *   A pointer to a void * pointer (object) that will be filled.
 * @return
 *   - 0: Success; objects dequeued.
 *   - -ENOENT: Not enough entries in the ring to dequeue, no object is
 *     dequeued.
 */
static __rte_always_inline int
rte_ring_sc_dequeue(struct rte_ring *r, void **obj_p)
{
    return rte_ring_sc_dequeue_bulk(r, obj_p, 1, NULL) ? 0 : -ENOENT;
}

/**
 * Dequeue one object from a ring.
 *
 * This function calls the multi-consumers or the single-consumer
 * version depending on the default behaviour that was specified at
 * ring creation time (see flags).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_p
 *   A pointer to a void * pointer (object) that will be filled.
 * @return
 *   - 0: Success, objects dequeued.
 *   - -ENOENT: Not enough entries in the ring to dequeue, no object is
 *     dequeued.
 */
static __rte_always_inline int
rte_ring_dequeue(struct rte_ring *r, void **obj_p)
{
    return rte_ring_dequeue_bulk(r, obj_p, 1, NULL) ? 0 : -ENOENT;
}

/**
 * Return the number of entries in a ring.
 *
 * @param r
 *   A pointer to the ring structure.
 * @return
 *   The number of entries in the ring.
 */
static inline unsigned
rte_ring_count(const struct rte_ring *r)
{
    uint32_t prod_tail = r->prod.tail;
    uint32_t cons_tail = r->cons.tail;
    uint32_t count = (prod_tail - cons_tail) & r->mask;
    return (count > r->capacity) ? r->capacity : count;
}

/**
 * Return the number of free entries in a ring.
 *
 * @param r
 *   A pointer to the ring structure.
 * @return
 *   The number of free entries in the ring.
 */
static inline unsigned
rte_ring_free_count(const struct rte_ring *r)
{
    return r->capacity - rte_ring_count(r);
}

/**
 * Test if a ring is full.
 *
 * @param r
 *   A pointer to the ring structure.
 * @return
 *   - 1: The ring is full.
 *   - 0: The ring is not full.
 */
static inline int
rte_ring_full(const struct rte_ring *r)
{
    return rte_ring_free_count(r) == 0;
}

/**
 * Test if a ring is empty.
 *
 * @param r
 *   A pointer to the ring structure.
 * @return
 *   - 1: The ring is empty.
 *   - 0: The ring is not empty.
 */
static inline int
rte_ring_empty(const struct rte_ring *r)
{
    return rte_ring_count(r) == 0;
}

/**
 * Return the size of the ring.
 *
 * @param r
 *   A pointer to the ring structure.
 * @return
 *   The size of the data store used by the ring.
 *   NOTE: this is not the same as the usable space in the ring. To query that
 *   use ``rte_ring_get_capacity()``.
 */
static inline unsigned int
rte_ring_get_size(const struct rte_ring *r)
{
    return r->size;
}

/**
 * Return the number of elements which can be stored in the ring.
 *
 * @param r
 *   A pointer to the ring structure.
 * @return
 *   The usable size of the ring.
 */
static inline unsigned int
rte_ring_get_capacity(const struct rte_ring *r)
{
    return r->capacity;
}



#endif