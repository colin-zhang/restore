#include "logger.h"

#ifdef __MACH__
#include <mach/mach_time.h>
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 0
int clock_gettime(int clk_id, struct timespec *t){
    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);
    uint64_t time;
    time = mach_absolute_time();
    double nseconds = ((double)time * (double)timebase.numer)/((double)timebase.denom);
    double seconds = ((double)time * (double)timebase.numer)/((double)timebase.denom * 1e9);
    t->tv_sec = seconds;
    t->tv_nsec = nseconds;
    return 0;
}
#else
#include <time.h>
#endif

#include "util.h"
#include "rwlock.h"

BusinessLogger::BusinessLogger()
  : 
    m_rotate_size(0),
    m_rotate_cycle(0),
    m_compress_type(0),
    m_start_time(0),
    m_roate_cnt(0)
{
    LOCK_INIT(&m_mutex, NULL);
}

BusinessLogger::~BusinessLogger()
{
    LOCK_DESTROY(&m_mutex);
}

void BusinessLogger::cleanLogger()
{
    LOCK_LOCK(&m_mutex);
    m_data.clear();
    LOCK_UNLOCK(&m_mutex);
}

static inline uint32_t getTimeUpNow() {
    struct timespec tpstart;
    clock_gettime(CLOCK_MONOTONIC, &tpstart);
    return tpstart.tv_sec;
}

//-----------------------------------------------------------
//---
//-----------------------------------------------------------

BasicBusinessLogger::BasicBusinessLogger()
  : 
    m_uptimeBak(0),
    m_serial_cnt(0)
{
    m_data.reserve(2*kVectorThreshold);
}

BasicBusinessLogger::~BasicBusinessLogger()
{
    m_uptimeBak = 0;
    checkRotate();
}

const char* BasicBusinessLogger::name()
{
    return "basic_logger";
}

void BasicBusinessLogger::clear()
{
    cleanLogger();
    if (m_compress_type == kCompressGzip) {
        m_gipHelper->compressReset();
    } else {
        m_buf.clear();
    }
}

int BasicBusinessLogger::init(const char* file_path, 
                        uint32_t rotate_size, 
                        uint32_t rotate_cycle,
                        uint8_t compress_type)
{
    m_file_path = file_path;
    m_rotate_size = rotate_size;
    m_rotate_cycle = rotate_cycle;
    m_compress_type = compress_type;
    m_uptimeBak = getTimeUpNow() + 10;

    m_start_time = getTimeUpNow();
    if (m_compress_type == kCompressGzip) {
        m_gipHelper = new GzipHelper(m_rotate_size + (16<<10));
        m_gipHelper->compressInit();
    }

#if 0
    printf("RequestLogger::init \n");
    printf("m_file_path = %s\n", m_file_path.c_str());
    printf("m_rotate_size = %d\n", m_rotate_size);
    printf("m_rotate_cycle = %d\n", m_rotate_cycle);
    printf("m_compress_type = %d\n", m_compress_type);
#endif
    return 0;
}

int BasicBusinessLogger::push_back(PcapPacket* members)
{
     LOCK_LOCK(&m_mutex);
     m_data.push_back(*members);
     LOCK_UNLOCK(&m_mutex);
     return 0;
}

void BasicBusinessLogger::getFileGenTime()
{
    char temptime2[32] = {0};
    time_t now;
    struct tm thiz_tm;
    time(&now);
    localtime_r(&now, &thiz_tm);
    //2017-06-12 16:32:57
    strftime(temptime2, 20, "%Y%m%d%H%M%S", &thiz_tm);
    m_fileGenTime = std::string(temptime2);
}

int BasicBusinessLogger::makeCsvLog(PcapPacket& packet)
{
    std::string line_log;
    int ret;
    char buff[1024];

    snprintf(buff, sizeof buff, "src ip=" IP_FORMAT(packet.scr_ipv4));
    line_log.append(buff);
    line_log.append(", ");
 
    snprintf(buff, sizeof buff, "dst ip=" IP_FORMAT(packet.dst_ipv4));
    line_log.append(buff);
    line_log.append(", ");

    snprintf(buff, sizeof buff, "src port = %d, dst port= %d \n", packet.scr_port, packet.dst_port);
    line_log.append(buff);

    if (m_compress_type == kCompressGzip) {
        ret = m_gipHelper->compressUpdate(line_log.c_str(), line_log.size());
        if (ret != 0) {
            printf("compressUpdate ERROR!!!!!");
        }
    } else {
        m_buf.append(line_log);
    }

    return 0;
}

int BasicBusinessLogger::checkRotate()
{
    std::vector<PcapPacket> data;
    std::vector<PcapPacket>::iterator it;
    bool isTimeOut = false;
    bool ifOutPutFile = false;

    //if (getTimeUpNow() -  >= m_uptimeBak + m_rotate_cycle) {
    if (getTimeUpNow() - m_start_time > (m_roate_cnt * m_rotate_cycle)) {
        isTimeOut = true;
        m_uptimeBak = getTimeUpNow();
        m_roate_cnt++;
    }

    if (isTimeOut || m_data.size() >= kVectorThreshold) {
        data.reserve(2*kVectorThreshold);
        //AutoTimer at(0, "checkRotate swap");
        AutoTimer* at = new AutoTimer(0, "checkRotate swap", isTimeOut ? "time out" : "kVectorThreshold");
        LOCK_LOCK(&m_mutex);
        data.swap(m_data);
        LOCK_UNLOCK(&m_mutex);
        delete at;
        return 1;
    } else {
        return 0;
    }

    m_serial_cnt = 0;
    getFileGenTime();

    for (it = data.begin(); it != data.end(); it++) {
        if (m_compress_type == kCompressGzip) {
            if (m_gipHelper->isFull()) {
                ifOutPutFile = true;
            }
        } else {
            if (m_buf.size() > m_rotate_size) {
                ifOutPutFile = true;
            } 
        }

        if (ifOutPutFile) {
            outputFile();
            ifOutPutFile = false;
            m_serial_cnt++;
        }
        makeCsvLog(*it);
    }

    if (isTimeOut && m_serial_cnt == 0) {
        outputFile();
    }

    return 1;
}

int BasicBusinessLogger::outputFile()
{

    std::string outputFilePath = Util::FormatStr(
                                      "%s/%s%05d.txt", 
                                      m_file_path.c_str(), 
                                      m_fileGenTime.c_str(), 
                                      m_serial_cnt
                                     );
    std::string outputFilePathTmp = "";

    if (m_compress_type == kCompressGzip) {
        outputFilePath += ".gz";
        outputFilePathTmp = outputFilePath + ".tmptmp";
        m_gipHelper->compressFinish();
        m_gipHelper->dumpCompressFile(outputFilePathTmp.c_str());
        m_gipHelper->compressReset();
        ::rename(outputFilePathTmp.c_str(), outputFilePath.c_str());
    } else {
        outputFilePathTmp = outputFilePath + ".tmptmp";
        FILE* fp = fopen(outputFilePathTmp.c_str(), "wb");
        if (fp) {
            fwrite(m_buf.c_str(), m_buf.size(), 1, fp);
            fflush(fp);
            //fdatasync(fileno(fp));
            fclose(fp);
            ::rename(outputFilePathTmp.c_str(), outputFilePath.c_str());
        } else {
            printf("%s\n", "error");
        }
        m_buf.clear();
    }

    return 0;
}
