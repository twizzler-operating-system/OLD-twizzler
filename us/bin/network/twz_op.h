#ifndef __TWZ_OP_H__
#define __TWZ_OP_H__

#include "common.h"

//options for resource discovery protocol
#define CONTROLLER_BASED 0
#define END_TO_END 1

#define TWIZZLER_PORT 9090 //port on host vm dedicated for twizzler control ops
//controller information
#define CONTROLLER_ADDR "10.0.0.4"
#define CONTROLLER_PORT 9091

//error codes
#define EINVALID_TWZ_OP 11
#define EINVALID_RESOURCE_DISCOVERY_PROTOCOL 12

int twz_op_send(const char* interface_name,
                object_id_t object_id,
                uint8_t twz_op,
                char* data,
                uint8_t resource_discovery_protocol);

void twz_op_recv(const char* interface_name,
                 remote_info_t* remote_info,
                 char* payload);

void obj_mapping_insert(uint8_t* object_id,
                        uint8_t* mac_addr);

uint8_t* obj_mapping_get(uint8_t* object_id);

void obj_mapping_delete(uint8_t* object_id);

void obj_mapping_view();

void obj_mapping_clear();

bool object_exists_locally(const char* interface_name,
                           object_id_t object_id);

#endif
