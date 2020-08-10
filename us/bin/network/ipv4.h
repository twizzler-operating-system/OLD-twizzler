#include <twz/obj.h>

#include "cons.h"

typedef struct interface interface_t;

typedef struct ipv4_addr
{
    char addr[MAX_IPV4_CHAR_SIZE];
}ipv4_addr_t;

//must eventually set up loopback interface

//must add subnet mask later
bool assign_ipv4_addr_to_intr(ipv4_addr_t *addr, twzobj *interface_obj);
bool clear_intr_ipv4_settings(interface_t interface);



//must eventually add dump APIs to display interface settings
