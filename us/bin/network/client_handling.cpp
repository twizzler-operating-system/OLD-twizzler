#include "client.h"

void net_client::enqueue_cmd(struct nstack_queue_entry *nqe)
{
	/* TODO: handle error, non-blocking (?) */
	queue_submit(&rxq_obj, (struct queue_entry *)nqe, 0);
}

void submit_command(std::shared_ptr<net_client> client, struct nstack_queue_entry *nqe)
{
	uint32_t id = client->outstanding_idalloc.get();
	auto cmd = std::make_shared<outstanding_command>(id);
	nqe->qe.info = id;
	fprintf(stderr, "submitting command to client %s: %d\n", client->name.c_str(), id);
	client->push_outstanding(cmd, id);
	client->enqueue_cmd(nqe);
}

bool handle_command(std::shared_ptr<net_client> client, struct nstack_queue_entry *nqe)
{
	fprintf(stderr,
	  "handling command from client %s: %d %d\n",
	  client->name.c_str(),
	  nqe->qe.info,
	  nqe->cmd);

	/* testing */
	struct nstack_queue_entry newnqe;
	newnqe.cmd = 789;
	submit_command(client, &newnqe);

	return true;
}

void handle_completion(std::shared_ptr<net_client> client, struct nstack_queue_entry *nqe)
{
	std::shared_ptr<outstanding_command> cmd = client->pop_outstanding(nqe->qe.info);
	if(cmd == nullptr) {
		return;
	}

	fprintf(
	  stderr, "got notified of completion from client %s: %d\n", client->name.c_str(), cmd->id);
	client->outstanding_idalloc.put(cmd->id);
}
