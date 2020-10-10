#ifndef __COMMON_TYPES_H__
#define __COMMON_TYPES_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <map>
#include <thread>
#include <mutex>

#include <twz/name.h>
#include <twz/obj.h>
#include <twz/queue.h>
#include <twz/driver/nic.h>
#include <twz/driver/queue.h>

#define LITTLEENDIAN 1
#define BIGENDIAN 2

#define MAC_ADDR_SIZE 6 //bytes
#define IP_ADDR_SIZE 4 //bytes
#define OBJECT_ID_SIZE 16 //bytes

typedef struct __attribute__((__packed__)) mac_addr {
    uint8_t mac[MAC_ADDR_SIZE];
} mac_addr_t;

typedef struct __attribute__((__packed__)) ip_addr {
    uint8_t ip[IP_ADDR_SIZE];
} ip_addr_t;

typedef struct __attribute__((__packed__)) object_id {
    uint8_t id[OBJECT_ID_SIZE];
} object_id_t;

uint8_t check_machine_endianess();

uint32_t ntohl(uint32_t n);

uint16_t ntohs(uint16_t n);

uint32_t htonl(uint32_t n);

uint16_t htons(uint16_t n);

bool compare_mac_addr(mac_addr_t my_mac,
                    mac_addr_t their_mac);

ip_addr_t convert_ip_addr(char* ip_addr);

uint16_t checksum(unsigned char* data,
                int8_t len);

uint64_t get_id();

void* allocate_packet_buffer_object(int pkt_size);

void free_packet_buffer_object(twzobj* queue_obj);
#endif
