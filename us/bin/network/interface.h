#ifndef __INTERFACE_H__
#define __INTERFACE_H__

#include "common.h"

#define MAX_INTERFACE_NAME_SIZE 248

typedef struct interface {
    char name[MAX_INTERFACE_NAME_SIZE];
    mac_addr_t mac;
    ip_addr_t ip;
    twzobj tx_queue_obj;
    twzobj rx_queue_obj;
} interface_t;

void init_interface(const char* interface_name,
                  const char* info_queue_name,
                  ip_addr_t interface_ip);

interface_t* get_interface_by_name(const char* interface_name);

#endif
