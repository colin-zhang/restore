#ifndef PCAP_H
#define PCAP_H

#include <stdint.h>
#include <time.h>
#include <sys/time.h>

#include <string>
#include <vector>

#define PCAP_SNAPLEN_DEFAULT 65535

struct PcapFileHeader
{
    uint32_t magic_number;  /* magic number */
    uint16_t version_major; /* major version number */
    uint16_t version_minor; /* minor version number */
    int32_t  thiszone;      /* GMT to local correction */
    uint32_t sigfigs;       /* accuracy of timestamps */
    uint32_t snaplen;       /* max length of captured packets, in octets */
    uint32_t network;       /* data link type */
}  __attribute__((__packed__));

struct PcapPacketHeader
{
    uint32_t timestamp;
    uint32_t microseconds;
    uint32_t packet_length;
    uint32_t packet_length_wire;
};

struct PcapPacket
{
    std::vector<uint8_t> data;
    struct timeval tv;
    uint32_t scr_ipv4;
    uint32_t dst_ipv4;
    uint16_t scr_port;
    uint16_t dst_port;    
    uint16_t l3_type;
    uint16_t l2_type;
};


static inline size_t Hash4Tuple(PcapPacket& packet)
{
    size_t key = ((size_t)(packet.scr_ipv4) * 59) ^ 
                 ((size_t)(packet.dst_ipv4)) ^ 
                 ((size_t)(packet.scr_port) << 16) ^ 
                 ((size_t)(packet.dst_port));
    return key;
}

inline void TimeStr(uint32_t timestamp, uint32_t microseconds, char* str)
{
    struct tm tm_thiz;
    time_t t = timestamp;
    struct tm* p = localtime_r(&t, &tm_thiz); 
    if (p == nullptr) {
        printf("%s, timestamp=%u\n", "error", timestamp);
        return;
    } 
    snprintf(str, 32,
            "%04d-%02d-%02d "
            "%02d:%02d:%02d.%u",
            1900 + p->tm_year, 
            1 + p->tm_mon, 
            p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec,
            microseconds / 1000);
}

static inline void PrintPcapFileHeader(PcapFileHeader* pfh)
{
    printf("magic_number=%x\n" 
           "version_major=%x\n" 
           "version_minor=%x\n" 
           "thiszone=%x\n" 
           "sigfigs=%x\n" 
           "snaplen=%u\n" 
           "network=%u\n" 
            "\n",
            pfh->magic_number,
            pfh->version_major,
            pfh->version_minor,
            pfh->thiszone,
            pfh->sigfigs,
            pfh->snaplen,
            pfh->network
        );
}

static inline void PrintPcapPacketHeader(PcapPacketHeader* pph)
{
    char buf[128] = {"x"};
    TimeStr(pph->timestamp, pph->microseconds, buf);
    printf("time:%s\n"
            "packet_length=%u\n"
            "packet_length_wire=%u\n",
            buf,
            pph->packet_length,
            pph->packet_length_wire
          );
}

#define IP_FORMAT(addr) "%d.%d.%d.%d",(uint32_t)addr>>24, \
                    (uint32_t)((addr>>16) & 0x000000FF),\
                    (uint32_t)((addr>>8)  & 0x000000FF),\
                     (uint32_t)(addr& 0x000000FF)

static inline void PrintPcapPacket(PcapPacket* packet)
{
    printf("src ip=" IP_FORMAT(packet->scr_ipv4));
    printf("\n");    
    printf("dst ip=" IP_FORMAT(packet->dst_ipv4));
    printf("\n");
    printf("src port = %d, dst port= %d \n", packet->scr_port, packet->dst_port);
}

// void PcapHeaderInit(struct pcap_header * header, unsigned int snaplen) {
//   header->magic_number = 0xa1b2c3d4;
//   header->version_major = 0x0002;
//   header->version_minor = 0x0004;
//   header->thiszone = 0;
//   header->sigfigs = 0;
//   header->snaplen = snaplen;
//   header->network = 0x00000001;
// }

typedef std::vector<PcapPacket> PcapPacketVector;

class PcapReader
{
public:
    PcapReader(uint8_t group_num);
    ~PcapReader();

    int ReadPcapFile(std::string file_path);
    int ParsePacket(PcapPacket& packet, uint8_t* start, size_t len);

    PcapPacketVector& GetPcapPacketVector(int id);

    void PrintInfo();
private:
    std::vector<std::string> files_;
    uint8_t group_num_;
    std::vector<PcapPacketVector> datas_;
};


#endif