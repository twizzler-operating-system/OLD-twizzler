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
}
