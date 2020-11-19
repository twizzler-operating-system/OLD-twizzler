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

class outstanding_command
{
  public:
	uint32_t id;

	outstanding_command(uint32_t _id)
	  : id(_id)
	{
	}
};

#include <stdio.h>
class net_client
{
  public:
	twzobj txq_obj, rxq_obj;
	twzobj txbuf_obj, rxbuf_obj;
	twzobj thrdobj;

	std::mutex lock;

	int flags;
	std::string name;

	std::unordered_map<uint32_t, std::shared_ptr<outstanding_command>> outstanding;
	id_allocator<uint32_t> outstanding_idalloc;

	void push_outstanding(std::shared_ptr<outstanding_command> cmd, uint32_t id)
	{
		std::lock_guard<std::mutex> _lg(lock);
		outstanding.insert(std::make_pair(id, cmd));
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
