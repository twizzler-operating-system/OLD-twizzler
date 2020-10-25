#pragma once

#include <twix/twix.h>
#include <twz/obj.h>
#include <twz/queue.h>

#include <memory>
#include <mutex>
#include <vector>

class unixprocess;
class unixthread;

class filedesc
{
  public:
	filedesc(int id, objid_t objid)
	  : id(id)
	  , objid(objid)
	{
	}
	int id;
	twzobj obj;
	objid_t objid;
	int flags;
	size_t pos;
};

class descriptor
{
  public:
	std::shared_ptr<filedesc> desc;
	int flags;
};

enum proc_state {
	PROC_NORMAL,
	PROC_EXITED,
};

class unixprocess
{
  public:
	int pid;
	int gid;
	int uid;
	int ppid;
	proc_state state = PROC_NORMAL;
	std::vector<std::shared_ptr<unixthread>> threads;
	std::vector<descriptor> fds;

	std::mutex lock;

	size_t add_thread(std::shared_ptr<unixthread> thr)
	{
		std::lock_guard<std::mutex> _lg(lock);
		threads.push_back(thr);
		return threads.size() - 1;
	}

	void remove_thread(size_t perproc_tid)
	{
		std::lock_guard<std::mutex> _lg(lock);
		threads.erase(threads.begin() + perproc_tid);
	}

	unixprocess(int pid)
	  : pid(pid)
	{
	}

	unixprocess(int pid, int ppid)
	  : pid(pid)
	  , ppid(ppid)
	{
	}

	~unixprocess()
	{
		fprintf(stderr, "process destructed %d\n", pid);
	}
};

class unixthread
{
  public:
	int tid;
	std::shared_ptr<unixprocess> parent_process;
	size_t perproc_id;

	unixthread(int tid, std::shared_ptr<unixprocess> proc)
	  : tid(tid)
	  , parent_process(proc)
	{
	}

	void exit()
	{
		parent_process->remove_thread(perproc_id);
	}

	~unixthread()
	{
		fprintf(stderr, "thread destructed %d\n", tid);
	}
};

class queue_client
{
  public:
	twzobj queue, thrdobj, buffer;
	std::shared_ptr<unixprocess> proc;
	std::shared_ptr<unixthread> thr;
	queue_client()
	{
	}

	long handle_command(struct twix_queue_entry *);

	int init();
	~queue_client();
	void *buffer_base()
	{
		return twz_object_base(&buffer);
	}

	template<typename T>
	void write_buffer(T *t)
	{
		void *base = buffer_base();
		memcpy(base, t, sizeof(T));
	}
};
