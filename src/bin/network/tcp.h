#ifndef __TCP_H__
#define __TCP_H__

#include "common.h"

#define TCP_HDR_SIZE 20       // bytes
#define PSEUDO_IP_HDR_SIZE 12 // bytes

typedef struct __attribute__((__packed__)) tcp_hdr {
	uint16_t src_port;
	uint16_t dst_port;
	uint32_t seq_num;
	uint32_t ack_num;
	uint16_t data_offset_and_flags;
	uint16_t window_size;
	uint16_t checksum;
	uint16_t urg_ptr;
} tcp_hdr_t;

typedef struct __attribute__((__packed__)) pseudo_ip_hdr {
	uint8_t src_ip[IP_ADDR_SIZE];
	uint8_t dst_ip[IP_ADDR_SIZE];
	uint8_t reserved;
	uint8_t protocol;
	uint16_t len;
} pseudo_ip_hdr_t;

void tcp_tx(ip_addr_t src_ip,
  ip_addr_t dst_ip,
  uint16_t src_port,
  uint16_t dst_port,
  uint32_t seq_num,
  uint32_t ack_num,
  uint8_t ns,
  uint8_t cwr,
  uint8_t ece,
  uint8_t urg,
  uint8_t ack,
  uint8_t psh,
  uint8_t rst,
  uint8_t syn,
  uint8_t fin,
  uint16_t window_size,
  uint16_t urg_ptr,
  void *tcp_pkt_ptr,
  uint16_t tcp_pkt_size,
  void *payload,
  uint16_t payload_sz);

void tcp_rx(const char *interface_name, remote_info_t *remote_info, void *tcp_pkt_ptr);

#endif
