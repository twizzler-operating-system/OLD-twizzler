#include "eth.h"
#include "ipv4.h"
#include "arp.h"

//typedef struct ipv4_addr ipv4_addr_t;

typedef struct interface
{
    char name[MAX_INTERFACE_NAME_SIZE];

    mac_addr_t mac;

    bool is_ipv4_addr_set;
    ipv4_addr_t ipv4_addr;
    //add subnet mask
}interface_t;



void init_interface(char *name, char *info_queue, twzobj *interface_props);
