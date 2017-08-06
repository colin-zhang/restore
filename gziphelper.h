//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string>
#include <vector>

#include "zlib.h"

class GzipHelper
{
public:
    //compress_size的大小为压缩类中, 压缩流m_data的大小, 
    //不支持动态增长
    //单位MB, 
    GzipHelper(size_t compress_size);
    GzipHelper(size_t compress_size, int compress_level, int memory_level);
    ~GzipHelper();
    
    //使用前首先初始化
    int compressInit();
    //下一个周期前reset
    int compressReset();
    
    int compressUpdate(const char* src, uint32_t src_len);

    //生成压缩文件头
    int compressFinish(const char* src, uint32_t src_len);
    int compressFinish();

    size_t getCompressSize();
    size_t getCompressCapacity();
    size_t getCompressAvail();

    bool isFull();
    
    //压缩流dump到文件
    int dumpCompressFile(const char* path);
public:
    //http://www.zlib.net/manual.html#Advanced
    static const size_t kGzipZlibHeaderDifferenceBytes = 16;
    static const int kWindowBitsToGetGzipHeader = 16;
    static const size_t kSafeThreshold = 8<<20;
    //压缩率 1-9, 9压缩率最高
    static const int kZlibCompressLevel = 1;
    //1-9, 9最高
    static const int kZlibMemoryLevel = 9;
public:
    std::vector<Bytef> m_data;
    size_t m_size;
    size_t m_compress_size;
    int m_compress_level;
    int m_memory_level;
private:
    z_stream m_stream;
};
