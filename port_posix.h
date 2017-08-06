#ifndef PORT_POSIX_H_
#define PORT_POSIX_H_

namespace port {
    static const int kLittleEndian = 1;
    inline uint16_t htobe(uint16_t v) {
        if (kLittleEndian) {
            unsigned char* pv = (unsigned char*)&v;
            return uint16_t(pv[0])<<8 | uint16_t(pv[1]);
        }
        return v;
    }
    inline uint32_t htobe(uint32_t v) {
        if (kLittleEndian) {
            unsigned char* pv = (unsigned char*)&v;
            return uint32_t(pv[0])<<24 | uint32_t(pv[1])<<16 | uint32_t(pv[2])<<8 | uint32_t(pv[3]);
        }
        return v;
    }
    inline uint64_t htobe(uint64_t v) {
        if (kLittleEndian) {
            unsigned char* pv = (unsigned char*)&v;
            return uint64_t(pv[0])<<56 | uint64_t(pv[1])<<48 | uint64_t(pv[2])<<40 | uint64_t(pv[3])<<32
                    | uint64_t(pv[4])<<24 | uint64_t(pv[5])<<16 | uint64_t(pv[6])<<8 | uint64_t(pv[7]);
        }
        return v;
    }

    inline uint16_t betoh(uint16_t v) {
        if (kLittleEndian) {
            return htobe(v);
        }
        return v;
    }
    inline uint32_t betoh(uint32_t v) {
        if (kLittleEndian) {
            return htobe(v);
        }
        return v;
    }
    inline uint64_t betoh(uint64_t v) {
        if (kLittleEndian) {
            return htobe(v);
        }
        return v;
    }
}

#endif
