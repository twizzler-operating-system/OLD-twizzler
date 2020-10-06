#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <logboi/logboi.h>
#include <twz/bstream.h>
#include <twz/gate.h>
#include <twz/io.h>
#include <twz/name.h>
#include <twz/security.h>
#include <twz/thread.h>
#include <unistd.h>

#include <twz/debug.h>

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

struct client {
	const char *name;
	twzobj obj, thrdobj;
	int flags;
	std::thread *thread;
	size_t id;
};

std::atomic<size_t> next_client_id(0);

std::mutex m;
std::condition_variable cv;
std::vector<client *> clients_to_cleanup;

static bool drain_log(client *client)
{
	char buf[1024];
	ssize_t r;
	while((r = twzio_read(&client->obj, buf, sizeof(buf) - 2, 0, TWZIO_NONBLOCK)) > 0) {
		if(buf[r - 1] != '\n') {
			buf[r++] = '\n';
		}
		buf[r] = 0;
		printf("[%s]: %s", client->name, buf);
		fflush(stdout);
	}
	if(r == -EAGAIN) {
		r = 0;
	}
	if(r < 0) {
		fprintf(stdout, "[logboi] error reading log for %s: %ld\n", client->name, r);
	}
	return r >= 0;
}

void client_loop(client *client)
{
	char *name = NULL;
	asprintf(&name, "logboi-client: %s", client->name);
	twz_thread_set_name(name);
	free(name);

	struct twzthread_repr *repr = (struct twzthread_repr *)twz_object_base(&client->thrdobj);
	for(;;) {
		struct event ev[2];
		if(twzio_poll(&client->obj, TWZIO_EVENT_READ, &ev[0]) == 1) {
			if(!drain_log(client))
				break;
			continue;
		}

		event_init_other(&ev[1], &repr->syncs[THRD_SYNC_EXIT], 1);
		event_wait(2, ev, NULL);
		if(ev[1].result) {
			break;
		}
	}
	drain_log(client);

	{
		std::lock_guard<std::mutex> _lg(m);
		clients_to_cleanup.push_back(client);
	}
	cv.notify_all();
}

extern "C" {
DECLARE_SAPI_ENTRY(open_connection,
  LOGBOI_GATE_OPEN,
  int,
  const char *name,
  int flags,
  objid_t *arg)
{
	// fprintf(stderr, "Hello from logboi open: %p: %s\n", arg, name);
	struct client *client = (struct client *)malloc(sizeof(struct client));
	twz_object_new(&client->obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE);
	struct bstream_hdr *hdr = (struct bstream_hdr *)twz_object_base(&client->obj);
	bstream_obj_init(&client->obj, hdr, 12);
	client->name = strdup(name);
	client->flags = flags;
	*arg = twz_object_guid(&client->obj);
	client->thread = new std::thread(client_loop, client);
	client->id = ++next_client_id;
	twz_object_init_guid(&client->thrdobj, twz_thread_repr_base()->reprid, FE_READ);
	twz_object_wire(NULL, &client->thrdobj);
	return 12345;
}
}

int main(int argc, char **argv)
{
	twzobj api_obj;
	twz_object_new(&api_obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE);
	twz_secure_api_create(&api_obj, "test");
	struct secure_api_header *sah = (struct secure_api_header *)twz_object_base(&api_obj);

	twz_name_assign(twz_object_guid(&api_obj), "/dev/logboi");

	while(1) {
		std::unique_lock<std::mutex> _lg(m);
		if(clients_to_cleanup.size() == 0) {
			cv.wait(_lg);
		}
		client *c = clients_to_cleanup.back();
		if(c) {
			clients_to_cleanup.pop_back();
		}
		c->thread->join();
		delete c->thread;
		free((void *)c->name);
		free(c);
	}

	return 0;
}
