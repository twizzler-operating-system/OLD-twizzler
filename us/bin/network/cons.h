#define SIZE_OF_ETH_HDR_EXCLUDING_PAYLOAD 14 //does not include FCS
#define MAC_ADDR_SIZE 6
#define MAX_INTERFACE_NAME_SIZE 248
#define MAX_IPV4_CHAR_SIZE 17 //16 + \0

#define PACKET_BUFFER_MAX 1514 //this only includes src/des mac + type + max payload
#define MAX_L2_PAYLOAD 1500
#define MIN_L2_PAYLOAD 46
