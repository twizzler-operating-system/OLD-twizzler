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

typedef struct ip_table_entry {
    ip_addr_t dst_ip;
    ip_addr_t netmask;
    ip_addr_t gateway;
    char tx_interface_name[MAX_INTERFACE_NAME_SIZE];
} ip_table_entry_t;

void ip_table_put(ip_table_entry_t entry);

void ip_table_get(ip_addr_t dst_ip,
                  char* tx_interface_name);

void ip_table_delete(ip_table_entry_t entry);

void ip_table_view();

void ip_table_clear();

void ip_tx(const char* interface_name,
           ip_addr_t dst_ip,
           uint8_t ip_type,
           void* ip_pkt_ptr,
           uint16_t ip_pkt_size);

void ip_rx(const char* interface_name,
           remote_info_t* remote_info,
           void* ip_pkt_ptr);

#endif
