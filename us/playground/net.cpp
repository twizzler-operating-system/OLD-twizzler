#include <cstdio>
#include <twz/obj.h>
#include <twz/objctl.h>
#include <twz/queue.h>
#include <unistd.h>

#include <thread>
#include <twz/driver/nic.h>
#include <twz/driver/queue.h>

twzobj txqueue_obj, rxqueue_obj, info_obj;

void consumer()
{
	while(1) {
		struct packet_queue_entry pqe;
		/* recv a packet queue entry */
		queue_receive(&rxqueue_obj, (struct queue_entry *)&pqe, 0);
		fprintf(stderr, "net got packet!\n");

		/* packet structure from the nic starts with a packet_header struct that contains some
		 * useful information (or, will in the future), followed by the actual packet data. */
		struct packet_header *ph = (struct packet_header *)twz_object_lea(&rxqueue_obj, pqe.ptr);
		/* packet data follows the packet header */
		char *data = (char *)(ph + 1);
		/* pqe.len refers to the length of the packet data PLUS the packet_header. The packet
		 * header's length (ph->len, here) refers to the length of the data. */
		fprintf(
		  stderr, ":: packet_header: %p %d :: packet_data: %p %d\n", ph, pqe.len, data, ph->len);

		fprintf(stderr, ":: packet contents: %s\n", (char *)data);

		/* when we're done with the packet, we'll tell the nic driver. This informs the nic driver
		 * that it may use the packet buffer that it gave us, so don't do this until we're ready to
		 * give up this memory! */
		queue_complete(&rxqueue_obj, (struct queue_entry *)&pqe, 0);
	}
}
// net /dev/e1000-txqueue /dev/e1000-rxqueue /dev/e1000-info
int main(int argc, char **argv)
{
	if(argc < 4) {
		fprintf(stderr, "usage: net txqueue rxqueue info\n");
		return 1;
	}
	int r;
	r = twz_object_init_name(&txqueue_obj, argv[1], FE_READ | FE_WRITE);
	if(r) {
		fprintf(stderr, "e1000: failed to open txqueue\n");
		return 1;
	}

	r = twz_object_init_name(&rxqueue_obj, argv[2], FE_READ | FE_WRITE);
	if(r) {
		fprintf(stderr, "e1000: failed to open rxqueue\n");
		return 1;
	}

	r = twz_object_init_name(&info_obj, argv[3], FE_READ | FE_WRITE);
	if(r) {
		fprintf(stderr, "e1000: failed to open rxqueue\n");
		return 1;
	}

	fprintf(stderr, "NET TESTING\n");

	/* NICs expose an "info object" that contains a bunch of data about them. Like the MAC address.
	 * But we have to protect this against reading it before it's ready. We do this with a simple
	 * flag, "NIC_FL_MAC_VALID", that indicates if the MAC data is valid or not. For now, we'll
	 * assume the MAC data itself is atomic too... */
	struct nic_header *nh = (struct nic_header *)twz_object_base(&info_obj);
	/* we can also wait on these flags, because if the NIC changes them, it's supposed to wake up
	 * any thread waiting on this memory qword. If you're not familiar with what this means, check
	 * out futexes on linux (this is similar).
	 *
	 * This is a standard wait loop for checking if a bit is set and waiting if not. First, grab the
	 * word, and check the bit */
	uint64_t flags = nh->flags;
	while(!(flags & NIC_FL_MAC_VALID)) {
		/* if it's not set, sleep and specify the comparison value as the value we just checked. */
		twz_thread_sync(THREAD_SYNC_SLEEP, &nh->flags, flags, /* timeout */ NULL);
		/* we woke up, so someone woke us up. Reload the flags to check the new value */
		flags = nh->flags;
		/* we have to go around the loop again because we might have had a spurious wake up. */
	}
	/* for now the mac is stored as a uint8_t[6]. Let me know if you want it in a different form...
	 * Maybe we'll add a uint32_t as well via a union. */
	fprintf(stderr,
	  "MAC is %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
	  nh->mac[0],
	  nh->mac[1],
	  nh->mac[2],
	  nh->mac[3],
	  nh->mac[4],
	  nh->mac[5]);

	twzobj buf_obj;
	if(twz_object_new(&buf_obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE))
		return 1;

	std::thread thr(consumer);

	void *d = twz_object_base(&buf_obj);
	sprintf((char *)d, "              this is a test from net!\n");
	struct packet_queue_entry p;
	p.ptr = twz_ptr_swizzle(&txqueue_obj, twz_object_base(&buf_obj), FE_READ);
	p.len = 62;

	p.qe.info = 1;

	while(1) {
		usleep(100000);
		fprintf(stderr, "submiting: %d\n", p.qe.info);
		queue_submit(&txqueue_obj, (struct queue_entry *)&p, 0);
		fprintf(stderr, "submitted: %d\n", p.qe.info);

		queue_get_finished(&txqueue_obj, (struct queue_entry *)&p, 0);
		fprintf(stderr, "completed: %d\n", p.qe.info);
	}

	for(;;)
		usleep(10000);
}
