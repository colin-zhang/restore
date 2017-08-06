#ifndef FILE_READER_H_
#define FILE_READER_H_ 

#include <string>

struct Mmap
{
    int fd;
    void* data;
    size_t length;
};

class FileReader
{
public:
    FileReader(std::string file_name);
    ~FileReader();
    int IsOK();
public:
    Mmap* buff;
};

#endif