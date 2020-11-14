#ifndef __UDP_H__
#define __UDP_H__

#include "common.h"

#define UDP_HDR_SIZE 8 //bytes

typedef struct __attribute__((__packed__)) udp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t len;
    uint16_t checksum;
} udp_hdr_t;

void udp_tx(uint16_t src_port,
            uint16_t dst_port,
            void* udp_pkt_ptr,
            uint16_t udp_pkt_size);

void udp_rx(const char* interface_name,
            remote_info_t* remote_info,
            void* udp_pkt_ptr);

#endif
