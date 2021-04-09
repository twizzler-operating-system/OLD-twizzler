#include "databuf.h"
#include "client.h"

void databuf::entry::complete()
{
	fprintf(stderr, "COMPLETE! %p\n", client.get());
	/* TODO: be able to complete any queue */
	if(client)
		client->complete(&nqe);
}
