
//reference [1]:
//https://github.com/google/proto-quic/blob/master/src/third_party/zlib/google/compression_utils.cc

#include <stdio.h>
#include "gziphelper.h"

GzipHelper::GzipHelper(size_t compress_size, int compress_level, int memory_level)
    : 
      m_size(0),
      m_compress_size(compress_size),
      m_compress_level(compress_level),
      m_memory_level(memory_level)
{

}

GzipHelper::~GzipHelper() 
{

}

int GzipHelper::streamInit(z_stream* stream)
{
    int err = deflateInit2(stream, 
                            m_compress_level, 
                            Z_DEFLATED, 
                            MAX_WBITS + kWindowBitsToGetGzipHeader, 
                            m_memory_level, 
                            Z_DEFAULT_STRATEGY);
    if (err != Z_OK) {
        fprintf(stderr, "streamInit err=%d\n", err);
        return err;
    }
    return err;
}

int GzipHelper::compressInit()
{
    memset(&m_stream, 0, sizeof(m_stream));
    m_stream.zalloc = static_cast<alloc_func>(0);
    m_stream.zfree = static_cast<free_func>(0);
    m_stream.opaque = static_cast<voidpf>(0);

    // int err = deflateInit2_(&m_stream,
    //                         Z_DEFAULT_COMPRESSION,
    //                         Z_DEFLATED,
    //                         MAX_WBITS + kWindowBitsToGetGzipHeader,
    //                         kZlibCompressLevel,
    //                         Z_DEFAULT_STRATEGY,
    //                         ZLIB_VERSION,
    //                         sizeof(z_stream));

    streamInit(&m_stream);

    // m_data.reserve(compressBound(m_compress_size) + 
    //                 kWindowBitsToGetGzipHeader + 
    //                 kSafeThreshold);    
    m_data.reserve((m_compress_size) + 
                    kWindowBitsToGetGzipHeader + 
                    kSafeThreshold);
    m_size = 0;
    return 0;
}

int GzipHelper::compressReset()
{
    m_size = 0;
    deflateReset(&m_stream);
    return streamInit(&m_stream);
}

int GzipHelper::compressUpdate(const char* source, uint32_t source_length)
{
    m_stream.next_in = (Bytef*)(source);
    m_stream.avail_in = static_cast<uInt>(source_length);
    m_stream.next_out = &m_data.front() + m_size;
    m_stream.avail_out = static_cast<uInt>(m_data.capacity() - m_size);

    int err = deflate(&m_stream, Z_NO_FLUSH);
    if (err != Z_OK) {
        fprintf(stderr, "compressUpdate deflate  err=%d\n", err);
        return err;
    }
    m_size = m_stream.total_out;
    return 0;
}

int GzipHelper::compressFinish(const char* source, uint32_t source_length)
{
    int err;
    m_stream.next_in = (Bytef*)(source);
    m_stream.avail_in = static_cast<uInt>(source_length);
    m_stream.next_out = &m_data.front() + m_size;
    m_stream.avail_out = static_cast<uInt>(m_data.capacity() - m_size);

    gz_header gzip_header;
    memset(&gzip_header, 0, sizeof(gzip_header));
    err = deflateSetHeader(&m_stream, &gzip_header);
    if (err != Z_OK) {
        fprintf(stderr, "deflateSetHeader  err=%d\n", err);
        return err;
    }

    err = deflate(&m_stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        deflateEnd(&m_stream);
        fprintf(stderr, "deflate  err=%d\n", err);
        return err == Z_OK ? Z_BUF_ERROR : err;
    }
    err = deflateEnd(&m_stream);
    if (err != Z_OK) {
        fprintf(stderr, "deflateEnd  err=%d\n", err);
        return err;
    }
    //m_data.resize(m_stream.total_out);
    m_size = m_stream.total_out;
    return 0;
}

int GzipHelper::compressFinish()
{
    int err;
    m_stream.next_in = (Bytef*)"";
    m_stream.avail_in = static_cast<uInt>(0);
    m_stream.next_out = &m_data.front() + m_size;
    m_stream.avail_out = static_cast<uInt>(m_data.capacity() - m_size);

    gz_header gzip_header;
    memset(&gzip_header, 0, sizeof(gzip_header));
    err = deflateSetHeader(&m_stream, &gzip_header);
    if (err != Z_OK) {
        fprintf(stderr, "deflateSetHeader  err=%d\n", err);
        return err;
    }

    err = deflate(&m_stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        fprintf(stderr, "deflate  err=%d\n", err);
        deflateEnd(&m_stream);
        return err == Z_OK ? Z_BUF_ERROR : err;
    }
    err = deflateEnd(&m_stream);
    if (err != Z_OK) {
        fprintf(stderr, "deflateEnd  err=%d\n", err);
        return err;
    }
    //m_data.resize(m_stream.total_out);
    m_size = m_stream.total_out;
    return 0;
}

size_t GzipHelper::getCompressSize() 
{
    return m_size;
}

size_t GzipHelper::getCompressCapacity() 
{
    return m_data.capacity();
}

size_t GzipHelper::getCompressAvail() 
{
    return m_data.capacity() - m_size - kWindowBitsToGetGzipHeader - kSafeThreshold; 
}

bool GzipHelper::isFull()
{   
    size_t window_size =  32 << 10;
    if (m_size + kSafeThreshold + kWindowBitsToGetGzipHeader +  window_size >  m_data.capacity()) {
        return true;
    } else {
        return false;
    }
}

int GzipHelper::dumpCompressFile(const char* path) 
{
    int fd = open(path, O_CREAT | O_WRONLY | O_CLOEXEC, 0666);
    if (fd < 0) {
        printf("%s\n", "error !");
        return -1;
    }
    write(fd, &m_data.front(), m_size);
    //fdatasync(fd);
    close(fd);
    return 0;
}
