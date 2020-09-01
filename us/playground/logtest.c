#include <stdio.h>

#include <twz/gate.h>

#include <logboi/logboi.h>

#include <twz/io.h>
#include <twz/thread.h>

int main()
{
	struct secure_api api;
	if(twz_secure_api_open_name("/dev/logboi", &api)) {
		fprintf(stderr, "couldn't open logboi API\n");
		return 1;
	}
	objid_t id = 0;
	int r = logboi_open_connection(&api, "test-program", 0, &id);
	// printf("::: %d : " IDFMT "\n", r, IDPR(id));

	// printf("logtest thr id : " IDFMT "\n", IDPR(twz_thread_repr_base()->reprid));
	twzobj logbuf;
	twz_object_init_guid(&logbuf, id, FE_READ | FE_WRITE);
	ssize_t rr = twzio_write(&logbuf, "logging test!\n", 14, 0, 0);
	sleep(1);
	rr = twzio_write(&logbuf, "another logging test!\n", 22, 0, 0);
	// sleep(1);
}
