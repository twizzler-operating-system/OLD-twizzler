#include <stdio.h>

#include <twz/gate.h>

#include <logboi/logboi.h>

int main()
{
	twzobj api_obj;
	int r = twz_object_init_name(&api_obj, "/lb", FE_READ);
	if(r)
		abort();

	struct secure_api_header *hdr = twz_object_base(&api_obj);
	printf("calling logboi:: " IDFMT "     " IDFMT "\n", IDPR(hdr->view), IDPR(hdr->sctx));
	r = logboi_open_connection(hdr, NULL);
	printf("::: %d\n", r);
}
