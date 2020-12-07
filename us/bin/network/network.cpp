#include "client.h"
#include "common.h"
#include "interface.h"
#include "eth.h"
#include "ipv4.h"
#include "encapsulate.h"
#include "udp_conn.h"
#include "tcp_conn.h"
#include "twz.h"
#include "twz_op.h"

int main(int argc, char *argv[])
{
	ip_addr_t interface_ip;
	ip_addr_t interface_bcast_ip;

	if(argc > 2) {
		/* extract IP address */
		interface_ip = string_to_ip_addr(argv[1]);
		interface_bcast_ip = string_to_ip_addr(argv[2]);
	} else {
		fprintf(stderr,
		  "usage: network host_ip_address host_broadcast_ip "
		  "<program_name> <args>\n");
		exit(1);
	}

	client_handlers_init();

	/* initialize a new network interface */
	init_interface("/dev/e1000", "/dev/e1000-info", interface_ip, interface_bcast_ip);

	/* start thread to receive packets on the new interface */
	std::thread recv_thread(eth_rx, "/dev/e1000");

	/* start thread to send TCP packets on the new interface */
	std::thread send_thread(handle_tcp_send);

	/* bootstrap IP table */
	ip_table_entry_t entry;
	for(int i = 0; i < IP_ADDR_SIZE; ++i) {
		entry.dst_ip.ip[i] = 0;
		entry.netmask.ip[i] = 0;
		entry.gateway.ip[i] = 0;
	}
	strncpy(entry.tx_interface_name, "/dev/e1000", MAX_INTERFACE_NAME_SIZE);

	ip_table_put(entry);

	/* bootstrap TCP connection table */
	bootstrap_conn_table();

	/* bind twizzler op port */
	if (bind_to_udp_port(0, string_to_ip_addr(TWIZZLER_IP), TWIZZLER_PORT) != 0) {
        exit(1);
    }

	/* free packet memory asynchronously */
	interface_t *interface = get_interface_by_name("/dev/e1000");
	std::thread free_pkt_memory(free_packet_buffer_object, &interface->tx_queue_obj);

///////////////////////////////////////////////////////////////////////////////////

	/* Run specified network program */
	if(argc > 3) {
		if(strcmp(argv[3], "udp") == 0) {
            if(strcmp(argv[1], "10.0.0.1") == 0) { //client
                uint16_t client_id = 3939;

                //send
                char data[115] =
                    "This is an amazing place to live but the cost of living is "
                    "extremely high. I guess you win some, you loose some :)";

                for (int i = 0; i < 115; i = i + 23) {
                    int ret = udp_send(client_id, string_to_ip_addr("10.0.0.2"),
                            3004, (data + i), 23);

                    char tmp[24];
                    strncpy(tmp, data+i, 23);
                    if (ret == 0) {
                        fprintf(stderr, "UDP pkt with payload - %s SENT\n", tmp);
                    } else {
                        fprintf(stderr, "UDP pkt with payload - %s DROPPED\n", tmp);
                    }
                }
            }

            if(strcmp(argv[1], "10.0.0.2") == 0) { //server
                uint16_t client_id = 7474;

                //bind
                bind_to_udp_port(client_id, string_to_ip_addr(argv[1]), 3004);

                //recv
                while (true) {
                    char buffer[1000];
                    ip_addr_t remote_ip;
                    uint16_t remote_port;

                    int ret = udp_recv(client_id, buffer, 1000,
                            &remote_ip, &remote_port);
                    if (ret > 0) {
                        fprintf(stderr, "Received from ('%u.%u.%u.%u', %u) %s\n",
                          (remote_ip.ip[0] & 0x000000FF),
                          (remote_ip.ip[1] & 0x000000FF),
                          (remote_ip.ip[2] & 0x000000FF),
                          (remote_ip.ip[3] & 0x000000FF),
                          (remote_port & 0x0000FFFF),
                          buffer);
                    } else {
                        usleep(1000);
                    }
                }
            }

		} else if(strcmp(argv[3], "tcp") == 0) {
            if(strcmp(argv[1], "10.0.0.1") == 0) { //client
                half_conn_t remote;
                memcpy(remote.ip.ip, string_to_ip_addr("10.0.0.2").ip,
                        IP_ADDR_SIZE);
                remote.port = 3004;

                uint16_t client_id = 567;

                //connect
                half_conn_t local;
                int ret = establish_tcp_conn_client(client_id, &local, remote);
                if(ret == 0) {
                    fprintf(stderr,
                      "Connection established with "
                      "('%u.%u.%u.%u', %u)\n",
                      (remote.ip.ip[0] & 0x000000FF),
                      (remote.ip.ip[1] & 0x000000FF),
                      (remote.ip.ip[2] & 0x000000FF),
                      (remote.ip.ip[3] & 0x000000FF),
                      (remote.port & 0x0000FFFF));
                } else {
                    fprintf(stderr,
                      "Connection establish with "
                      "('%u.%u.%u.%u', %u) failed\n",
                      (remote.ip.ip[0] & 0x000000FF),
                      (remote.ip.ip[1] & 0x000000FF),
                      (remote.ip.ip[2] & 0x000000FF),
                      (remote.ip.ip[3] & 0x000000FF),
                      (remote.port & 0x0000FFFF));
                    exit(1);
                }

                //send
                char data[115] =
                    "This is an amazing place to live but the cost of living is "
                    "extremely high. I guess you win some, you loose some :)";
                ret = tcp_send(client_id, data, 115);
                if (ret == 0) {
                    fprintf(stderr, "TCP SEND TO NET STACK SUCCEDDED!!");
                }
            }

            if(strcmp(argv[1], "10.0.0.2") == 0) { //server
                half_conn_t local;
                memcpy(local.ip.ip, string_to_ip_addr(argv[1]).ip, IP_ADDR_SIZE);
                local.port = 3004;

                uint16_t client_id = 585;

                //bind
                if (bind_to_tcp_port(client_id, local.ip, local.port) != 0) {
                    exit(1);
                }

                //listen + accept
                half_conn_t remote;
                while (true) {
                    int ret = establish_tcp_conn_server(client_id, local, &remote);
                    if (ret == 0) {
                        fprintf(stderr,
                          "Connection established with "
                          "('%u.%u.%u.%u', %u)\n",
                          (remote.ip.ip[0] & 0x000000FF),
                          (remote.ip.ip[1] & 0x000000FF),
                          (remote.ip.ip[2] & 0x000000FF),
                          (remote.ip.ip[3] & 0x000000FF),
                          (remote.port & 0x0000FFFF));

                        //recv
                        while (true) {
                            char data[MSS+1] = { 0 };
                            int ret = tcp_recv(client_id, data, MSS);
                            if (ret > 0) {
                                fprintf(stderr, "%s", data);
                            } else {
                                usleep(1000);
                            }
                        }

                    } else {
                        usleep(1000);
                    }
                }
            }

		} else if(strcmp(argv[3], "twz") == 0) {
			if(argc == 5) {
				// populate local object mappings
				object_id_t object_id;
				for(int i = 0; i < OBJECT_ID_SIZE; ++i) {
					object_id.id[i] = interface->mac.mac[i % MAC_ADDR_SIZE];
				}
				obj_mapping_insert(object_id.id, interface->ip.ip);

				if(strcmp(argv[4], "0") == 0 || strcmp(argv[4], "1") == 0) {
					if(strcmp(argv[1], "10.0.0.1") == 0) { // read/write
						int count = 0;
						object_id_t obj_id_1 = (object_id_t){ .id = { 0x08,
							                                    0x00,
							                                    0x00,
							                                    0x00,
							                                    0x02,
							                                    0x22,
							                                    0x08,
							                                    0x00,
							                                    0x00,
							                                    0x00,
							                                    0x02,
							                                    0x22,
							                                    0x08,
							                                    0x00,
							                                    0x00,
							                                    0x00 } };
						object_id_t obj_id_2 = (object_id_t){ .id = { 0x08,
							                                    0x00,
							                                    0x00,
							                                    0x00,
							                                    0x03,
							                                    0x33,
							                                    0x08,
							                                    0x00,
							                                    0x00,
							                                    0x00,
							                                    0x03,
							                                    0x33,
							                                    0x08,
							                                    0x00,
							                                    0x00,
							                                    0x00 } };
						char payload[5];
						while(count < 1000) {
							++count;
							sprintf(payload, "%d", count);
							int ret;
							if(count % 2 == 0) {
								ret = twz_op_send(
								  "/dev/e1000", obj_id_1, TWZ_READ_REQ, payload, 5, atoi(argv[4]));
							} else {
								ret = twz_op_send(
								  "/dev/e1000", obj_id_2, TWZ_WRITE_REQ, payload, 5, atoi(argv[4]));
							}
							usleep(1000000);
						}

					} else if(strcmp(argv[4], "0") == 0) { // advertise
						usleep(20000000);
						int ret =
						  twz_op_send("/dev/e1000", object_id, TWZ_ADVERT, NULL, 0, atoi(argv[4]));
					}
				}

			} else {
				fprintf(stderr,
				  "Error: twz program requires "
				  "[0/1] as argument\n");
				exit(1);
			}

		} else {
			fprintf(stderr, "Error: unrecognized program name\n");
			exit(1);
		}
	}

	for(;;)
		usleep(100000);

	return 0;
}
