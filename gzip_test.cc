//
//  Created by Colin zhang on 2017/7/5
//  Copyright © 2017年 Colin zhang All rights reserved.
//

#include <stdint.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>

#include <iostream>
#include <string>

#include "gziphelper.h"

struct Mmap
{
    int fd;
    void* data;
    size_t length;
};

static int compress_level = 1;

Mmap* mmap_create(const char* filename, size_t offset, size_t size)
{
    Mmap* mp = NULL;
    struct stat st;

    if (stat(filename, &st) < 0) {
        std::cout << "stat err,  " << filename << std::endl;
        return NULL;
    }
    size = (offset + size) <= st.st_size ? size : st.st_size - offset;

    mp = (Mmap*)malloc(sizeof(Mmap));
    if (NULL == mp) {
        std::cout << "malloc err = "  << std::endl;
        return NULL;
    }

    mp->fd = open(filename, O_RDONLY);
    if (mp->fd > 0) {
        mp->length = size;
        mp->data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, mp->fd, offset);
    }

    if (mp->data == NULL || mp->data == MAP_FAILED) {
        close(mp->fd);
        free(mp);
        mp = NULL;
        std::cout << "map err = "  << std::endl;
    }

    return mp;
}

void mmap_destroy(Mmap* mp)
{
    if(mp != NULL)
    {
        close(mp->fd);
        munmap(mp->data, mp->length);
        free(mp);
    }
    return;
}

static ssize_t get_file_size(const char* path)
{
    ssize_t file_size = -1;
    struct stat statbuff;

    if (stat(path, &statbuff) < 0) {
        return file_size;
    } else {
        file_size = statbuff.st_size;
        return file_size;
    }
}

static inline uint64_t get_curr_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec *1000 + tv.tv_usec /1000;
}

uint64_t gzip_helper_test(std::string& file, Mmap* mm)
{
    std::string file_gz = file + ".gz";
    GzipHelper gzipHelper(1 << 30, compress_level, 9);
    gzipHelper.compressInit();
    uint64_t compress_size = 0;

    char* p = (char*) mm->data;
    uint64_t len = mm->length;
    size_t kEach = 64 << 20;
    int err;

    while (len > kEach) {
        err = gzipHelper.compressUpdate((const char*)p, kEach);
        if (err != 0) {
            std::cout << "err = " << err << std::endl;
        }
        p += kEach;
        len -= kEach;
    }

    if (len > 0) {
        err = gzipHelper.compressUpdate((const char*)p, len);
        if (err != 0) {
            std::cout << "err = " << err << std::endl;
        }
    }


    err = gzipHelper.compressFinish();
    if (err != 0) {
        std::cout << "err = " << err << std::endl;
    }
    compress_size = gzipHelper.getCompressSize();
    gzipHelper.dumpCompressFile(file_gz.c_str());

    return compress_size;
}

uint64_t diff_time(uint64_t a, uint64_t b)
{
    return b - a;
}

int main(int argc, char const *argv[])
{
    /* code */
    uint64_t start;
    uint64_t end;
    uint64_t compress_size = 0;
    std::string origin_file;

    if (argv[1] == NULL) {
        origin_file = "alldump__core00_000_0000.pcap.g";
    } else {
        origin_file =std::string(argv[1]);
    }

    if (argc == 3) {
        compress_level = atoi(argv[2]);
        if (compress_level > 9 || compress_level < 0) {
            std::cerr << " compress_level error \n";
            return -1;
        }
    }

    std::string gz_file = origin_file + ".1.gz";
    uint64_t file_size = get_file_size(origin_file.c_str());

    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(10, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) != 0) {
        std::cerr << "CPU affinity, error...\n";
    }


    Mmap* mm = mmap_create(origin_file.c_str(), 0, file_size);
    if (NULL == mm) {
        std::cerr << " mm == NULL " << std::endl;
        return -1;
    }

    char mode[32];
    snprintf(mode, sizeof mode, "wb%df", compress_level);
    start = get_curr_ms();
    gzFile gf = gzopen(gz_file.c_str(), mode);
    gzwrite(gf, mm->data, mm->length);
    gzclose(gf);
    end = get_curr_ms();

    compress_size = get_file_size(gz_file.c_str());
    printf( "gzip api: \n"
            "use %llu ms, origin size = %llu, compress_size = %llu "
            "rate = %f, "
            "speed = %f"
            "\n", 
            diff_time(start, end), 
            file_size,
            compress_size, 
            (double)compress_size / (double)file_size,
            (double)file_size / diff_time(start, end) / 1000
            );

    start = get_curr_ms();
    compress_size = gzip_helper_test(origin_file, mm);
    end = get_curr_ms();

    printf( "gzip help: \n"
            "use %llu ms, origin size = %llu, compress_size = %llu "
            "rate = %f, "
            "speed = %f"
            "\n", 
            diff_time(start, end), 
            file_size,
            compress_size, 
            (double)compress_size / (double)file_size,
            (double)file_size / diff_time(start, end) / 1000
            );

    mmap_destroy(mm);

    return 0;
}

