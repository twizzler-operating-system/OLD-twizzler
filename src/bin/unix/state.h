#pragma once

#include <twix/twix.h>
#include <twz/obj.h>
#include <twz/obj/hier.h>
#include <twz/obj/queue.h>
#include <twz/sys/obj.h>

#include <fcntl.h>
#include <unistd.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

class unixprocess;
class unixthread;

class filedesc
{
  private:
	std::mutex lock;
	bool inited = false;

  public:
	bool has_obj = false;
	filedesc(objid_t objid, size_t pos, int fcntl_flags)
	  : objid(objid)
	  , fcntl_flags(fcntl_flags)
	  , pos(pos)
	{
		int r = twz_object_init_guid(&obj, objid, FE_READ | FE_WRITE);
		(void)r; // TODO
		inited = true;
		has_obj = true;
	}
	filedesc()
	{
	}

	~filedesc()
	{
		if(inited && has_obj) {
			twz_object_release(&obj);
		}
	}

	int poll(uint64_t type, struct event *ev);

	bool is_nonblock(int flags);
	int init_path(std::shared_ptr<filedesc> at,
	  const char *path,
	  int _fcntl_flags,
	  int mode = 0,
	  int flags = 0);
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
	long ioctl(int cmd, void *buf);
	twzobj obj;
	objid_t objid;
	std::atomic<int> fcntl_flags;
	std::atomic<size_t> pos = { 0 };
	int d_type;
	std::string d_name;
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
	PROC_FORKED,
};

class queue_client;
class unixprocess
{
	class pwaiter
	{
	  public:
		std::shared_ptr<queue_client> client;
		twix_queue_entry tqe;
		int pid;

		pwaiter(std::shared_ptr<queue_client> _client, twix_queue_entry *tqe, int pid)
		  : client(_client)
		  , tqe(*tqe)
		  , pid(pid)
		{
		}
	};

  public:
	int pid, pgid;
	int gid, egid;
	int uid, euid;
	int exit_status;
	proc_state state = PROC_NORMAL;
	bool status_changed = false;

	std::vector<objid_t> objects_to_delete;

	std::vector<std::pair<std::shared_ptr<queue_client>, twix_queue_entry *>> state_waiters;

	std::vector<std::shared_ptr<unixthread>> threads;
	std::vector<descriptor> fds;

	std::shared_ptr<filedesc> cwd;

	std::shared_ptr<unixprocess> parent;
	std::vector<std::shared_ptr<unixprocess>> children;

	std::vector<pwaiter *> waiting_clients;

	std::mutex lock;

	unixprocess(std::shared_ptr<unixprocess> parent);

	void send_signal(int sig);

	std::pair<proc_state, int> wait_ready(std::shared_ptr<queue_client> client,
	  twix_queue_entry *tqe)
	{
		std::unique_lock<std::mutex> _lg(lock);
		if(state == PROC_FORKED) {
			state_waiters.push_back(std::make_pair(client, new twix_queue_entry(*tqe)));
		}
		return std::make_pair(state, exit_status);
	}

	void mark_ready();
	void change_status(proc_state state, int status);

	bool wait_for_child(std::shared_ptr<queue_client> client, twix_queue_entry *tqe, int pid);
	bool child_status_change(unixprocess *child, int status);

	bool wait(int *status)
	{
		bool ret;
		std::shared_ptr<unixprocess> _p = nullptr;
		{
			std::lock_guard<std::mutex> _lg(lock);
			if(state == PROC_FORKED) {
				ret = false;
			} else if(status_changed) {
				status_changed = false;
				*status = exit_status;
				if(state == PROC_EXITED) {
					_p = parent;
				}
				ret = true;
			}
		}
		if(_p)
			_p->child_died(pid);
		return ret;
	}

	void child_died(int pid)
	{
		for(size_t i = 0; i < children.size(); i++) {
			auto c = children[i];
			if(c->pid == pid) {
				children.erase(children.begin() + i);
			}
		}
	}

	size_t add_thread(std::shared_ptr<unixthread> thr)
	{
		std::lock_guard<std::mutex> _lg(lock);
		threads.push_back(thr);
		return threads.size() - 1;
	}

	void kill_all_threads(int);

	void remove_thread(size_t perproc_tid)
	{
		{
			std::lock_guard<std::mutex> _lg(lock);
			threads.erase(threads.begin() + perproc_tid);
		}
		if(threads.size() == 0 || perproc_tid == 0) {
			exit();
		}
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

	void close_fd(int fd)
	{
		std::lock_guard<std::mutex> _lg(lock);
		if(fds.size() <= (size_t)fd) {
			return;
		}
		fds[fd].desc = nullptr;
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

	~unixprocess()
	{
		for(auto id : objects_to_delete) {
			//	debug_printf("destroying object in process destroy: " IDFMT "\n", IDPR(id));
			twz_object_delete_guid(id, 0);
		}
		// fprintf(stderr, "process destructed %d\n", pid);
	}

  private:
	void exit();
};

#include <signal.h>
class queue_client;
class unixthread
{
  public:
	int tid;
	std::shared_ptr<unixprocess> parent_process;
	size_t perproc_id;

	std::shared_ptr<queue_client> client;

	unixthread(std::shared_ptr<unixprocess> parent);
	unixthread(int tid, std::shared_ptr<unixprocess> proc, std::shared_ptr<queue_client> client)
	  : tid(tid)
	  , parent_process(proc)
	  , client(client)
	{
	}

	bool is_leader()
	{
		return perproc_id == 0;
	}

	bool send_signal(int sig, bool);

	void exit()
	{
		parent_process->remove_thread(perproc_id);
		client = nullptr;
	}

	void kill();

	~unixthread()
	{
		// fprintf(stderr, "thread destructed %d\n", tid);
	}

	void suspend(twix_queue_entry *tqe)
	{
		std::lock_guard<std::mutex> _lg(lock);
		suspended_tqe = new twix_queue_entry(*tqe);
	}

	void resume();

  private:
	twix_queue_entry *suspended_tqe = nullptr;
	std::mutex lock;
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

	void exit();
	int init();
	~queue_client();
	void *buffer_base()
	{
		return twz_object_base(&buffer);
	}

	void complete(twix_queue_entry *tqe)
	{
		/* TODO: non blocking, ignore if exited... */
		// fprintf(stderr, "COMPLETING %d\n", tqe->qe.info);
		queue_complete(&queue, (struct queue_entry *)tqe, 0);
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

std::pair<int, std::shared_ptr<filedesc>> open_file(std::shared_ptr<filedesc> at,
  const char *path,
  int flags = 0);

std::shared_ptr<unixprocess> procs_lookup_forked(objid_t id);
void procs_insert_forked(objid_t id, std::shared_ptr<unixprocess> proc);
std::shared_ptr<unixprocess> process_lookup(int pid);
void thrds_insert_forked(objid_t id, std::shared_ptr<unixthread> thrd);

std::pair<long, bool> twix_cmd_open(std::shared_ptr<queue_client> client, twix_queue_entry *tqe);
std::pair<long, bool> twix_cmd_pio(std::shared_ptr<queue_client> client, twix_queue_entry *tqe);
std::pair<long, bool> twix_cmd_fcntl(std::shared_ptr<queue_client> client, twix_queue_entry *tqe);
std::pair<long, bool> twix_cmd_mmap(std::shared_ptr<queue_client> client, twix_queue_entry *tqe);
std::pair<long, bool> twix_cmd_stat(std::shared_ptr<queue_client> client, twix_queue_entry *tqe);
std::pair<long, bool> twix_cmd_getdents(std::shared_ptr<queue_client> client,
  twix_queue_entry *tqe);
std::pair<long, bool> twix_cmd_readlink(std::shared_ptr<queue_client> client,
  twix_queue_entry *tqe);
std::pair<long, bool> twix_cmd_clone(std::shared_ptr<queue_client> client, twix_queue_entry *tqe);
std::pair<long, bool> twix_cmd_send_signal(std::shared_ptr<queue_client> client,
  twix_queue_entry *tqe);
std::pair<long, bool> twix_cmd_faccessat(std::shared_ptr<queue_client> client,
  twix_queue_entry *tqe);
std::pair<long, bool> twix_cmd_dup(std::shared_ptr<queue_client> client, twix_queue_entry *tqe);
std::pair<long, bool> twix_cmd_poll(std::shared_ptr<queue_client> client, twix_queue_entry *tqe);
std::pair<long, bool> twix_cmd_ioctl(std::shared_ptr<queue_client> client, twix_queue_entry *tqe);

#define R_S(r) std::make_pair(r, true)
#define R_A(r) std::make_pair(r, false)
std::pair<long, bool> handle_command(std::shared_ptr<queue_client> client, twix_queue_entry *tqe);
int client_init(std::shared_ptr<queue_client> client);
