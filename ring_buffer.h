#ifndef LOGGER_RING_BUFFER_H_
#define LOGGER_RING_BUFFER_H_

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define ATOM_CAS(ptr, oval, nval) __sync_bool_compare_and_swap(ptr, oval, nval)
#define ATOM_CAS_POINTER(ptr, oval, nval) __sync_bool_compare_and_swap(ptr, oval, nval)
#define ATOM_INC(ptr) __sync_add_and_fetch(ptr, 1)
#define ATOM_FINC(ptr) __sync_fetch_and_add(ptr, 1)
#define ATOM_DEC(ptr) __sync_sub_and_fetch(ptr, 1)
#define ATOM_FDEC(ptr) __sync_fetch_and_sub(ptr, 1)
#define ATOM_ADD(ptr,n) __sync_add_and_fetch(ptr, n)
#define ATOM_SUB(ptr,n) __sync_sub_and_fetch(ptr, n)
#define ATOM_AND(ptr,n) __sync_and_and_fetch(ptr, n)


static inline bool
is_power_of_2(uint32_t n)
{
    return (n != 0 && ((n & (n - 1)) == 0));
}

static inline uint32_t
roundup_power_of_2(uint32_t a)
{
    int i;
    if (is_power_of_2(a))
        return a;

    uint32_t position = 0;
    for (i = a; i != 0; i >>= 1)
        position++;

    return (uint32_t)(1 << position);
}

template <typename T>
class RingBuff
{
public:
    RingBuff(uint32_t size)
      : 
        wd_index_(0),
        rd_index_(0)
    {
        size_ = roundup_power_of_2(size);
        data_ = new T[size_];
    };

    ~RingBuff() {
        delete[] data_;
    }

    int Put(T* item) {
        if (wd_index_ >= rd_index_ + size_) {
            printf("wd_index_=%u, rd_index_=%u, size_ = %u, hsize_ = %u\n", wd_index_, rd_index_, size_, (size_ >> 1));
            return -1;
        } else {
            uint32_t wd_idx = ATOM_FINC(&wd_index_);
            uint32_t idx = wd_idx & (size_ - 1);
            memcpy(&data_[idx], item, sizeof(T));
            return 0;
        }
    }

    int GetHalf(T* item) {
        int32_t half_size = (size_ >> 1);
        if (wd_index_ >= rd_index_ + half_size) {
            uint32_t idx = rd_index_ & (size_ - 1);
            memcpy(item, &data_[idx], sizeof(T) * half_size);
            ATOM_ADD(&rd_index_, half_size);
            printf("get half, read_index=%u, write_index=%u \n", rd_index_, wd_index_);
            printf("\twrite_index=%u \n", ATOM_ADD(&wd_index_, 0));
            return 1;
        } else {
            return 0;
        }
    }

    int GetAll(T* item) {
        uint32_t size = wd_index_ - rd_index_;
        uint32_t idx = rd_index_ & (size_ - 1);
        memcpy(item, &data_[idx], sizeof(T) * size_);
        printf("idx=%u, size_=%u sizeof(T)=%u\n", idx, size_, sizeof(T));
        rd_index_ = wd_index_;
        return size;
    }

    uint32_t Size() {
        return size_;
    }

    void Print() {
        printf("wd_index_=%u, rd_index_=%u, size_ = %u\n", wd_index_, rd_index_, size_);
    }

private:
    mutable uint32_t wd_index_;
    mutable uint32_t rd_index_;
    uint32_t size_;
    T* data_;
};

#endif
