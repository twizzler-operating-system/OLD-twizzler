#include "state.h"
#include <twix/twix.h>

#include <memory>
#include <mutex>
#include <unordered_map>

static std::atomic_int next_taskid(2);

template<typename K, typename V>
class refmap
{
  private:
	std::unordered_map<K, std::shared_ptr<V>> map;
	std::mutex lock;

  public:
	std::shared_ptr<V> lookup(K k)
	{
		std::lock_guard<std::mutex> _lg(lock);
		auto it = map.find(k);
		if(it == map.end())
			return nullptr;
		else
			return (*it).second;
	}

	template<typename... Args>
	void insert(K k, Args... args)
	{
		std::lock_guard<std::mutex> _lg(lock);
		map.insert(std::make_pair(k, std::make_shared<V>(k, args...)));
	}

	void insert_existing(K k, std::shared_ptr<V> v)
	{
		std::lock_guard<std::mutex> _lg(lock);
		map.insert(std::make_pair(k, v));
	}

	void remove(K k)
	{
		std::lock_guard<std::mutex> _lg(lock);
		map.erase(k);
	}
};

static refmap<int, unixprocess> proctable;
static refmap<int, unixthread> thrtable;

static std::mutex forked_lock;
static std::unordered_map<objid_t, std::shared_ptr<unixprocess>> forked_procs;

std::shared_ptr<unixprocess> process_lookup(int pid)
{
	return proctable.lookup(pid);
}

void procs_insert_forked(objid_t id, std::shared_ptr<unixprocess> proc)
{
	std::lock_guard<std::mutex> _lg(forked_lock);
	forked_procs.insert(std::make_pair(id, proc));
}

std::shared_ptr<unixprocess> procs_lookup_forked(objid_t id)
{
	/* TODO: also need to clean up this map when a parent exits... */
	std::lock_guard<std::mutex> _lg(forked_lock);
	auto it = forked_procs.find(id);
	if(it == forked_procs.end()) {
		return nullptr;
	}

	auto ret = it->second;
	forked_procs.erase(it);
	return ret;
}

void unixprocess::exit()
{
	/* already locked */
	fprintf(stderr, "process exited!\n");
	state = PROC_EXITED;

	/* TODO: kill child threads */
	for(auto th : threads) {
		th->kill();
	}

	for(auto child : children) {
		/* handle parent process exit */
		child->parent = nullptr;
	}

	/* handle child process exit for parent */
	if(parent) {
		parent->child_died(pid);
	} else {
		proctable.remove(pid);
	}

	/* TODO: clean up */
	for(auto pw : waiting_clients) {
		//	delete pw;
	}
	waiting_clients.clear();
}

void unixprocess::kill_all_threads(int except)
{
	std::lock_guard<std::mutex> _lg(lock);
	for(auto t : threads) {
		if(except == -1 || (t->tid != except)) {
			t->kill();
		}
	}
}

void unixthread::kill()
{
	fprintf(stderr, "TODO: thread kill\n");
	sys_signal(twz_object_guid(&client->thrdobj), -1ul, SIGKILL, 0, 0);
	/* TODO: actually kill thread */
}
bool unixthread::send_signal(int sig, bool allow_pending)
{
	fprintf(stderr, "sending signal %d to %d\n", sig, tid);
	long r = sys_signal(twz_object_guid(&client->thrdobj), -1ul, sig, 0, 0);
	if(r) {
		fprintf(stderr, "WARNING: send_signal %ld\n", r);
	}
	resume();
	return true;
}

void unixprocess::send_signal(int sig)
{
	std::lock_guard<std::mutex> _lg(lock);
	if(threads.size() == 0)
		return;
	bool ok = false;
	for(auto thread : threads) {
		if(thread->send_signal(sig, false)) {
			ok = true;
			break;
		}
	}
	if(!ok) {
		threads[0]->send_signal(sig, true);
	}
}

unixprocess::unixprocess(std::shared_ptr<unixprocess> parent)
  : parent(parent)
{
	std::lock_guard<std::mutex> _lg(parent->lock);
	pid = next_taskid.fetch_add(1);
	fds = parent->fds;
	cwd = parent->cwd;
	state = PROC_FORKED;
	gid = parent->gid;
	uid = parent->uid;
}

bool unixprocess::wait_for_child(std::shared_ptr<queue_client> client,
  twix_queue_entry *tqe,
  int pid)
{
	std::lock_guard<std::mutex> _lg(lock);
	bool found = false;
	for(auto child : children) {
		int status;
		if(pid == -1 || pid == child->pid) {
			found = true;
			if(child->wait(&status)) {
				tqe->arg0 = status;
				tqe->ret = child->pid;
				client->complete(tqe);
				return true;
			}
		}
	}
	if(!found)
		return false;

	pwaiter *pw = new pwaiter(client, tqe, pid);
	waiting_clients.push_back(pw);

	return true;
}

bool unixprocess::child_status_change(unixprocess *child, int status)
{
	/* TODO: signal SIGCHLD? */
	std::lock_guard<std::mutex> _lg(lock);
	for(size_t i = 0; i < waiting_clients.size(); i++) {
		pwaiter *pw = waiting_clients[i];
		if(pw->pid == child->pid || pw->pid == -1) {
			pw->tqe.arg0 = status;
			pw->tqe.ret = child->pid;
			pw->client->complete(&pw->tqe);
			waiting_clients.erase(waiting_clients.begin() + i);
			delete pw;
			return true;
		}
	}

	return false;
}

int client_init(std::shared_ptr<queue_client> client)
{
	int r = twz_object_new(
	  &client->queue, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_NONE);
	if(r)
		return r;
	r = twz_object_new(
	  &client->buffer, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_NONE);
	if(r)
		return r;
	r = queue_init_hdr(
	  &client->queue, 12, sizeof(struct twix_queue_entry), 12, sizeof(struct twix_queue_entry));
	if(r)
		return r;

	std::shared_ptr<unixprocess> existing_proc =
	  procs_lookup_forked(twz_object_guid(&client->thrdobj));
	if(existing_proc != nullptr) {
		fprintf(stderr, "queue_client_init existing\n");
		client->proc = existing_proc;
		proctable.insert_existing(existing_proc->pid, existing_proc);
	} else {
		int taskid = next_taskid.fetch_add(1);
		proctable.insert(taskid);
		client->proc = proctable.lookup(taskid);
	}
	thrtable.insert(client->proc->pid, client->proc, client);
	client->thr = thrtable.lookup(client->proc->pid);
	client->thr->perproc_id = client->proc->add_thread(client->thr);

	if(!existing_proc) {
		auto [rr, desc] = open_file(nullptr, "/");
		if(rr) {
			return rr;
		}
		client->proc->cwd = desc;
	}

	client->proc->mark_ready();

	return 0;
}

void unixthread::resume()
{
	std::lock_guard<std::mutex> _lg(lock);
	if(suspended_tqe) {
		client->complete(suspended_tqe);
		suspended_tqe = nullptr;
	}
}

void unixprocess::change_status(proc_state _state, int status)
{
	std::lock_guard<std::mutex> _lg(lock);
	proc_state old_state = state;
	state = _state;
	exit_status = status;
	for(auto w : state_waiters) {
		w.first->complete(w.second);
		delete w.second;
	}
	state_waiters.clear();
	if(old_state != PROC_FORKED && parent) {
		parent->child_status_change(this, status);
	}
}

void unixprocess::mark_ready()
{
	change_status(PROC_NORMAL, 0);
}

void queue_client::exit()
{
	fprintf(stderr, ":::: %p\n", thr.get());
	thr->exit();
}

queue_client::~queue_client()
{
	fprintf(stderr, "client destructed! (tid %d)\n", thr->tid);
	twz_object_delete(&queue, 0);
	twz_object_delete(&buffer, 0);
	twz_object_unwire(NULL, &thrdobj);
	thrtable.remove(thr->tid);
	// thr->exit();
}
