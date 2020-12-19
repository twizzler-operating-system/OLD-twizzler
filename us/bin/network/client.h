#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <twz/obj.h>
#include <twz/queue.h>

#include <nstack/nstack.h>

template<typename T>
class id_allocator
{
  private:
	std::vector<T> stack;
	std::mutex m;
	T max = 0;

  public:
	T get()
	{
		std::lock_guard<std::mutex> _lg(m);
		if(stack.size() == 0) {
			return max++;
		}
		T t = stack.back();
		stack.pop_back();
		return t;
	}

	void put(T t)
	{
		std::lock_guard<std::mutex> _lg(m);
		stack.push_back(t);
	}
};

class net_client;
class outstanding_command
{
  public:
	uint32_t id;
	std::shared_ptr<net_client> client;
	struct nstack_queue_entry nqe;
	void (*fn)(std::shared_ptr<net_client>, struct nstack_queue_entry *, void *);
	void *data;

	outstanding_command(uint32_t _id,
	  std::shared_ptr<net_client> client,
	  struct nstack_queue_entry *nqe,
	  void (*fn)(std::shared_ptr<net_client>, struct nstack_queue_entry *, void *),
	  void *data)
	  : id(_id)
	  , client(client)
	  , nqe(*nqe)
	  , fn(fn)
	  , data(data)
	{
	}

	void complete()
	{
		if(fn) {
			fn(client, &nqe, data);
		}
	}
};

#include <stdio.h>
#include <thread>

class tcp_endpoint;
class tcp_rendezvous;

class net_client
{
  public:
	class connection
	{
		enum state {
			NONE,
			WAITING_CONNECT,
			CONNECTED,
		};

	  public:
		std::atomic<enum state> state;
		std::mutex lock;
		uint16_t id;
		std::shared_ptr<net_client> client;
		std::shared_ptr<tcp_endpoint> endp;
		std::thread thrd;
		nstack_queue_entry *connect_nqe;
		connection(std::shared_ptr<net_client> client,
		  uint16_t id,
		  std::shared_ptr<tcp_endpoint> endp);
		/* TODO: release the ID when tearing down */

		int bind(struct netaddr *na);
		int connect(nstack_queue_entry *);
		void complete_connection();
		void notify_accept();
		int accept(uint16_t *);
		ssize_t send(std::shared_ptr<net_client>, nstack_queue_entry *, void *, size_t);
		void recv_thrd();
	};

	twzobj txq_obj, rxq_obj;
	twzobj txbuf_obj, rxbuf_obj;
	twzobj thrdobj;
	size_t testing_rxb_off = OBJ_NULLPAGE_SIZE; // TODO: make an actual allocation system for rxbuf

	std::mutex lock;

	int flags;
	std::string name;

	std::unordered_map<uint32_t, std::shared_ptr<outstanding_command>> outstanding;
	id_allocator<uint32_t> outstanding_idalloc;
	id_allocator<uint16_t> conn_idalloc;

	std::unordered_map<uint16_t, std::shared_ptr<connection>> conns;

	std::shared_ptr<tcp_rendezvous> listener = nullptr;

	void insert_connection(std::shared_ptr<tcp_endpoint> endp);

	uint16_t create_connection(std::shared_ptr<net_client> client, int tcp_id = -1)
	{
		std::lock_guard<std::mutex> _lg(lock);
		uint16_t id = conn_idalloc.get();
		if(id != 0xffff)
			conns.insert(std::make_pair(id, std::make_shared<connection>(client, id, nullptr)));
		return id;
	}

	std::shared_ptr<connection> get_connection(uint16_t id)
	{
		std::lock_guard<std::mutex> _lg(lock);
		auto it = conns.find(id);
		if(it == conns.end())
			return nullptr;
		return it->second;
	}

	void remove_connection(uint16_t id)
	{
		std::lock_guard<std::mutex> _lg(lock);
		conns.erase(id);
	}

	void push_outstanding(std::shared_ptr<outstanding_command> cmd, uint32_t id)
	{
		std::lock_guard<std::mutex> _lg(lock);
		outstanding.insert(std::make_pair(id, cmd));
	}

	void complete(struct nstack_queue_entry *nqe)
	{
		/* TODO: nonblock, etc */
		if(queue_complete(&txq_obj, (struct queue_entry *)nqe, QUEUE_NONBLOCK)) {
			fprintf(stderr, "WARNING - completion would have blocked\n");
		}
	}

	std::shared_ptr<outstanding_command> pop_outstanding(uint32_t id)
	{
		std::lock_guard<std::mutex> _lg(lock);
		auto it = outstanding.find(id);
		if(it == outstanding.end()) {
			return nullptr;
		}
		return it->second;
	}

	net_client(int _flags, const char *_name)
	  : flags(_flags)
	  , name(_name)
	{
	}

	int init_objects();
	void enqueue_cmd(struct nstack_queue_entry *nqe);

	/* TODO: implement destructor */
	~net_client()
	{
		fprintf(stderr, "net_client destructed\n");
	}

  private:
};

int client_handlers_init();
bool handle_command(std::shared_ptr<net_client>, struct nstack_queue_entry *nqe);
void handle_completion(std::shared_ptr<net_client>, struct nstack_queue_entry *nqe);
