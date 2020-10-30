#ifndef __SEND_H__
#define __SEND_H__

#include "common.h"

#define ARP_TIMEOUT 1000000 //in us

//error codes
#define EMAX_FRAME_SIZE 1
#define EARP_WAIT 2

void send_arp_packet(const char* interface_name,
                     uint16_t opcode,
                     mac_addr_t dst_mac,
                     ip_addr_t dst_ip);

int send_udp_packet(const char* interface_name,
                    object_id_t object_id,
                    uint8_t twz_op,
                    ip_addr_t dst_ip,
                    uint16_t src_port,
                    uint16_t dst_port,
                    char* payload);

#endif
