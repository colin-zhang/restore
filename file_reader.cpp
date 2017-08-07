#include "file_reader.h"

#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/time.h>
#ifdef USE_MMAP
#include <sys/mman.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <iostream>
#include <string>

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

static Mmap* mmap_create(const char* filename, size_t offset, size_t size)
{
    Mmap* mp = nullptr;
    struct stat st;

    if (stat(filename, &st) < 0) {
        std::cout << "stat err,  " << filename << std::endl;
        return nullptr;
    }
    size = (offset + size) <= (size_t)st.st_size ? size : st.st_size - offset;


    mp = (Mmap*)malloc(sizeof(Mmap));
    if (nullptr == mp) {
        std::cout << "malloc err = "  << std::endl;
        return nullptr;
    }
#ifdef USE_MMAP
    mp->fd = open(filename, O_RDONLY);
    if (mp->fd > 0) {
        mp->length = size;
        mp->data = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, mp->fd, offset);
    }

    if (mp->data == nullptr || mp->data == MAP_FAILED) {
        close(mp->fd);
        free(mp);
        mp = nullptr;
        std::cout << "map err = "  << std::endl;
    }
#else 
    mp->fd = open(filename, O_RDONLY);
    if (mp->fd < 0) {
        free(mp);
        return nullptr;
    }
    mp->data = malloc(size);
    mp->length = size;
    read(mp->fd, mp->data, size);
    close(mp->fd);

#endif

    return mp;
}

static void mmap_destroy(Mmap* mp)
{
    if (mp != nullptr) {
    #ifdef USE_MMAP
        close(mp->fd);
        munmap(mp->data, mp->length);
        free(mp);
    #else
        free(mp->data);
        free(mp);
    #endif
        //printf("%s\n", "mmap_destroy");
    }
    return;
}


FileReader::FileReader(std::string file_name)
{
    uint64_t file_size = get_file_size(file_name.c_str());
    buff = mmap_create(file_name.c_str(), 0, file_size);
    if (buff != nullptr) {
        printf("length = %lu\n", buff->length);
    } else {
        printf("FileReader mmap_create error \n");
    }
}

FileReader::~FileReader()
{
    mmap_destroy(buff);
}

int FileReader::IsOK()
{
    if (buff == nullptr) {
        return 0;
    } else {
        return 1;
    }
}

