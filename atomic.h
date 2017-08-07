#ifndef ATOMIC_H_
#define ATOMIC_H_

#ifdef __SSE2__
#include <emmintrin.h>
//PAUSE instruction for tight loops (avoid busy waiting)
static inline void
Pause(void)
{
    _mm_pause();
}
#else 
Pause(void)
{

}
#endif

#define CompilerBarrier() do {             \
    asm volatile ("" : : : "memory");       \
} while(0)

#define AtomicFetchAdd(a_ptr, a_count) __sync_fetch_and_add (a_ptr, a_count)
#define AtomicFetchSub(a_ptr, a_count) __sync_fetch_and_sub (a_ptr, a_count)
#define AtomicCAS(a_ptr, a_oldVal, a_newVal) __sync_bool_compare_and_swap(a_ptr, a_oldVal, a_newVal)

#endif