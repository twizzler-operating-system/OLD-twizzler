#include <cstdio>
#include <logboi/logboi.h>
#include <twz/gate.h>
#include <twz/name.h>
#include <twz/security.h>
#include <unistd.h>

extern "C" {
DECLARE_SAPI_ENTRY(open_connection, LOGBOI_GATE_OPEN, int, objid_t *arg)
{
	printf("Hello from logboi open: %p\n", arg);
	*arg = 0x12345678;
	printf("LB:: ID:::  = " IDFMT "\n", IDPR(*arg));
	return 1234;
}
}

int main(int argc, char **argv)
{
	printf("Hello \n");

	twzobj api_obj;
	twz_object_new(&api_obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE);
	twz_secure_api_create(&api_obj, "test");
	struct secure_api_header *sah = (struct secure_api_header *)twz_object_base(&api_obj);

	printf("logboi setup: " IDFMT "    " IDFMT "\n", IDPR(sah->view), IDPR(sah->sctx));

	twz_name_assign(twz_object_guid(&api_obj), "/lb");

	while(1) {
		sleep(1);
	}

	return 0;
}
