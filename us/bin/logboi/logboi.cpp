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

#include <thread>
#include <vector>

struct client {
	const char *name;
	twzobj obj;
	int flags;
	std::thread *thread;
	size_t id;
};

std::atomic<size_t> next_client_id(0);
std::vector<client *> clients;

void client_loop(client *client)
{
	twz_thread_set_name("log-client");
	printf("logboi started client :: " IDFMT "\n", IDPR(twz_object_guid(&client->obj)));

	char buf[1024];
	for(;;) {
		ssize_t r = twzio_read(&client->obj, buf, sizeof(buf), 0, 0);
		if(r < 0) {
			fprintf(stdout, "[logboi] error reading %ld\n", r);
			continue;
		}
		buf[r] = 0;
		printf("logboi log: %ld: %s\n", r, buf);
	}
}

extern "C" {
DECLARE_SAPI_ENTRY(open_connection,
  LOGBOI_GATE_OPEN,
  int,
  const char *name,
  int flags,
  objid_t *arg)
{
	// printf("Hello from logboi open: %p: %s\n", arg, name);
	struct client *client = (struct client *)malloc(sizeof(struct client));
	twz_object_new(&client->obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE);
	struct bstream_hdr *hdr = (struct bstream_hdr *)twz_object_base(&client->obj);
	bstream_obj_init(&client->obj, hdr, 12);
	client->name = strdup(name);
	client->flags = flags;
	*arg = twz_object_guid(&client->obj);
	client->thread = new std::thread(client_loop, client);
	client->id = ++next_client_id;
	clients.push_back(client);
	return 0;
}
}

void test()
{
	while(1)
		sleep(1);
}

int main(int argc, char **argv)
{
	printf("Hello \n");

	twzobj api_obj;
	twz_object_new(&api_obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE);
	twz_secure_api_create(&api_obj, "test");
	struct secure_api_header *sah = (struct secure_api_header *)twz_object_base(&api_obj);

	// printf("logboi setup: " IDFMT "    " IDFMT "\n", IDPR(sah->view), IDPR(sah->sctx));

	twz_name_assign(twz_object_guid(&api_obj), "/lb");

	while(1) {
		sleep(1);
	}

	return 0;
}
