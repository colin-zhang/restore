#ifndef PACKET_H_
#define PACKET_H_

#include <stdint.h>
#include <netinet/in.h>

#define ETHER_ADDR_LEN 6

struct ether_addr {
    uint8_t addr_bytes[ETHER_ADDR_LEN]; 
} __attribute__((__packed__));

struct ether_hdr {
    struct ether_addr d_addr; 
    struct ether_addr s_addr; 
    uint16_t ether_type;      
} __attribute__((__packed__));

struct vlan_hdr {
     uint16_t vlan_tci; 
     uint16_t eth_proto;
} __attribute__((__packed__));

struct vxlan_hdr {
    uint32_t vx_flags; 
    uint32_t vx_vni;   
} __attribute__((__packed__));

struct ipv4_hdr {
    uint8_t  version_ihl;       /**< version and header length */
    uint8_t  type_of_service;   /**< type of service */
    uint16_t total_length;      /**< length of packet */
    uint16_t packet_id;     /**< packet ID */
    uint16_t fragment_offset;   /**< fragmentation offset */
    uint8_t  time_to_live;      /**< time to live */
    uint8_t  next_proto_id;     /**< protocol ID */
    uint16_t hdr_checksum;      /**< header checksum */
    uint32_t src_addr;      /**< source address */
    uint32_t dst_addr;      /**< destination address */
} __attribute__((__packed__));

/**
 * TCP Header
 */
struct tcp_hdr {
    uint16_t src_port;  /**< TCP source port. */
    uint16_t dst_port;  /**< TCP destination port. */
    uint32_t sent_seq;  /**< TX data sequence number. */
    uint32_t recv_ack;  /**< RX data acknowledgement sequence number. */
    uint8_t  data_off;  /**< Data offset. */
    uint8_t  tcp_flags; /**< TCP flags */
    uint16_t rx_win;    /**< RX flow control window. */
    uint16_t cksum;     /**< TCP checksum. */
    uint16_t tcp_urp;   /**< TCP urgent pointer, if any. */
} __attribute__((__packed__));

/**
 * UDP Header
 */
struct udp_hdr {
    uint16_t src_port;    /**< UDP source port. */
    uint16_t dst_port;    /**< UDP destination port. */
    uint16_t dgram_len;   /**< UDP datagram length */
    uint16_t dgram_cksum; /**< UDP datagram checksum */
} __attribute__((__packed__));

/* Ethernet frame types */
#define ETHER_TYPE_IPv4 0x0800 /**< IPv4 Protocol. */
#define ETHER_TYPE_IPv6 0x86DD /**< IPv6 Protocol. */
#define ETHER_TYPE_ARP  0x0806 /**< Arp Protocol. */
#define ETHER_TYPE_RARP 0x8035 /**< Reverse Arp Protocol. */
#define ETHER_TYPE_VLAN 0x8100 /**< IEEE 802.1Q VLAN tagging. */
#define ETHER_TYPE_1588 0x88F7 /**< IEEE 802.1AS 1588 Precise Time Protocol. */
#define ETHER_TYPE_SLOW 0x8809 /**< Slow protocols (LACP and Marker). */
#define ETHER_TYPE_TEB  0x6558 /**< Transparent Ethernet Bridging. */

#define ETHER_VXLAN_HLEN (sizeof(struct udp_hdr) + sizeof(struct vxlan_hdr))


#endif
