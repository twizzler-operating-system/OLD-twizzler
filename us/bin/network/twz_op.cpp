#include "twz_op.h"

#include "interface.h"
#include "twz.h"
#include "encapsulate.h"


std::map<uint8_t*,uint8_t*> obj_mapping;
std::mutex obj_mapping_mutex;


void obj_mapping_insert(uint8_t* object_id,
                        uint8_t* ip_addr)
{
    std::map<uint8_t*,uint8_t*>::iterator it;

    obj_mapping_mutex.lock();

    uint8_t* key = (uint8_t *)malloc(sizeof(uint8_t)*OBJECT_ID_SIZE);
    uint8_t* value = (uint8_t *)malloc(sizeof(uint8_t)*IP_ADDR_SIZE);
    memcpy(key, object_id, OBJECT_ID_SIZE);
    memcpy(value, ip_addr, IP_ADDR_SIZE);

    bool found;
    for (it = obj_mapping.begin(); it != obj_mapping.end(); ++it) {
        found = true;
        for (int i = 0; i < OBJECT_ID_SIZE; ++i) {
            if (it->first[i] != object_id[i]) {
                found = false;
                break;
            }
        }
        if (found) break;
    }

    if (!found) {
        obj_mapping[key] = value;
    } else {
        it->second = value;
    }

    obj_mapping_mutex.unlock();
}


uint8_t* obj_mapping_get(uint8_t* object_id)
{
    std::map<uint8_t*,uint8_t*>::iterator it;

    obj_mapping_mutex.lock();

    bool found;
    for (it = obj_mapping.begin(); it != obj_mapping.end(); ++it) {
        found = true;
        for (int i = 0; i < OBJECT_ID_SIZE; ++i) {
            if (it->first[i] != object_id[i]) {
                found = false;
                break;
            }
        }
        if (found) break;
    }

    obj_mapping_mutex.unlock();

    if (!found) {
        return NULL;
    } else {
        return it->second;
    }
}


void obj_mapping_delete(uint8_t* object_id)
{
    std::map<uint8_t*,uint8_t*>::iterator it;

    obj_mapping_mutex.lock();

    bool found;
    for (it = obj_mapping.begin(); it != obj_mapping.end(); ++it) {
        found = true;
        for (int i = 0; i < OBJECT_ID_SIZE; ++i) {
            if (it->first[i] != object_id[i]) {
                found = false;
                break;
            }
        }
        if (found) {
            free(it->first);
            free(it->second);
            obj_mapping.erase(it);
            break;
        }
    }

    obj_mapping_mutex.unlock();
}


void obj_mapping_view()
{
    std::map<uint8_t*,uint8_t*>::iterator it;

    obj_mapping_mutex.lock();

    fprintf(stderr, "OBJ MAPPING TABLE:\n");
    fprintf(stderr, "------------------------------------------\n");
    for (it = obj_mapping.begin(); it != obj_mapping.end(); ++it) {
        int i;
        for (i = 0; i < OBJECT_ID_SIZE-1; ++i) {
            fprintf(stderr, "%02X", it->first[i]);
        }
        fprintf(stderr, "%02X -> ", it->first[i]);

        for (i = 0; i < IP_ADDR_SIZE-1; ++i) {
            fprintf(stderr, "%u.", (it->second[i] & 0x000000FF));
        }
        fprintf(stderr, "%u\n", (it->second[i] & 0x000000FF));
    }
    fprintf(stderr, "------------------------------------------\n");

    obj_mapping_mutex.unlock();
}


void obj_mapping_clear()
{
    std::map<uint8_t*,uint8_t*>::iterator it;

    obj_mapping_mutex.lock();

    for (it = obj_mapping.begin(); it != obj_mapping.end(); ++it) {
        free(it->first);
        free(it->second);
        obj_mapping.erase(it);
    }

    obj_mapping_mutex.unlock();
}


bool object_exists_locally(const char* interface_name,
                           object_id_t object_id)
{
    interface_t* interface = get_interface_by_name(interface_name);

    uint8_t* ip_addr = obj_mapping_get(object_id.id);

    if (ip_addr == NULL) return false;

    for (int i = 0; i < IP_ADDR_SIZE; ++i) {
        if (ip_addr[i] != interface->ip.ip[i]) {
            return false;
        }
    }

    return true;
}


int twz_op_send(const char* interface_name,
                object_id_t object_id,
                uint8_t twz_op,
                char* data,
                uint16_t data_size,
                uint8_t resource_discovery_protocol)
{
    interface_t* interface = get_interface_by_name(interface_name);

    ip_addr_t dst_ip;

    if (resource_discovery_protocol == CONTROLLER_BASED) {
        dst_ip = interface->bcast_ip;
    } else if (resource_discovery_protocol == END_TO_END) {
        uint8_t* addr = obj_mapping_get(object_id.id);
        if (addr == NULL) {
            dst_ip = interface->bcast_ip;
        } else {
            for (int i = 0; i < IP_ADDR_SIZE; ++i) {
                dst_ip.ip[i] = addr[i];
            }
        }
    } else {
        fprintf(stderr, "Error twz_op_send: invalid resource discovery "
                "protocol option (%d)\n", resource_discovery_protocol);
        return EINVALID_RESOURCE_DISCOVERY_PROTOCOL;
    }

    switch (twz_op) {
        case TWZ_ADVERT:
            if (resource_discovery_protocol == CONTROLLER_BASED) {
                char* msg = (char *)malloc(sizeof(char)*45);
                char* p = msg;
                for (int i = 0; i < OBJECT_ID_SIZE; ++i) {
                    msg += sprintf(msg, "%02X", object_id.id[i]);
                }
                for (int i = 0; i < MAC_ADDR_SIZE; ++i) {
                    msg += sprintf(msg, "%02X", interface->mac.mac[i]);
                }
                msg += sprintf(msg, ":");
                fprintf(stderr, "Sending twizzler packet (id: ");
                for (int i = 0; i < OBJECT_ID_SIZE; ++i) {
                    fprintf(stderr, "%02X", object_id.id[i]);
                }
                fprintf(stderr, ") OP: ADVERTISMENT ");
                fprintf(stderr, "to ('%s', %u) PAYLOAD: %s\n",
                        CONTROLLER_ADDR,
                        (CONTROLLER_PORT & 0x0000FFFF),
                        p);
                int ret =  encap_udp_packet(object_id,
                                            NOOP,
                                            string_to_ip_addr(CONTROLLER_ADDR),
                                            TWIZZLER_PORT,
                                            CONTROLLER_PORT,
                                            p,
                                            45);
                free(p);
                return ret;

            } else {
                return EINVALID_TWZ_OP;
            }

        case TWZ_READ_REQ:
            fprintf(stderr, "Sending twizzler packet (id: ");
            for (int i = 0; i < OBJECT_ID_SIZE; ++i) {
                fprintf(stderr, "%02X", object_id.id[i]);
            }
            fprintf(stderr, ") OP: READ REQUEST ");
            fprintf(stderr, "to ('%u.%u.%u.%u', %u) PAYLOAD: %s\n",
                    (dst_ip.ip[0] & 0x000000FF),
                    (dst_ip.ip[1] & 0x000000FF),
                    (dst_ip.ip[2] & 0x000000FF),
                    (dst_ip.ip[3] & 0x000000FF),
                    (TWIZZLER_PORT & 0x0000FFFF),
                    (data != NULL) ? data : "");
            return encap_udp_packet(object_id,
                                    twz_op,
                                    dst_ip,
                                    TWIZZLER_PORT,
                                    TWIZZLER_PORT,
                                    data,
                                    data_size);

        case TWZ_WRITE_REQ:
            fprintf(stderr, "Sending twizzler packet (id: ");
            for (int i = 0; i < OBJECT_ID_SIZE; ++i) {
                fprintf(stderr, "%02X", object_id.id[i]);
            }
            fprintf(stderr, ") OP: WRITE REQUEST ");
            fprintf(stderr, "to ('%u.%u.%u.%u', %u) PAYLOAD: %s\n",
                    (dst_ip.ip[0] & 0x000000FF),
                    (dst_ip.ip[1] & 0x000000FF),
                    (dst_ip.ip[2] & 0x000000FF),
                    (dst_ip.ip[3] & 0x000000FF),
                    (TWIZZLER_PORT & 0x0000FFFF),
                    (data != NULL) ? data : "");
            return encap_udp_packet(object_id,
                                    twz_op,
                                    dst_ip,
                                    TWIZZLER_PORT,
                                    TWIZZLER_PORT,
                                    data,
                                    data_size);

        default:
            fprintf(stderr, "Error twz_op_send: unrecognized twz op %d\n", twz_op);
            return EINVALID_TWZ_OP;
    }
}


void twz_op_recv(const char* interface_name,
                 remote_info_t* remote_info,
                 char* payload)
{
    fprintf(stderr, "Received twizzler packet (id: ");
    for (int i = 0; i < OBJECT_ID_SIZE; ++i) {
        fprintf(stderr, "%02X", remote_info->object_id.id[i]);
    }
    fprintf(stderr, ") ");

    switch (remote_info->twz_op) {
        case TWZ_READ_REQ:
            fprintf(stderr, "OP: READ REQUEST ");
            fprintf(stderr, "from ('%u.%u.%u.%u', %u) PAYLOAD: %s\n",
                    (remote_info->remote_ip.ip[0] & 0x000000FF),
                    (remote_info->remote_ip.ip[1] & 0x000000FF),
                    (remote_info->remote_ip.ip[2] & 0x000000FF),
                    (remote_info->remote_ip.ip[3] & 0x000000FF),
                    (remote_info->remote_port & 0x0000FFFF),
                    payload);
            if (object_exists_locally(interface_name, remote_info->object_id)) {
                //TODO read object
                char read_data[9] = "Read ACK";

                //reply back
                int ret =  encap_udp_packet(remote_info->object_id,
                                            TWZ_READ_REPLY,
                                            remote_info->remote_ip,
                                            TWIZZLER_PORT,
                                            TWIZZLER_PORT,
                                            read_data,
                                            9);
            } else {
                fprintf(stderr, "Error twz_op_recv: object ");
                for (int i = 0; i < OBJECT_ID_SIZE; ++i) {
                    fprintf(stderr, "%02X", remote_info->object_id.id[i]);
                }
                fprintf(stderr, " does not exist on this machine\n");
            }
            break;

        case TWZ_WRITE_REQ:
            fprintf(stderr, "OP: WRITE REQUEST ");
            fprintf(stderr, "from ('%u.%u.%u.%u', %u) PAYLOAD: %s\n",
                    (remote_info->remote_ip.ip[0] & 0x000000FF),
                    (remote_info->remote_ip.ip[1] & 0x000000FF),
                    (remote_info->remote_ip.ip[2] & 0x000000FF),
                    (remote_info->remote_ip.ip[3] & 0x000000FF),
                    (remote_info->remote_port & 0x0000FFFF),
                    payload);
            if (object_exists_locally(interface_name, remote_info->object_id)) {
                //TODO write payload
                //reply back
                uint16_t len = strlen(payload);
                char* msg = (char *)malloc(sizeof(char)*(11+len));
                sprintf(msg, "Write ACK %s", payload);
                int ret =  encap_udp_packet(remote_info->object_id,
                                            TWZ_WRITE_REPLY,
                                            remote_info->remote_ip,
                                            TWIZZLER_PORT,
                                            TWIZZLER_PORT,
                                            msg,
                                            11+len);
                free(msg);
            } else {
                fprintf(stderr, "Error twz_op_recv: object ");
                for (int i = 0; i < OBJECT_ID_SIZE; ++i) {
                    fprintf(stderr, "%02X", remote_info->object_id.id[i]);
                }
                fprintf(stderr, " does not exist on this machine\n");
            }
            break;

        case TWZ_READ_REPLY:
            fprintf(stderr, "OP: READ REPLY ");
            obj_mapping_insert(remote_info->object_id.id,
                               remote_info->remote_ip.ip);
            fprintf(stderr, "from ('%u.%u.%u.%u', %u) PAYLOAD: %s\n",
                    (remote_info->remote_ip.ip[0] & 0x000000FF),
                    (remote_info->remote_ip.ip[1] & 0x000000FF),
                    (remote_info->remote_ip.ip[2] & 0x000000FF),
                    (remote_info->remote_ip.ip[3] & 0x000000FF),
                    (remote_info->remote_port & 0x0000FFFF),
                    payload);
            break;

        case TWZ_WRITE_REPLY:
            fprintf(stderr, "OP: WRITE REPLY ");
            obj_mapping_insert(remote_info->object_id.id,
                               remote_info->remote_ip.ip);
            fprintf(stderr, "from ('%u.%u.%u.%u', %u) PAYLOAD: %s\n",
                    (remote_info->remote_ip.ip[0] & 0x000000FF),
                    (remote_info->remote_ip.ip[1] & 0x000000FF),
                    (remote_info->remote_ip.ip[2] & 0x000000FF),
                    (remote_info->remote_ip.ip[3] & 0x000000FF),
                    (remote_info->remote_port & 0x000000FF),
                    payload);
            break;

        default:
            fprintf(stderr, "Error twz_op_recv: invalid twizzler op (%d); "
                    "packet dropped\n", remote_info->twz_op);
    }
}

