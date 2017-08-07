#ifndef LOGGER_H_
#define LOGGER_H_

#include <stdint.h>
#include <string>
#include <vector>
#include <pthread.h>

#include "gziphelper.h"
#include "ring_buffer.h"
#include "pcap.h"
#include "rwlock.h"

#include "buffer_ring.h"

//ring buffer 主备切换的设计

#if 0
typedef pthread_mutex_t LockVar;
#define LOCK_INIT       pthread_mutex_init
#define LOCK_DESTROY    pthread_mutex_destroy
#define LOCK_LOCK       pthread_mutex_lock
#define LOCK_UNLOCK     pthread_mutex_unlock
#else
typedef RwLock LockVar;
#define LOCK_INIT       rwlock_init
#define LOCK_DESTROY    rwlock_deinit
#define LOCK_LOCK       rwlock_write_lock
#define LOCK_UNLOCK     rwlock_write_unlock
#endif

enum LoggerCompressType
{
    kCompressNone = 0,
    kCompressGzip = 1,
};


class BusinessLogger {
public:
    BusinessLogger();
    ~BusinessLogger();
    virtual const char* name() = 0;
    virtual int init(const char* file_path, 
                    uint32_t rotate_size, 
                    uint32_t rotate_cycle, 
                    uint8_t compress_type) = 0;

    virtual int  push_back(PcapPacket* members) = 0;

    virtual int  checkRotate() = 0;

    virtual int  outputFile() = 0;

    void cleanLogger();

protected:
    //std::vector<PcapPacket> m_data;
    BuffRing<PcapPacket>* m_data;
    uint32_t m_rotate_size;
    uint32_t m_rotate_cycle;
    uint8_t  m_compress_type;
    LockVar m_mutex;
    std::string m_file_path;
    uint32_t m_start_time;
    uint32_t m_roate_cnt;
};

class BasicBusinessLogger : public BusinessLogger
{
    static const uint32_t kVectorThreshold = 64 << 20; 
public:
    BasicBusinessLogger();
    ~BasicBusinessLogger();
    virtual const char* name();
    virtual int init(const char* file_path, 
                      uint32_t rotate_size, 
                      uint32_t rotate_cycle, 
                      uint8_t compress_type);
    virtual int push_back(PcapPacket* members);
    virtual int checkRotate();
    virtual int outputFile();
    void clear();
private:
    int  makeCsvLog(PcapPacket& members);

    void getFileGenTime(); 

private:
    std::string m_fileGenTime;
    std::string m_buf;
    GzipHelper* m_gipHelper;
    uint64_t m_uptimeBak;
    uint32_t m_serial_cnt;    
};


#endif

