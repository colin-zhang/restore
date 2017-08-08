#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>

#include "pcap.h"
#include "packet.h"
#include "endian.h"
#include "file_reader.h"

PcapReader::PcapReader(uint8_t group_num)
 : group_num_(group_num)
{
    datas_.reserve(group_num_);
    datas_.resize(group_num_);
    printf("datas_.size = %lu\n", datas_.size());
}

PcapReader::~PcapReader()
{

}

int PcapReader::ParsePacket(PcapPacket& packet, uint8_t* start, size_t len)
{
    ether_hdr* ether_header = reinterpret_cast<ether_hdr*>(start);
    uint16_t l2_type = ntoh16(ether_header->ether_type);

    packet.l2_type = l2_type;
    if (ETHER_TYPE_IPv4 == l2_type) {
        //printf("%x:%x\n", l2_type, ETHER_TYPE_IPv4);
        ipv4_hdr* ipv4_header = reinterpret_cast<ipv4_hdr*>(start + sizeof(ether_hdr));
        //uint8_t* ip_payload = reinterpret_cast<uint8_t*>(start + sizeof(ether_hdr) + sizeof(ipv4_hdr));
        uint8_t* ip_payload = reinterpret_cast<uint8_t*>(start + sizeof(ether_hdr) + 
                                              ((ipv4_header->version_ihl & 0x0f) << 2));

        packet.l3_type = ipv4_header->next_proto_id;
        //packet.scr_ipv4 = ntoh32(ipv4_header->src_addr);
        packet.scr_ipv4 = ntoh32(ipv4_header->src_addr);
        packet.dst_ipv4 = ntoh32(ipv4_header->dst_addr);

        if (IPPROTO_TCP == ipv4_header->next_proto_id) {
            tcp_hdr* tcp_header = (tcp_hdr*)ip_payload;
            packet.scr_port = ntoh16(tcp_header->src_port);
            packet.dst_port = ntoh16(tcp_header->dst_port);

        } else if (IPPROTO_UDP == ipv4_header->next_proto_id) {
            udp_hdr* udp_header = (udp_hdr*)ip_payload;
            packet.scr_port = ntoh16(udp_header->src_port);
            packet.dst_port = ntoh16(udp_header->dst_port);
        } else {
            return 0;
        }

        //PrintPcapPacket(&packet);
        return 1;
    }
    return 0;
}

int PcapReader::ReadPcapFile(std::string file_path) 
{
    size_t offset = 0;
    //uint8_t* buf;
    FileReader file_reader(file_path);
    if (!file_reader.IsOK()) {
        printf("%s\n", "xxx");
        return -1;
    }

    PcapFileHeader pfh;
    uint8_t* p = (uint8_t*)file_reader.buff->data;
    memcpy((void*)&pfh, p, sizeof(pfh));
    offset += sizeof(pfh);
    PrintPcapFileHeader(&pfh);

    while (offset <= file_reader.buff->length) {
        PcapPacketHeader pph;
        PcapPacket packet;
        memcpy((void*)&pph, p + offset, sizeof(pph));
        packet.tv.tv_sec = pph.timestamp;
        packet.tv.tv_usec = pph.microseconds;
        //PrintPcapPacketHeader(&pph);
        assert(pph.packet_length == pph.packet_length_wire);
        offset += sizeof(pph);

        int ret = ParsePacket(packet, p + offset, pph.packet_length);
        if (ret) {
            size_t key = Hash4Tuple(packet);
            datas_[key % group_num_].push_back(packet);
        }
        offset += pph.packet_length;
    }

    PrintInfo();
    return 0;
}

void PcapReader::PrintInfo()
{
    std::vector<PcapPacketVector>::iterator it;
    size_t all = 0;
    printf("datas_.size = %lu\n", datas_.size());
    for (it = datas_.begin(); it != datas_.end(); it++) {
        printf("size = %lu\n", it->size());
        all += it->size();
    }
    printf("all = %lu\n", all);
}

PcapPacketVector& PcapReader::GetPcapPacketVector(int id)
{
    size_t i = (size_t)id % group_num_;
    return datas_[i];
}

// int PcapReader::ReadPcapFile(std::string file_path)
// {
//     size_t offset = 0;
//     size_t cnt = 0;
//     int ret = 0;
//     uint8_t buf[PCAP_SNAPLEN_DEFAULT * 4];

//     int fd = open(file_path.c_str(), O_RDONLY);
//     if (fd < 0) {
//         return -1;
//     }

//     PcapFileHeader* pfh;
//     PcapPacketHeader pph;
//     if ((ret = pread(fd, buf, sizeof(PcapFileHeader), offset)) == -1) {
//         return -1;
//     }

//     offset += sizeof(PcapFileHeader);
//     pfh = reinterpret_cast<PcapFileHeader*>(buf);
//     PrintPcapFileHeader(pfh);

//     do {
//         if ((ret = pread(fd, (void*)&pph, sizeof(pph), offset)) <= 0) {
//             printf("1, ret = %d, offset=%lu\n", ret, offset);
//             break;
//         }

//         PrintPcapPacketHeader(&pph);
//         printf("%s\n", " ");
//         offset += sizeof(PcapPacketHeader);
        
//         assert(pph.packet_length == pph.packet_length_wire);
//         if ((ret = pread(fd, buf, pph.packet_length, offset)) <= 0) {
//             printf("%s2\n", " ");
//             break;
//         }

//         printf("pph.packet_length = %u, offset=%lu\n", pph.packet_length, offset);
//         offset += pph.packet_length;
//         printf("pph.packet_length = %u, offset=%lu\n", pph.packet_length, offset);

//         ether_hdr* ether_header = reinterpret_cast<ether_hdr*>(buf);
//         //ether_header->ether_type;
//         printf("%x:%x\n", ether_header->ether_type, ETHER_TYPE_IPv4);


//         cnt++;
//     } while(1);

//     printf("cnt = %lu \n", cnt);
//     close(fd);

//     return 0;
// }
