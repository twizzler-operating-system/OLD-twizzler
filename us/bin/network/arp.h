#ifndef __ARP_H__
#define __ARP_H__

#include "common.h"

#define ARP_HDR_SIZE 28 //bytes
#define HW_ADDR_SIZE 6 //bytes
#define PROTO_ADDR_SIZE 4 //bytes

#define ARP_REQUEST 0x0001
#define ARP_REPLY 0x0002

#define ARP_TIMEOUT 1000 //ms

typedef struct __attribute__((__packed__)) arp_hdr {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t hw_addr_len;
    uint8_t proto_addr_len;
    uint16_t opcode;
    uint8_t sender_hw_addr[HW_ADDR_SIZE];
    uint8_t sender_proto_addr[PROTO_ADDR_SIZE];
    uint8_t target_hw_addr[HW_ADDR_SIZE];
    uint8_t target_proto_addr[PROTO_ADDR_SIZE];
} arp_hdr_t;

void arp_table_put(uint8_t* proto_addr,
                   uint8_t* hw_addr);

uint8_t* arp_table_get(uint8_t* proto_addr);

void arp_table_delete(uint8_t* proto_addr);

void arp_table_view();

void arp_table_clear();

void insert_arp_req(uint8_t* proto_addr);

void delete_arp_req(uint8_t* proto_addr);

bool is_arp_req_inflight(uint8_t* proto_addr);

void view_arp_req_inflight();

void arp_tx(uint16_t hw_type,
            uint16_t proto_type,
            uint8_t hw_addr_len,
            uint8_t proto_addr_len,
            uint16_t opcode,
            uint8_t sender_hw_addr[HW_ADDR_SIZE],
            uint8_t sender_proto_addr[PROTO_ADDR_SIZE],
            uint8_t target_hw_addr[HW_ADDR_SIZE],
            uint8_t target_proto_addr[PROTO_ADDR_SIZE],
            void* arp_pkt_ptr);

void arp_rx(const char* interface_name,
            void* arp_pkt_ptr);

#endif
