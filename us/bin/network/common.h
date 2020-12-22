#ifndef __COMMON_TYPES_H__
#define __COMMON_TYPES_H__

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <iostream>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include <twz/driver/nic.h>
#include <twz/driver/queue.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/queue.h>

typedef enum { LITTLEENDIAN, BIGENDIAN } endianess_t;

#define SHUTDOWN_WRITE 1
#define SHUTDOWN_READ 2

#define MAX_INTERFACE_NAME_SIZE 248
#define MAC_ADDR_SIZE 6 // bytes
#define IP_ADDR_SIZE 4 // bytes
#define OBJECT_ID_SIZE 16 // bytes

// error codes
#define EMAXFRAMESIZE 1
#define EARPFAILED 2
#define ETCPCONNFAILED 3

#define DEFAULT_IP "0.0.0.0"

typedef struct __attribute__((__packed__)) mac_addr {
	uint8_t mac[MAC_ADDR_SIZE];
} mac_addr_t;

typedef struct __attribute__((__packed__)) ip_addr {
	uint8_t ip[IP_ADDR_SIZE];
} ip_addr_t;

typedef struct __attribute__((__packed__)) object_id {
	uint8_t id[OBJECT_ID_SIZE];
} object_id_t;

typedef struct remote_info {
	mac_addr_t remote_mac;
	object_id_t object_id;
	uint8_t twz_op;
	ip_addr_t remote_ip;
	uint16_t remote_port;
	uint16_t ip_payload_size;
	char payload[1500];
	uint16_t payload_size;
} remote_info_t;

endianess_t check_machine_endianess();

uint32_t ntohl(uint32_t n);

uint16_t ntohs(uint16_t n);

uint32_t htonl(uint32_t n);

uint16_t htons(uint16_t n);

bool compare_mac_addr(mac_addr_t their_mac, mac_addr_t my_mac);

bool compare_ip_addr(ip_addr_t their_ip, ip_addr_t my_ip, ip_addr_t bcast_ip);

mac_addr_t string_to_mac_addr(char *mac_addr);

ip_addr_t string_to_ip_addr(char *ip_addr);

uint16_t finish_partial_checksum(uint32_t sum);
uint32_t partial_checksum(uint32_t, unsigned char *data, uint16_t len, bool);

uint16_t checksum(unsigned char *data, uint16_t len);

uint64_t get_id();

void *allocate_packet_buffer_object(uint16_t pkt_size);

void free_packet_buffer_object(twzobj *queue_obj);

#endif
