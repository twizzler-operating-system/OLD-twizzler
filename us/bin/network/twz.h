#ifndef __TWZ_H__
#define __TWZ_H__

#include "common.h"

#define TWZ_HDR_SIZE 18 //bytes

typedef struct __attribute__((__packed__)) twz_hdr {
    object_id_t object_id;
    uint16_t type;
} twz_hdr_t;

void twz_tx(object_id_t object_id,
          uint16_t twz_type,
          void* pkt_ptr);

void twz_rx(const char* interface_name,
          void* pkt_ptr);

#endif
