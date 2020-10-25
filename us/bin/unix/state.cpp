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

	void remove(K k)
	{
		std::lock_guard<std::mutex> _lg(lock);
		map.erase(k);
	}
};

static refmap<int, unixprocess> proctable;
static refmap<int, unixthread> thrtable;
static refmap<int, filedesc> filetable;

int queue_client::init()
{
	int r =
	  twz_object_new(&queue, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_NONE);
	if(r)
		return r;
	r = twz_object_new(&buffer, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_NONE);
	if(r)
		return r;
	r = queue_init_hdr(
	  &queue, 12, sizeof(struct twix_queue_entry), 12, sizeof(struct twix_queue_entry));
	if(r)
		return r;

	int taskid = next_taskid.fetch_add(1);
	proctable.insert(taskid, 1);
	proc = proctable.lookup(taskid);
	thrtable.insert(taskid, proc);
	thr = thrtable.lookup(taskid);
	proc->add_thread(thr);

	return 0;
}

queue_client::~queue_client()
{
	twz_object_delete(&queue, 0);
	twz_object_delete(&buffer, 0);
	twz_object_unwire(NULL, &thrdobj);
	thrtable.remove(thr->tid);
	proc->state = PROC_EXITED;
	thr->exit();

	if(proc->ppid == 0) {
		proctable.remove(proc->pid);
	}
}
