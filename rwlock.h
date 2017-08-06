#ifndef RW_LOCK_H_
#define RW_LOCK_H_

#include <xmmintrin.h>

typedef struct rw_lock {
    volatile int32_t cnt; /**< -1 when W lock held, > 0 when R locks held. */
    rw_lock() : cnt(0) {};
} rwlock_t;

#define RWLOCK_INITIALIZER { 0 }

static inline void
rwlock_init(rwlock_t *rwl, void* p)
{
    rwl->cnt = 0;
}

static inline void
rwlock_deinit(rwlock_t *rwl)
{
    
}

static inline void rwlock_read_lock(rwlock_t *rwl)
{
    int32_t x;
    int success = 0;

    while (success == 0) {
        x = rwl->cnt;
        /* write lock is held */
        if (x < 0) {
            _mm_pause();
            continue;
        }
        success = __sync_bool_compare_and_swap((volatile uint32_t *)&rwl->cnt, x, x + 1);
    }
}

static inline bool rwlock_read_trylock(rwlock_t *rwl)
{
    //
    // int32_t x = rwl->cnt;
    // if (x < 0) {
    //     return 0;
    // }
    // return __sync_add_and_fetch(&rwl->cnt, 1) != 0;
    int32_t x;
    int success = 0;

    while (success == 0) {
        x = rwl->cnt;
        /* write lock is held */
        if (x < 0) {
            return 0;
        }
        success = __sync_bool_compare_and_swap((volatile uint32_t *)&rwl->cnt, x, x + 1);
    }
    return 1;
}

static inline void rwlock_read_unlock(rwlock_t *rwl)
{
    __sync_fetch_and_sub(&rwl->cnt, 1);
}

static inline void rwlock_write_lock(rwlock_t *rwl)
{
    int32_t x;
    int success = 0;

    while (success == 0) {
        x = rwl->cnt;
        /* a lock is held */
        if (x != 0) {
            _mm_pause();
            continue;
        }
        success = __sync_bool_compare_and_swap((volatile uint32_t *)&rwl->cnt, 0, -1);
    }
}

static inline void rwlock_write_unlock(rwlock_t *rwl)
{
    __sync_fetch_and_add(&rwl->cnt, 1);
}

#endif