#include <stdio.h>

#include <twz/gate.h>

#include <twix/twix.h>

#include <twz/io.h>
#include <twz/thread.h>

#include <twz/security.h>

#include <twz/queue.h>

int main()
{
	struct secure_api api;
	if(twz_secure_api_open_name("/dev/unix", &api)) {
		fprintf(stderr, "couldn't open twix API\n");
		return 1;
	}
	objid_t id = 0;
	int r = twix_open_queue(&api, 0, &id);
	printf("::: %d : " IDFMT "\n", r, IDPR(id));

	// printf("logtest thr id : " IDFMT "\n", IDPR(twz_thread_repr_base()->reprid));
	twzobj queue;
	twz_object_init_guid(&queue, id, FE_READ | FE_WRITE);

	struct twix_queue_entry tqe;
	tqe.x = 1234;
	queue_submit(&queue, (struct queue_entry *)&tqe, 0);
	queue_get_finished(&queue, (struct queue_entry *)&tqe, 0);

	printf("here\n");

	return 0;
}
