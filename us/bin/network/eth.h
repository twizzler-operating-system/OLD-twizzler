#include <cstdlib>
#include <cstdio>
#include <vector>
#include <iostream>
#include <map>
#include <thread>

#include <unistd.h>
#include <string.h>

#include <twz/name.h>
#include <twz/obj.h>
#include <twz/queue.h>
#include <twz/driver/nic.h>
#include <twz/driver/queue.h>

#include "cons.h"

//forward decleration
typedef struct interface interface_t;


#pragma pack(push,1)
typedef struct mac_addr
{
    //unsigned char mac[6];
    uint8_t mac[6];
}mac_addr_t;

typedef struct eth_hdr
{
    /*preamble (7 bytes) and SFD (1 byte) are used by physical layer, so we don't need to include them*/
    mac_addr_t dst_mac; //6 bytes
    mac_addr_t src_mac;
    unsigned short type; //aka type field... 2 bytes
    char payload[248];/*Allowed 46-1500 bytes*/ //this needs to me modified so data is allocated then freed every time we send data
}eth_hdr_t;
#pragma pack(pop)


/*network APIs*/
void l2_send(char *data, twzobj *queue_obj, twzobj *interface_obj); //change this function to also add type of data
void l2_recv(twzobj *queue_obj);

