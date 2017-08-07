#ifndef DEFINE_H
#define DEFINE_H

#include <assert.h>

#define ASSERT(con) assert(con)

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&);                \
    TypeName& operator=(const TypeName&)
#endif

#ifdef __GNUC__
#define likely(x)       (__builtin_expect(!!(x), 1))
#define unlikely(x)     (__builtin_expect(!!(x), 0))
#else
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif


#define __define_always_inline inline __attribute__((always_inline))
#define __define_aligned(n)    __attribute__((__aligned__(n)))
