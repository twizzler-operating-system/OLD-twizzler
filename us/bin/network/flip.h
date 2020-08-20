#include <string>

#include<twz/obj.h>

#include "intr_props.h"

/*NETWORK LAYER*/

/* L3-meta-header: indicates which indicates which header fields are present in the packet
 * one-byte pieces that have continuation bits (1st bit of each byte)
 * 1st byte: continuation bit, ESP (extra simsple packet), Version, Destination (2 bits), Type, TTL, Flow
 * 2nd byte: continuation bit, source(2 bits), length, checksum, don't fragment, reserved bit
 * 3rd byte: continuation bit, fragment offset, last fragment, 5 bits of reserved bits*/

#pragma pack(push,1)

typedef struct flip_esp_14_bit_data
{
    unsigned char data;
}flip_esp_14_bit_data;

typedef struct flip_l3_hdr3_
{
    uint16_t fragment_offset; //indicates fragment offset with respect to original packet
    //payload must be added
}flip_l3_hdr3_t;

typedef struct flip_l3_hdr2_
{
    uint32_t source; //right now set for 32 bits (ipv4 addres) FUTURE WORK: this must vary based on metaheader specification
    uint16_t length; //max packet size if 64KBytes
    uint16_t checksum; //check correctness of packet payload... calculated simular to IP Checksum
    flip_l3_hdr3_t next_hdr;
    //payload must be added
}flip_l3_hdr2_t;

typedef struct flip_l3_hdr1_
{
    uint8_t version; //1 byte, higher 4 bits are version, lower 4 bits are priority. 0 assumed for both
    uint32_t destination; //right now set for 32 bits (ipv4 addres) FUTURE WORK: this must vary based on metaheader specification
    uint8_t type; //protocol type
    uint8_t ttl;
    uint32_t flow; //flow identification for QoS
    flip_l3_hdr2_t next_hdr;
    flip_esp_14_bit_data esp_data;
    
    
}flip_l3_hdr1_t;

typedef struct flip_l3_metahdr_
{
    //again, memory must be able to be dynamically allocated
    uint8_t meta_hdr1;
    uint8_t meta_hdr2;
    uint8_t meta_hdr3;
    flip_l3_hdr1_t next_hdr;
    //eventually add pointer to 14-bit esp
}flip_l3_metahdr_t;

//!! THESE DATA STRUCTURES NEED TO BE MODIFIED TO ALLOCATE MEMORY

#pragma pack (pop)



/*Type should not be 0x2020 but a different type, for ESP packets*/
void flip_send_esp_14bit_data();
void flip_send_esp_6bit_data();

void flip_recv_esp_14bit_data();
void flip_recv_esp_6bit_data();


/*this function will need to be modified
 for ex destination MAC will need to be discovered with ARP protocol*/
void flip_send(uint8_t meta1, uint8_t meta2, uint8_t meta3, uint8_t type, char *dst_ip, char *data, twzobj *interface_obj, twzobj *tx_queue_obj);
void flip_recv(twzobj *interface_obj, void *pkt_ptr);

