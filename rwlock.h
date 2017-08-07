#ifndef RW_LOCK_H_
#define RW_LOCK_H_

#include "atomic.h"

typedef struct RwLock {
    volatile int32_t cnt; /**< -1 when W lock held, > 0 when R locks held. */
    RwLock() : cnt(0) {};
} RwLock;

#define RWLOCK_INITIALIZER { 0 }

static inline void
rwlock_init(RwLock *rwl, void* p)
{
    rwl->cnt = 0;
}

static inline void
rwlock_deinit(RwLock *rwl)
{
    
}

static inline void rwlock_read_lock(RwLock *rwl)
{
    int32_t x;
    int success = 0;

    while (success == 0) {
        x = rwl->cnt;
        /* write lock is held */
        if (x < 0) {
            Pause();
            continue;
        }
        success = AtomicCAS((volatile uint32_t *)&rwl->cnt, x, x + 1);
    }
}

static inline bool rwlock_read_trylock(RwLock *rwl)
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
        success = AtomicCAS((volatile uint32_t *)&rwl->cnt, x, x + 1);
    }
    return 1;
}

static inline void rwlock_read_unlock(RwLock *rwl)
{
    AtomicFetchSub(&rwl->cnt, 1);
}

static inline void rwlock_write_lock(RwLock *rwl)
{
    int32_t x;
    int success = 0;

    while (success == 0) {
        x = rwl->cnt;
        /* a lock is held */
        if (x != 0) {
            Pause();
            continue;
        }
        success = AtomicCAS((volatile uint32_t *)&rwl->cnt, 0, -1);
    }
}

static inline void rwlock_write_unlock(RwLock *rwl)
{
    AtomicFetchAdd(&rwl->cnt, 1);
}

#endif