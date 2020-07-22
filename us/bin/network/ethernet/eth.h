//#include <string>

#pragma pack(push,1)
typedef struct mac_addr
{
	unsigned char mac[6];
}mac_addr_t;

typedef struct eth_hdr
{
    /*preamble (7 bytes) and SFD (1 byte) are used by physical layer, so we don't need to include them*/
    mac_addr_t dst_mac; //6 bytes
    mac_addr_t src_mac;
    unsigned short type; //aka type field... 2 bytes
    char payload[248];/*Allowed 46-1500 bytes*/ //this needs to me modified so data is allocated then freed every time we send data
    //unsigned int FCS; //CRC aka Frame Check Sequence
}eth_hdr_t;
#pragma pack(pop)
