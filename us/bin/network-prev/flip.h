#include <string>

#include<twz/obj.h>

#include "intr_props.h"

/*NETWORK LAYER*/

/* L3-meta-header: indicates which indicates which header fields are present in the packet
 * one-byte pieces that have continuation bits (1st bit of each byte)
 * 1st byte: continuation bit, ESP (extra simsple packet), Version, Destination (2 bits), Type (changed from 1 byte to 2!!), TTL, Flow
 * 2nd byte: continuation bit, source(2 bits), length, checksum, don't fragment, reserved bit
 * 3rd byte: continuation bit, fragment offset, last fragment, 5 bits of reserved bits*/

//FLIP constants
#define META_HDR_SIZE 1
#define VERSIZON_SIZE 1
#define ADDR_SIZE_2_BYTES 2
#define ADDR_SIZE_4_BYTES 4
#define ADDR_SIZE_16_BYTES 16
#define TYPE_SIZE 2
#define TTL_SIZE 1
#define FLOW_SIZE 4
#define LENGTH_SIZE 2
#define CHECKSUM_SIZE 2
#define CURRENT_VERSION 0

/*Functions for Manipulating IP address*/
uint32_t ipv4_char_to_int(char *ip_addr);
void ipv4_int_to_char(uint32_t ip_addr, char *ip_addr_str);

/*Type should not be 0x2020 but a different type, for ESP packets*/
void flip_send_esp_14bit_data();
void flip_send_esp_6bit_data();

void flip_recv_esp_14bit_data();
void flip_recv_esp_6bit_data();


/*this function will need to be modified
 for ex destination MAC will need to be discovered with ARP protocol*/
void flip_send(uint8_t meta1, uint8_t meta2, uint8_t meta3, uint16_t type, bool broadcast_pkt, char *dst_ip, char *data, twzobj *interface_obj, twzobj *tx_queue_obj);
void flip_recv(twzobj *interface_obj, void *pkt_ptr, mac_addr_t src_mac);

