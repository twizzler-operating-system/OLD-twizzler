#include "databuf.h"
#include "client.h"

void databuf::entry::complete()
{
	/* TODO: be able to complete any queue */
	if(client)
		client->complete(&nqe);
}
