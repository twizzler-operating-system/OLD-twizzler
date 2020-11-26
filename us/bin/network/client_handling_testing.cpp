#include "client.h"

void net_client::enqueue_cmd(struct nstack_queue_entry *nqe)
{
	/* TODO: handle error, non-blocking (?) */
	queue_submit(&rxq_obj, (struct queue_entry *)nqe, 0);
}

void submit_command(std::shared_ptr<net_client> client,
  struct nstack_queue_entry *nqe,
  void (*fn)(std::shared_ptr<net_client>, struct nstack_queue_entry *, void *),
  void *data)
{
	uint32_t id = client->outstanding_idalloc.get();
	auto cmd = std::make_shared<outstanding_command>(id, client, nqe, fn, data);
	nqe->qe.info = id;
	fprintf(stderr, "submitting command to client %s: %d\n", client->name.c_str(), id);
	client->push_outstanding(cmd, id);
	client->enqueue_cmd(nqe);
}

static void __callback_complete(std::shared_ptr<net_client> client,
  struct nstack_queue_entry *nqe = nullptr,
  void *data = nullptr)
{
	(void)nqe;
	struct nstack_queue_entry *tc = (struct nstack_queue_entry *)data;
	client->complete(tc);
	delete tc;
}

bool handle_command(std::shared_ptr<net_client> client, struct nstack_queue_entry *nqe)
{
	fprintf(stderr,
	  "handling command from client %s: %d %d\n",
	  client->name.c_str(),
	  nqe->qe.info,
	  nqe->cmd);

	switch(nqe->cmd) {
		case NSTACK_CMD_CONNECT: {
			fprintf(stderr, "creating conn\n");
			uint16_t cid = client->create_connection();
			fprintf(stderr, "created connection %d\n", cid);
			if(cid == 0xffff) {
				client->remove_connection(cid);
				nqe->ret = -ENOMEM; // TODO
				return true;
			}
			nqe->ret = cid;
			return true;
		} break;

		case NSTACK_CMD_SEND: {
			fprintf(stderr, "doing send\n");
			struct nstack_queue_entry newnqe = *nqe;
			newnqe.cmd = NSTACK_CMD_RECV;
			size_t off = client->testing_rxb_off;
			client->testing_rxb_off += nqe->data_len;
			char *buf = (char *)twz_object_base(&client->rxbuf_obj);
			buf += off;
			memcpy(buf,
			  twz_object_lea(&client->txq_obj, nqe->data_ptr),
			  nqe->data_len); // should sanity check ptr
			newnqe.data_ptr = twz_ptr_swizzle(&client->rxq_obj, buf, FE_READ | FE_WRITE);
			submit_command(client, &newnqe, nullptr, nullptr);
			fprintf(stderr, "send done\n");
			return true;
		} break;
	}

	/* testing */
	// struct nstack_queue_entry newnqe;
	// newnqe.cmd = 789;
	// submit_command(client, &newnqe);

	return true;
}

void handle_completion(std::shared_ptr<net_client> client, struct nstack_queue_entry *nqe)
{
	std::shared_ptr<outstanding_command> cmd = client->pop_outstanding(nqe->qe.info);
	if(cmd == nullptr) {
		return;
	}

	cmd->complete();

	fprintf(
	  stderr, "got notified of completion from client %s: %d\n", client->name.c_str(), cmd->id);
	client->outstanding_idalloc.put(cmd->id);
}
