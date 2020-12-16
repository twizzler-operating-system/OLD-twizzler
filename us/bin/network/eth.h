#ifndef __ETH_H__
#define __ETH_H__

#include "common.h"

#define ETH_HDR_SIZE 14           // bytes
#define MAX_ETH_PAYLOAD_SIZE 1500 // bytes
#define MAX_ETH_FRAME_SIZE (ETH_HDR_SIZE + MAX_ETH_PAYLOAD_SIZE)

// ETH types
#define IPV4 0x0800
#define ARP 0x0806
#define TWIZZLER 0x0700

typedef struct __attribute__((__packed__)) eth_hdr {
	mac_addr_t dst_mac;
	mac_addr_t src_mac;
	uint16_t type;
} eth_hdr_t;

void eth_tx(const char *interface_name,
  mac_addr_t dst_mac,
  uint16_t eth_type,
  void *eth_pkt_ptr,
  uint16_t eth_pkt_size);

void eth_tx_2(const char *interface_name,
  mac_addr_t dst_mac,
  uint16_t eth_type,
  void *eth_pkt_ptr,
  uint16_t eth_pkt_size,
  void *,
  uint16_t);

void eth_rx(const char *interface_name);

#endif
