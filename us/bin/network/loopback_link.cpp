#include "loopback_link.h"
#include "eth.h"


/* simulates a network link by moving packets from tx queue of an interface
 * to rx queue of the interface. Used for loopback testing */
void loopback_network_link(twzobj* tx_queue_obj,
                         twzobj* rx_queue_obj)
{
    fprintf(stderr, "Started a simulated link for loopback testing\n");

    while (true) {
        struct packet_queue_entry tx_pqe;
        queue_receive(tx_queue_obj, (struct queue_entry *)&tx_pqe, 0);
        char* pkt_ptr = (char *)twz_object_lea(tx_queue_obj, tx_pqe.ptr);

        usleep(1000); //propagation delay

        struct packet_queue_entry rx_pqe;
        rx_pqe.qe.info = tx_pqe.qe.info;
        rx_pqe.ptr = twz_ptr_swizzle(rx_queue_obj, pkt_ptr, FE_READ);
        rx_pqe.len = strlen(pkt_ptr);
        queue_submit(rx_queue_obj, (struct queue_entry *)&rx_pqe, 0);
    }
}
