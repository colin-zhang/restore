#ifndef ENDIAN_H_
#define ENDIAN_H_ 

#include "port_posix.h"

inline int16_t hton16(int16_t v) { return (int16_t)port::htobe((uint16_t)v); }
inline int32_t hton32(int32_t v) { return (int32_t)port::htobe((uint32_t)v); }
inline int64_t hton64(int64_t v) { return (int64_t)port::htobe((uint64_t)v); }

inline int16_t ntoh16(int16_t v) { return (int16_t)port::betoh((uint16_t)v); }
inline int32_t ntoh32(int32_t v) { return (int32_t)port::betoh((uint32_t)v); }
inline int64_t ntoh64(int64_t v) { return (int64_t)port::betoh((uint64_t)v); }

#endif
