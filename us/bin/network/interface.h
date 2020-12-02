#ifndef __INTERFACE_H__
#define __INTERFACE_H__

#include "common.h"

typedef struct interface {
    char name[MAX_INTERFACE_NAME_SIZE];
    mac_addr_t mac;
    ip_addr_t ip;
    ip_addr_t bcast_ip;
    twzobj tx_queue_obj;
    twzobj rx_queue_obj;
} interface_t;

void init_interface(const char* interface_name,
                    const char* info_queue_name,
                    ip_addr_t interface_ip,
                    ip_addr_t interface_bcast_ip);

interface_t* get_interface_by_name(const char* interface_name);

void get_interface_by_ip(ip_addr_t ip,
                         char* interface_name);

void bind_to_ip(ip_addr_t ip);

#endif
