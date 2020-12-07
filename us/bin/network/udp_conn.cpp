#include "udp_conn.h"

#include "interface.h"
#include "twz.h"
#include "ipv4.h"
#include "encapsulate.h"


std::map<uint16_t,uint16_t> udp_client_table;
std::mutex udp_client_table_mutex;


void udp_client_table_put(uint16_t client_id,
                          uint16_t udp_port)
{
    std::map<uint16_t,uint16_t>::iterator it;

    udp_client_table_mutex.lock();

    bool found = false;
    for (it = udp_client_table.begin(); it != udp_client_table.end(); ++it) {
        if (it->first == client_id) {
            found = true;
            break;
        }
    }

    if (!found) {
        udp_client_table[client_id] = udp_port;
    } else {
        it->second = udp_port;
    }

    udp_client_table_mutex.unlock();
}


uint16_t udp_client_table_get(uint16_t client_id)
{
    std::map<uint16_t,uint16_t>::iterator it;

    udp_client_table_mutex.lock();

    bool found = false;
    for (it = udp_client_table.begin(); it != udp_client_table.end(); ++it) {
        if (it->first == client_id) {
            found = true;
            break;
        }
    }

    udp_client_table_mutex.unlock();

    if (!found) {
        return 0;
    } else {
        return it->second;
    }
}


void udp_client_table_delete(uint16_t client_id)
{
    std::map<uint16_t,uint16_t>::iterator it;

    udp_client_table_mutex.lock();

    bool found = false;
    for (it = udp_client_table.begin(); it != udp_client_table.end(); ++it) {
        if (it->first == client_id) {
            found = true;
            break;
        }
    }

    if (found) {
        udp_client_table.erase(it);
    }

    udp_client_table_mutex.unlock();
}


void udp_client_table_view()
{
    std::map<uint16_t,uint16_t>::iterator it;

    udp_client_table_mutex.lock();

    fprintf(stderr, "UDP CLIENT TABLE:\n");
    fprintf(stderr, "------------------------------------------\n");
    for (it = udp_client_table.begin(); it != udp_client_table.end(); ++it) {
        fprintf(stderr, "%u -> %u\n", it->first, it->second);
    }
    fprintf(stderr, "------------------------------------------\n");

    udp_client_table_mutex.unlock();
}


udp_port_t* udp_ports[65536] = {NULL};
std::mutex udp_port_mtx;


udp_port_t* get_udp_port(uint16_t port)
{
    return udp_ports[port];
}


int bind_to_udp_port(uint16_t client_id,
                     ip_addr_t ip,
                     uint16_t port)
{
    if (bind_to_ip(ip) != 0) {
        return EUDPBINDFAILED;
    }

    if (port == 0) {
        fprintf(stderr, "Error bind_to_port: port number cannot be 0\n");
        return EUDPBINDFAILED;
    }

    udp_port_mtx.lock();

    if (udp_ports[port] == NULL) {
        udp_ports[port] = (udp_port_t *)malloc(sizeof(udp_port_t));
        udp_ports[port]->client_id = client_id;
        memcpy(udp_ports[port]->ip.ip, ip.ip, IP_ADDR_SIZE);
        udp_ports[port]->rx_buffer = create_generic_ring_buffer(PKT_BUFFER_SIZE);
        udp_port_mtx.unlock();

        udp_client_table_put(client_id, port);

        return 0;

    } else {
        fprintf(stderr, "Error bind_to_udp_port: port %d already in use\n", port);
        return EUDPBINDFAILED;
    }
}


uint16_t bind_to_random_udp_port(uint16_t client_id)
{
    udp_port_mtx.lock();

    for (uint32_t i = 1; i < 65536; ++i) {
        if (udp_ports[i] == NULL) {
            udp_ports[i] = (udp_port_t *)malloc(sizeof(udp_port_t));
            udp_ports[i]->client_id = client_id;
            memcpy(udp_ports[i]->ip.ip, string_to_ip_addr(DEFAULT_IP).ip,
                    IP_ADDR_SIZE);
            udp_ports[i]->rx_buffer = create_generic_ring_buffer(PKT_BUFFER_SIZE);
            udp_port_mtx.unlock();

            udp_client_table_put(client_id, i);

            return i;
        }
    }

    fprintf(stderr, "Error bind_to_random_port: no free ports available\n");
    exit(1);
}


void free_udp_port(uint16_t port)
{
    udp_port_mtx.lock();

    if (udp_ports[port] != NULL) {
        if (udp_ports[port]->rx_buffer != NULL) {
            free_generic_ring_buffer(udp_ports[port]->rx_buffer);
        }
        free(udp_ports[port]);
        udp_port_mtx.unlock();
        return;
    }
}


int udp_send(uint16_t client_id,
             ip_addr_t dst_ip,
             uint16_t dst_port,
             char* payload,
             uint16_t payload_size)
{
    uint16_t src_port = udp_client_table_get(client_id);
    if (src_port == 0) {
        src_port = bind_to_random_udp_port(client_id);
    }

    object_id_t object_id;
    return encap_udp_packet(object_id,
                            NOOP,
                            udp_ports[src_port]->ip,
                            dst_ip,
                            src_port,
                            dst_port,
                            payload,
                            payload_size);
}


int udp_recv(uint16_t client_id,
             char* buffer,
             uint16_t buffer_size,
             ip_addr_t* remote_ip,
             uint16_t* remote_port)
{
    uint16_t src_port = udp_client_table_get(client_id);
    if (src_port != 0) {
        remote_info_t* remote_info = (remote_info_t *)
            generic_ring_buffer_remove(udp_ports[src_port]->rx_buffer);

        if (remote_info != NULL && buffer_size >= remote_info->payload_size) {
            memcpy(buffer, remote_info->payload, remote_info->payload_size);
            memcpy(remote_ip->ip, remote_info->remote_ip.ip, IP_ADDR_SIZE);
            *remote_port = remote_info->remote_port;
            int ret = remote_info->payload_size;
            free(remote_info);
            return ret;
        } else {
            return 0;
        }
    }

    return 0;
}


void cleanup_udp_conn(uint16_t port)
{
    /* cleanup entry from udp client table */
    udp_port_t* udp_port = udp_ports[port];
    if (udp_port != NULL) {
        udp_client_table_delete(udp_port->client_id);
    }

    /* free local UDP port */
    free_udp_port(port);
}

