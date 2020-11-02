#pragma once

#include <twix/twix.h>
#include <twz/obj.h>
#include <twz/queue.h>

#include <fcntl.h>
#include <unistd.h>

#include <memory>
#include <mutex>
#include <vector>

class unixprocess;
class unixthread;

class filedesc
{
  private:
	std::mutex lock;

  public:
	filedesc(objid_t objid, size_t pos, int fcntl_flags)
	  : objid(objid)
	  , fcntl_flags(fcntl_flags)
	  , pos(pos)
	{
		int r = twz_object_init_guid(&obj, objid, FE_READ | FE_WRITE);
		(void)r; // TODO
	}
	filedesc()
	{
	}

	~filedesc()
	{
		twz_object_release(&obj);
	}

	int init_path(const char *path, int _fcntl_flags, int mode = 0);
	bool access(int mode)
	{
		std::lock_guard<std::mutex> _lg(lock);
		bool ok = true;
		ok = ok && (!(mode & R_OK) || (fcntl_flags & (O_RDONLY + 1)));
		ok = ok && (!(mode & W_OK) || (fcntl_flags & (O_WRONLY + 1)));
		return ok;
	}
	ssize_t read(void *, size_t, off_t, int, bool);
	ssize_t write(const void *, size_t, off_t, int, bool);
	twzobj obj;
	objid_t objid;
	std::atomic<int> fcntl_flags;
	std::atomic<size_t> pos = 0;
};

class descriptor
{
  public:
	descriptor(std::shared_ptr<filedesc> desc, int flags)
	  : desc(desc)
	  , flags(flags)
	{
	}
	descriptor()
	{
		desc = nullptr;
		flags = 0;
	}
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

	int get_file_flags(int fd)
	{
		std::lock_guard<std::mutex> _lg(lock);
		if(fds.size() <= (size_t)fd || fd < 0 || fds[fd].desc == nullptr) {
			return -EBADF;
		}
		return fds[fd].flags;
	}

	int set_file_flags(int fd, int flags)
	{
		std::lock_guard<std::mutex> _lg(lock);
		if(fds.size() <= (size_t)fd || fd < 0 || fds[fd].desc == nullptr) {
			return -EBADF;
		}
		fds[fd].flags = flags;
		return 0;
	}

	void steal_fd(int fd, std::shared_ptr<filedesc> desc, int flags)
	{
		std::lock_guard<std::mutex> _lg(lock);
		if(fds.size() <= (size_t)fd) {
			fds.resize(fd + 1);
		}
		fds[fd].desc = desc;
		fds[fd].flags = flags;
	}

	int assign_fd(std::shared_ptr<filedesc> desc, int flags)
	{
		std::lock_guard<std::mutex> _lg(lock);
		for(size_t i = 0; i < fds.size(); i++) {
			if(fds[i].desc == nullptr) {
				fds[i].desc = desc;
				fds[i].flags = flags;
				return i;
			}
		}
		fds.push_back(descriptor(desc, flags));
		return fds.size() - 1;
	}

	std::shared_ptr<filedesc> get_file(int fd)
	{
		std::lock_guard<std::mutex> _lg(lock);
		if(fds.size() <= (size_t)fd || fd < 0) {
			return nullptr;
		}
		return fds[fd].desc;
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

	auto buffer_to_string(size_t buflen)
	{
		struct ret {
			bool ok;
			std::string str;
		};
		if(buflen > OBJ_TOPDATA / 2) {
			return ret{ false, "" };
		}
		return ret{ true, std::string((const char *)buffer_base(), buflen) };
	}

	template<typename T>
	void write_buffer(T *t, size_t off = 0)
	{
		void *base = (void *)((char *)buffer_base() + off);
		memcpy(base, t, sizeof(T));
	}
};
