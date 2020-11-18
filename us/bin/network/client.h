#pragma once

#include <memory>
#include <string>
#include <twz/obj.h>

class net_client
{
  public:
	twzobj txq_obj, rxq_obj;
	twzobj txbuf_obj, rxbuf_obj;
	twzobj thrdobj;

	int flags;
	std::string name;

	net_client(int _flags, const char *_name)
	  : flags(_flags)
	  , name(_name)
	{
	}

	int init_objects();

	/* TODO: implement destructor */
  private:
};

int client_handlers_init();
bool handle_command(std::shared_ptr<net_client>, struct nstack_queue_entry *nqe);
