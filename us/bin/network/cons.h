#define SIZE_OF_ETH_HDR_EXCLUDING_PAYLOAD 14 //does not include FCS
#define MAC_ADDR_SIZE 6
#define MAX_INTERFACE_NAME_SIZE 248
#define MAX_IPV4_CHAR_SIZE 16

#define PACKET_BUFFER_MAX 1514 //this only includes src/des mac + type + max payload
#define MAX_L2_PAYLOAD 1500
#define MIN_L2_PAYLOAD 46

//FLIP constants
#define META_HDR_SIZE 1
#define VERSIZON_SIZE 1
#define ADDR_SIZE_2_BYTES 2
#define ADDR_SIZE_4_BYTES 4
#define ADDR_SIZE_16_BYTES 16
#define TYPE_SIZE 1
#define TTL_SIZE 1
#define FLOW_SIZE 4
#define LENGTH_SIZE 2
#define CHECKSUM_SIZE 2

