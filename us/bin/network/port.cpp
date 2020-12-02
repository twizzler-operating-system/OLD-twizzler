#include "port.h"

#include "interface.h"
#include "ipv4.h"


udp_port_t* udp_ports[65536] = {NULL};
std::mutex udp_port_mtx;


uint8_t tcp_ports[65536] = {0};
std::mutex tcp_port_mtx;


udp_port_t* get_udp_port(uint16_t port)
{
    return udp_ports[port];
}


void bind_to_udp_port(ip_addr_t ip,
                      uint16_t port)
{
    bind_to_ip(ip);

    if (port == 0) {
        fprintf(stderr, "Error bind_to_port: port number cannot be 0\n");
        exit(1);
    }

    udp_port_mtx.lock();

    if (udp_ports[port] == NULL) {
        udp_ports[port] = (udp_port_t *)malloc(sizeof(udp_port_t));
        memcpy(udp_ports[port]->ip.ip, ip.ip, IP_ADDR_SIZE);
        udp_ports[port]->rx_buffer = create_generic_ring_buffer(PKT_BUFFER_SIZE);
        udp_port_mtx.unlock();
        return;
    } else {
        fprintf(stderr, "Error bind_to_udp_port: port %d already in use\n", port);
        exit(1);
    }
}


uint16_t bind_to_random_udp_port()
{
    udp_port_mtx.lock();

    for (uint32_t i = 1; i < 65536; ++i) {
        if (udp_ports[i] == NULL) {
            udp_ports[i] = (udp_port_t *)malloc(sizeof(udp_port_t));
            memcpy(udp_ports[i]->ip.ip, string_to_ip_addr(DEFAULT_IP).ip,
                    IP_ADDR_SIZE);
            udp_ports[i]->rx_buffer = create_generic_ring_buffer(PKT_BUFFER_SIZE);
            udp_port_mtx.unlock();
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


void bind_to_tcp_port(ip_addr_t ip,
                      uint16_t port)
{
    bind_to_ip(ip);

    if (port == 0) {
        fprintf(stderr, "Error bind_to_port: port number cannot be 0\n");
        exit(1);
    }

    tcp_port_mtx.lock();

    if (tcp_ports[port] == 0) {
        tcp_ports[port] = 1;
        tcp_port_mtx.unlock();
        return;
    } else {
        fprintf(stderr, "Error bind_to_tcp_port: port %d already in use\n", port);
        exit(1);
    }
}


uint16_t bind_to_random_tcp_port()
{
    tcp_port_mtx.lock();

    for (uint32_t i = 1; i < 65536; ++i) {
        if (tcp_ports[i] == 0) {
            tcp_ports[i] = 1;
            tcp_port_mtx.unlock();
            return i;
        }
    }

    fprintf(stderr, "Error bind_to_random_port: no free ports available\n");
    exit(1);
}


void free_tcp_port(uint16_t port)
{
    tcp_port_mtx.lock();

    if (tcp_ports[port] == 1) {
        tcp_ports[port] = 0;
        tcp_port_mtx.unlock();
        return;
    }
}

