#pragma once

#include <memory>
#include <mutex>
#include <nstack/nstack.h>
#include <queue>

class net_client;

class databuf
{
  public:
	class entry
	{
	  public:
		std::shared_ptr<net_client> client;
		nstack_queue_entry nqe;
		void *ptr;
		size_t len;
		size_t off = 0;
		entry(std::shared_ptr<net_client> client, nstack_queue_entry *nqe, void *ptr, size_t len)
		  : client(client)
		  , nqe(nqe ? *nqe : nstack_queue_entry{})
		  , ptr(ptr)
		  , len(len)
		{
		}

		size_t rem()
		{
			return len - off;
		}

		void *next_ptr()
		{
			void *ret = (void *)((char *)ptr + off);
			return ret;
		}

		void complete();

		bool consume(size_t tlen)
		{
			off += tlen;
			if(off >= len) {
				complete();
				return true;
			}
			return false;
		}
	};

	class databufptr
	{
	  public:
		void *ptr;
		size_t len;
		databufptr(void *ptr, size_t len)
		  : ptr(ptr)
		  , len(len)
		{
		}
	};

  private:
	std::mutex lock;
	std::queue<entry *> entries;

  public:
	void append(std::shared_ptr<net_client> client, nstack_queue_entry *nqe, void *ptr, size_t len)
	{
		std::lock_guard<std::mutex> _lg(lock);
		entries.push(new entry(client, nqe, ptr, len));
	}

	databufptr get_next(size_t max)
	{
		std::lock_guard<std::mutex> _lg(lock);
		if(entries.size() == 0)
			return databufptr(NULL, 0);
		entry *entry = entries.front();
		size_t amount = max;
		if(amount > entry->rem())
			amount = entry->rem();
		databufptr dbp(entry->next_ptr(), amount);
		return dbp;
	}

	void remove(size_t len)
	{
		std::lock_guard<std::mutex> _lg(lock);
		while(len > 0) {
			assert(entries.size() > 0);
			entry *entry = entries.front();
			size_t thislen = len;
			if(thislen > entry->rem())
				thislen = entry->rem();
			if(entry->consume(thislen)) {
				entries.pop();
				delete entry;
			}
			len = -thislen;
		}
	}
};
