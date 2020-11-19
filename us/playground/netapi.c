#include <stdio.h>

#include <twz/gate.h>
#include <twz/obj.h>

#include <nstack/nstack.h>

int main()
{
	printf("Hello, World!\n");

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
}
