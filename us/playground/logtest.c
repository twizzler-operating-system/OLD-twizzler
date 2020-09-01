#include <stdio.h>

#include <twz/gate.h>

#include <logboi/logboi.h>

#include <twz/io.h>
#include <twz/thread.h>

int main()
{
	twzobj api_obj;
	int r = twz_object_init_name(&api_obj, "/dev/logboi", FE_READ);
	if(r)
		abort();

	struct secure_api_header *hdr = twz_object_base(&api_obj);
	fprintf(stderr, "calling logboi:: " IDFMT "     " IDFMT "\n", IDPR(hdr->view), IDPR(hdr->sctx));
	objid_t id = 0;
	r = logboi_open_connection(hdr, "test-program", 0, &id);
	printf("::: %d : " IDFMT "\n", r, IDPR(id));

	printf("logtest thr id : " IDFMT "\n", IDPR(twz_thread_repr_base()->reprid));
	twzobj logbuf;
	twz_object_init_guid(&logbuf, id, FE_READ | FE_WRITE);
	ssize_t rr = twzio_write(&logbuf, "logging test!\n", 14, 0, 0);
	printf("wrote stuff: %ld\n", rr);
	sleep(1);
	rr = twzio_write(&logbuf, "another logging test!\n", 22, 0, 0);
	// sleep(1);
}
