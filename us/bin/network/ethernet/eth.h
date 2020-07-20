#pragma pack(push,1)
typedef struct mac_addr
{
	char mac[48];
}mac_addr_t;

typedef struct eth_hdr
{
    /*preamble (7 bytes) and SFD (1 byte) are used by physical layer, so we don't need to include them*/
    mac_addr_t dst_mac; //6 bytes
    mac_addr_t src_mac;
    unsigned short type; //aka type field... 2 bytes
    char payload[248] = "test"; /*Allowed 46-1500 bytest*/
    //unsigned int FCS; //CRC aka Frame Check Sequence
}eth_hdr_t;
#pragma pack(pop)
