/*NETWORK LAYER*/

/* L3-meta-header: indicates which indicates which header fields are present in the packet
 * one-byte pieces that have continuation bits (1st bit of each byte)
 * 1st byte: continuation bit, ESP (extra simsple packet), Version, Destination (2 bits), Type, TTL, Flow
 * 2nd byte: continuation bit, source(2 bits), length, checksum, don't fragment, reserved bit
 * 3rd byte: continuation bit, fragment offset, last fragment, 5 bits of reserved bits*/

#pragma pack(push,1)

typedef struct flip_l3_metahdr_
{
    uint8_t meta_hdr;
}flip_l3_metahdr_t;

typedef struct flip_l3_hdr1_
{
    uint8_t version; //1 byte, higher 4 bits are version, lower 4 bits are priority. 0 assumed for both
    uint32_t destination; //right now set for 32 bits (ipv4 addres) FUTURE WORK: this must vary based on metaheader specification
    uint8_t type; //protocol type
    uint8_t ttl;
    uint32_t flow; //flow identification for QoS
    
}flip_l3_hdr1_t;

typedef struct flip_l3_hdr2_
{
    uint32_t source; //right now set for 32 bits (ipv4 addres) FUTURE WORK: this must vary based on metaheader specification
    uint16_t length; //max packet size if 64KBytes
    uint16_t checksum; //check correctness of packet payload... calculated simular to IP Checksum
}flip_l3_hdr2_t;

typedef struct flip_l3_hdr3_
{
    uint16_t fragment_offset; //indicates fragment offset with respect to original packet
}flip_l3_hdr3_t;

typedef struct flip_esp_14_bit_data
{
    usigned char data;
}flip_esp_14_bit_data;


#pragma pack (pop)


