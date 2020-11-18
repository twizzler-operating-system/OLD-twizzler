#include "common.h"
#include "interface.h"
#include "eth.h"
#include "twz.h"
#include "ipv4.h"
#include "port.h"
#include "encapsulate.h"
#include "char_ring_buffer.h"
#include "tcp_conn.h"
#include "twz_op.h"
#include "client.h"

int main(int argc, char* argv[])
{
    ip_addr_t interface_ip;
    ip_addr_t interface_bcast_ip;

    if (argc > 2) {
        /* extract IP address */
        interface_ip = string_to_ip_addr(argv[1]);
        interface_bcast_ip = string_to_ip_addr(argv[2]);
    } else {
        fprintf(stderr, "usage: network host_ip_address host_broadcast_ip "
                "<program_name> <args>\n");
        exit(1);
    }

	client_handlers_init();

    /* initialize a new network interface */
    init_interface("/dev/e1000", "/dev/e1000-info",
            interface_ip, interface_bcast_ip);

    /* start thread to receive packets on the new interface */
    std::thread recv_thread (eth_rx, "/dev/e1000");

    /* start thread to send TCP packets on the new interface */
    std::thread send_thread (send_tcp_data);

    /* bootstrap IP table */
    ip_table_entry_t entry;
    for (int i = 0; i < IP_ADDR_SIZE; ++i) {
        entry.dst_ip.ip[i] = 0;
        entry.netmask.ip[i] = 0;
        entry.gateway.ip[i] = 0;
    }
    strncpy(entry.tx_interface_name, "/dev/e1000", MAX_INTERFACE_NAME_SIZE);

    ip_table_put(entry);

    /* bootstrap TCP connection table */
    bootstrap_conn_table();

    /* bind twizzler op port */
    bind_to_port(TWIZZLER_PORT, UDP);

    /* free packet memory asynchronously */
    interface_t* interface = get_interface_by_name("/dev/e1000");
    std::thread free_pkt_memory (free_packet_buffer_object,
            &interface->tx_queue_obj);

///////////////////////////////////////////////////////////////////////////////////

    /* Run specified network program */
    if (argc > 3) {
        if (strcmp(argv[3], "udp") == 0) {
            if (argc == 7) {
                object_id_t object_id;
                int count = 0;
                while (count < 50) {
                    ++count;
                    char payload[19] = "This is test data.";
                    fprintf(stderr, "Sending out a UDP packet...");
                    int ret = encap_udp_packet
                        (object_id, NOOP,
                         string_to_ip_addr(argv[4]),
                         atoi(argv[5]),
                         atoi(argv[6]),
                         payload,
                         19);
                    if (ret == 0) {
                        fprintf(stderr, "Packet successfully sent\n");
                    } else {
                        fprintf(stderr, "Packet send failed\n");
                    }
                    usleep(1000000);
                }

            } else {
                fprintf(stderr, "Error: udp program requires 3 arguments - "
                        "dst_ip_addr src_port dst_port\n");
                exit(1);
            }

        } else if (strcmp(argv[3], "tcp") == 0) {
            if (argc == 7) {
                if (strcmp(argv[1], "10.0.0.1") == 0) { //client
                    half_conn_t remote;
                    memcpy(remote.ip.ip,string_to_ip_addr(argv[4]).ip,IP_ADDR_SIZE);
                    remote.port = atoi(argv[6]);

                    half_conn_t local;
                    int ret = establish_tcp_conn_client(&local, remote);
                    if (ret == 0) {
                        fprintf(stderr, "Connection established with "
                                "('%u.%u.%u.%u', %u)\n",
                                (remote.ip.ip[0] & 0x000000FF),
                                (remote.ip.ip[1] & 0x000000FF),
                                (remote.ip.ip[2] & 0x000000FF),
                                (remote.ip.ip[3] & 0x000000FF),
                                (remote.port & 0x0000FFFF));
                    } else {
                        fprintf(stderr, "Connection establish with "
                                "('%u.%u.%u.%u', %u) failed\n",
                                (remote.ip.ip[0] & 0x000000FF),
                                (remote.ip.ip[1] & 0x000000FF),
                                (remote.ip.ip[2] & 0x000000FF),
                                (remote.ip.ip[3] & 0x000000FF),
                                (remote.port & 0x0000FFFF));
                    }

                    tcp_conn_t conn;
                    memcpy(conn.local.ip.ip, local.ip.ip, IP_ADDR_SIZE);
                    memcpy(conn.remote.ip.ip, remote.ip.ip, IP_ADDR_SIZE);
                    conn.local.port = local.port;
                    conn.remote.port = remote.port;

                    tcp_conn_state_t* conn_state = conn_table_get(conn);

                    char data[115] = "This is an amazing place to live but the cost of living is extremely high. I guess you win some, you loose some :)";
                    char_ring_buffer_add(conn_state->tx_buffer, data, 115);
                }

                if (strcmp(argv[1], "10.0.0.2") == 0) { //server
                    half_conn_t local;
                    bind_to_ip(string_to_ip_addr(argv[1]));
                    memcpy(local.ip.ip, string_to_ip_addr(argv[1]).ip, IP_ADDR_SIZE);
                    bind_to_port(atoi(argv[5]), TCP);
                    local.port = atoi(argv[5]);

                    half_conn_t remote;
                    int ret = establish_tcp_conn_server(local, &remote);
                    fprintf(stderr, "Connection established with "
                            "('%u.%u.%u.%u', %u)\n",
                            (remote.ip.ip[0] & 0x000000FF),
                            (remote.ip.ip[1] & 0x000000FF),
                            (remote.ip.ip[2] & 0x000000FF),
                            (remote.ip.ip[3] & 0x000000FF),
                            (remote.port & 0x0000FFFF));

                    tcp_conn_t conn;
                    memcpy(conn.local.ip.ip, local.ip.ip, IP_ADDR_SIZE);
                    memcpy(conn.remote.ip.ip, remote.ip.ip, IP_ADDR_SIZE);
                    conn.local.port = local.port;
                    conn.remote.port = remote.port;

                    tcp_conn_state_t* conn_state = conn_table_get(conn);

                    while (true) {
                        char data[MSS] = {0};
                        uint32_t ret = char_ring_buffer_remove
                            (conn_state->rx_buffer, data, MSS);
                        if (ret > 0) {
                            fprintf(stderr, "%s", data);
                        } else {
                            usleep(1000);
                        }
                    }
                }

            } else {
                fprintf(stderr, "Error: tcp program requires 3 arguments - "
                        "dst_ip_addr src_port dst_port\n");
                exit(1);
            }

        } else if (strcmp(argv[3], "twz") == 0) {
            if (argc == 5) {
                //populate local object mappings
                object_id_t object_id;
                for (int i = 0; i < OBJECT_ID_SIZE; ++i) {
                    object_id.id[i] = interface->mac.mac[i % MAC_ADDR_SIZE];
                }
                obj_mapping_insert(object_id.id, interface->ip.ip);

                if (strcmp(argv[4], "0") == 0
                || strcmp(argv[4], "1") == 0) {
                    if (strcmp(argv[1], "10.0.0.1") == 0) { //read/write
                        int count = 0;
                        object_id_t obj_id_1 = (object_id_t) {
                            .id = {0x08, 0x00, 0x00, 0x00, 0x02, 0x22,
                                   0x08, 0x00, 0x00, 0x00, 0x02, 0x22,
                                   0x08, 0x00, 0x00, 0x00}
                        };
                        object_id_t obj_id_2 = (object_id_t) {
                            .id = {0x08, 0x00, 0x00, 0x00, 0x03, 0x33,
                                   0x08, 0x00, 0x00, 0x00, 0x03, 0x33,
                                   0x08, 0x00, 0x00, 0x00}
                        };
                        char payload[5];
                        while (count < 1000) {
                            ++count;
                            sprintf(payload, "%d", count);
                            int ret;
                            if (count % 2 == 0) {
                                ret = twz_op_send("/dev/e1000", obj_id_1,
                                        TWZ_READ_REQ, payload, 5,
                                        atoi(argv[4]));
                            } else {
                                ret = twz_op_send("/dev/e1000", obj_id_2,
                                        TWZ_WRITE_REQ, payload, 5,
                                        atoi(argv[4]));
                            }
                            usleep(1000000);
                        }

                    } else if (strcmp(argv[4], "0") == 0){ //advertise
                        usleep(20000000);
                        int ret = twz_op_send("/dev/e1000", object_id,
                                TWZ_ADVERT, NULL, 0, atoi(argv[4]));
                    }
                }

            } else {
                fprintf(stderr, "Error: twz program requires "
                        "[0/1] as argument\n");
                exit(1);
            }

        } else {
            fprintf(stderr, "Error: unrecognized program name\n");
            exit(1);
        }
    }

    for (;;)
        usleep(100000);

    return 0;
}
