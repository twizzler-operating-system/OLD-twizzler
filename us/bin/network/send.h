#ifndef __SEND_H__
#define __SEND_H__

#include "common.h"

#define ARP_TIMEOUT 1000000 //in us

#define ARP_TIMEOUT_ERROR 1

void send_arp_packet(const char* interface_name,
                   uint16_t opcode,
                   mac_addr_t dst_mac,
                   ip_addr_t dst_ip);

int send_ipv4_packet(const char* interface_name,
                    ip_addr_t dst_ip,
                    uint8_t ip_type,
                    char* payload);

#endif
