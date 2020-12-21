#include <stdio.h>

#include <twz/gate.h>
#include <twz/obj.h>

#include <nstack/net.h>
#include <nstack/nstack.h>

#include <time.h>

void timespec_diff(struct timespec *start, struct timespec *stop, struct timespec *result)
{
	if((stop->tv_nsec - start->tv_nsec) < 0) {
		result->tv_sec = stop->tv_sec - start->tv_sec - 1;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000ul;
	} else {
		result->tv_sec = stop->tv_sec - start->tv_sec;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec;
	}
	return;
}

int main(int argc, char **argv)
{
	printf("Hello, World!\n");

	if(!fork()) {
		int s = 0;
		if(argv[1] && argv[1][0] == 's')
			s = 1;

		usleep(100000);
		printf("!!!!!\n\n\n\n\n");
		if(s)
			execlp("network", "network", "10.0.0.1", "10.0.0.255", NULL);
		else
			execlp("network", "network", "10.0.0.2", "10.0.0.255", NULL);

		exit(0);
	}
	usleep(100000);
	usleep(2000000);
	printf("$$$$$$$$$\n\n\n\n\n");

	struct netmgr *mgr = netmgr_create("test-program", 0);

#if 0
	fprintf(stderr, "connecting\n");
	struct netaddr addr;
	struct netcon *con = netmgr_connect(mgr, &addr, 0, NULL);
	fprintf(stderr, "sending\n");

	struct timespec t0, t1, t2, d1, d2;
	char buf[12] = {};
	size_t len = sizeof(buf);
	size_t i = 0;
	while(1) {
		// printf("sending\n");
		clock_gettime(CLOCK_MONOTONIC, &t0);
		ssize_t ret = netcon_send(con, buf, len, 0);
		clock_gettime(CLOCK_MONOTONIC, &t1);
		// printf("send returned %ld\n", ret);
		ret = netcon_recv(con, buf, len, 0);
		clock_gettime(CLOCK_MONOTONIC, &t2);
		// printf("recv got :: %ld :: <%s>\n", ret, buf);

		timespec_diff(&t0, &t1, &d1);
		timespec_diff(&t1, &t2, &d2);
		i++;
		if(i % 100 == 0) {
			printf("%ld :: %ld: %ld %ld\n", i, len, d1.tv_nsec, d2.tv_nsec);
		}
	}
#endif

	if(argv[1] && argv[1][0] == 's') {
		struct netaddr addr = netaddr_from_ipv4_string("10.0.0.1", 5000);
		if(netmgr_bind(mgr, &addr, 0)) {
			fprintf(stderr, "[nas] fail to bind!\n");
			return 1;
		}
		while(1) {
			printf("Accepting...\n");
			struct netcon *cc = netcon_accept(mgr);
			assert(cc);
			printf("Accepted Conn! %d\n", cc->id);

			int shut = 0;
			while(!shut) {
#if 0
				char buf[5000] = {};
				size_t len = 5000;
				printf("Waiting on recv\n");
				ssize_t ret = netcon_recv(cc, buf, len, 0);
				printf("Recv got :: %ld :: <%s>\n", ret, buf);
#else
				struct netevent *nev = netevent_next_con(cc, 0);
				switch(nev->event) {
					case NETEVENT_RX:
						printf("RECV: %ld:<%s>\n", nev->data_len, nev->data_ptr);
						break;
					case NETEVENT_SHUTDOWN:
						printf("SHUTDOWN: %x\n", nev->info);
						shut = 1;
						break;
				}
				netevent_done(nev);
#endif
			}
			// netcon_shutdown(cc, NETCON_SHUTDOWN_WRITE);
			netcon_destroy(cc);
		}
	} else {
		printf("STARTIN AS CLIENT\n");
		struct netaddr addr = netaddr_from_ipv4_string("10.0.0.1", 5000);
		struct netcon *con = netmgr_connect(mgr, &addr, 0, NULL);
		if(!con) {
			fprintf(stderr, "[nac] got null con\n");
			return 1;
		}
		fprintf(stderr, "netmgr_connect returned!\n");
		while(1) {
#if 0
			const char *hw = "hello, world!\n";
			ssize_t ret = netcon_send(con, hw, strlen(hw) + 1, 0);
			printf("Sent Data\n");
#else
			struct netbuf buf;
			netmgr_get_buf(mgr, &buf);
			const char *hw = "hello, world!\n";
			size_t hw_len = strlen(hw) + 1; // null byte
			assert(buf.len >= hw_len);
			memcpy(buf.ptr, hw, hw_len);
			buf.len = hw_len;
			netcon_transmit_buf(con, &buf);
#endif
			// for(;;)
			usleep(1000000);
			// printf("Recv got :: %ld :: <%s>\n", ret, buf);
		}

		/*
		for(int i = 0; i < 1000; i++) {
		    netmgr_echo(mgr);
		}
		netmgr_wait_all_tx_complete(mgr);
		*/
	}
	netmgr_destroy(mgr);

#if 0
	struct secure_api api;
	if(twz_secure_api_open_name("/dev/nstack", &api)) {
		fprintf(stderr, "couldn't open network stack API\n");
		return 1;
	}

	struct nstack_open_ret ret;
	int r = nstack_open_client(&api, 0, "test-program", &ret);
	if(r) {
		fprintf(stderr, "failed to open client connection\n");
		return 1;
	}

	printf("got back object IDs from nstack:\n");
	printf("  txq  : " IDFMT "\n", IDPR(ret.txq_id));
	printf("  txbuf: " IDFMT "\n", IDPR(ret.txbuf_id));
	printf("  rxq  : " IDFMT "\n", IDPR(ret.rxq_id));
	printf("  rxbuf: " IDFMT "\n", IDPR(ret.rxbuf_id));

	struct nstack_queue_entry nqe;
	nqe.qe.info = 123;
	nqe.cmd = 456;

	twzobj txq_obj, rxq_obj;
	twz_object_init_guid(&txq_obj, ret.txq_id, FE_READ | FE_WRITE);
	twz_object_init_guid(&rxq_obj, ret.rxq_id, FE_READ | FE_WRITE);
	fprintf(stderr, "submitting %d\n", 123);
	queue_submit(&txq_obj, (struct queue_entry *)&nqe, 0);
	queue_get_finished(&txq_obj, (struct queue_entry *)&nqe, 0);
	fprintf(stderr, "got finished %d\n", nqe.qe.info);

	fprintf(stderr, "waiting for commands!\n");
	queue_receive(&rxq_obj, (struct queue_entry *)&nqe, 0);
	fprintf(stderr, "got %d; sending completion\n", nqe.qe.info);
	queue_complete(&rxq_obj, (struct queue_entry *)&nqe, 0);
	fprintf(stderr, "client done!\n");
#endif
}
