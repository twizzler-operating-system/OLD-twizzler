#include "intr_props.h"

void init_interface(char *name, char *info_queue, twzobj *interface_props)
{
    twzobj info_obj;
    
    /*create object to hold interface properties*/
    if(twz_object_new(interface_props, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_NONE) < 0)
    {
        fprintf(stderr, "init_interface: Could not create object for interface properties\n");
        exit(1);
    }
    if(twz_name_assign(twz_object_guid(interface_props), name)<0)
    {
        fprintf(stderr,"init_interface: Could not name object for interface properties\n");
        exit(1);
    }

    /*open info object to get physical information of interface*/
    if((twz_object_init_name(&info_obj, info_queue, FE_READ | FE_WRITE)))
    {
        fprintf(stderr, "init_interface: Failed to open info-queue\n");
        exit(1);
    }
    struct nic_header *nh = (struct nic_header *)twz_object_base(&info_obj);
    uint64_t flags = nh->flags;
    while(!(flags & NIC_FL_MAC_VALID))
    {
        /* if it's not set, sleep and specify the comparison value as the value we just checked. */
        twz_thread_sync(THREAD_SYNC_SLEEP, &nh->flags, flags, /* timeout */ NULL);
        /* we woke up, so someone woke us up. Reload the flags to check the new value */
        flags = nh->flags;
        /* we have to go around the loop again because we might have had a spurious wake up. */
    }

    /*assign inferface properties*/
    interface_t *intr = (interface_t *)twz_object_base(interface_props);
    strncpy(intr->name, name, MAX_INTERFACE_NAME_SIZE);
    memcpy(intr->mac.mac, nh->mac, MAC_ADDR_SIZE);
    intr->is_ipv4_addr_set = false;
}

