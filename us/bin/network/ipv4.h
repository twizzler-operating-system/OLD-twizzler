#ifndef __IPV4_H__
#define __IPV4_H__

#include "common.h"

#define IP_HDR_SIZE 20 //bytes

//IP types
#define ICMP 0x01
#define TCP 0x06
#define UDP 0x11

typedef struct __attribute__((__packed__)) ip_hdr {
    uint8_t ver_and_ihl; //version:4, IHL:4
    uint8_t tos; //dscp:6, ECN:2
    uint16_t tot_len;
    uint16_t identification;
    uint16_t flags_and_offset; //flags:3, fragment offset:13
    uint8_t ttl;
    uint8_t protocol;
    uint16_t hdr_checksum;
    ip_addr_t src_ip;
    ip_addr_t dst_ip;
} ip_hdr_t;

void ip_tx(const char* interface_name,
           ip_addr_t dst_ip,
           uint8_t ip_type,
           void* pkt_ptr,
           int pkt_size);

void ip_rx(const char* interface_name,
           remote_info_t* remote_info,
           void* pkt_ptr);

#endif
