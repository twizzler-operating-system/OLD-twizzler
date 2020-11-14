#ifndef __PORT_H__
#define __PORT_H__

#include "common.h"

void bind_to_port(uint16_t port_num,
                  uint8_t protocol);

uint16_t bind_to_random_port(uint8_t protocol);

void free_port(uint16_t port_num,
               uint8_t protocol);

#endif
