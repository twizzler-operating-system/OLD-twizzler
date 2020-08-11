#include "flip.h"

bool assign_ipv4_addr_to_intr(ipv4_addr_t *addr, twzobj *interface_obj)
{
    interface_t *interface = (interface_t *)twz_object_base(interface_obj);

    if(!interface) return false;

    interface->is_ipv4_addr_set = true;
    memcpy(interface->ipv4_addr.addr, addr, MAX_IPV4_CHAR_SIZE);
    return true;
}
