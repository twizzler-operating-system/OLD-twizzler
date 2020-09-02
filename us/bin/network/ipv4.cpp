#include "flip.h"

bool assign_ipv4_addr_to_intr(ipv4_addr_t *addr, twzobj *interface_obj)
{
    interface_t *interface = (interface_t *)twz_object_base(interface_obj);

    if(interface < 0) return false; //how do we know this didn't work?

    interface->is_ipv4_addr_set = true;
    memcpy(interface->ipv4_addr.addr, addr, MAX_IPV4_CHAR_SIZE);
    return true;
}

void get_intr_ipv4(twzobj *interface_obj, char *ipv4_addr)
{
    interface_t *interface = (interface_t *)twz_object_base(interface_obj);
    if(interface->is_ipv4_addr_set)
        strncpy(ipv4_addr, interface->ipv4_addr.addr, MAX_IPV4_CHAR_SIZE);
}
