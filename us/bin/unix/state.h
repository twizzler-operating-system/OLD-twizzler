#pragma once

#include <twz/obj.h>
#include <twz/queue.h>

struct twix_queue_entry;

class queue_client
{
  public:
	twzobj queue, thrdobj, buffer;
	queue_client()
	{
	}

	long handle_command(struct twix_queue_entry *);

	int init()
	{
		int r =
		  twz_object_new(&queue, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_NONE);
		if(r)
			return r;
		r = twz_object_new(
		  &buffer, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_NONE);
		if(r)
			return r;
		r = queue_init_hdr(
		  &queue, 12, sizeof(struct twix_queue_entry), 12, sizeof(struct twix_queue_entry));
		return r;
	}
	~queue_client()
	{
		twz_object_delete(&queue, 0);
		twz_object_delete(&buffer, 0);
	}
};
