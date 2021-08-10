#include "v2.h"
#include <twz/obj.h>
#include <twz/sys/thread.h>
#include <twz/sys/view.h>

struct unix_server {
	struct secure_api api;
	_Atomic bool inited;
	_Atomic bool ok;
};
static struct unix_server userver = {};

static twzobj conn_obj;
static _Atomic bool conn_obj_init = false;
static _Atomic int conn_obj_lock = 0;
static _Atomic uint32_t conn_obj_max = 0;

#include <twz/mutex.h>
static uint32_t *idlist = NULL;
static size_t idlist_len, idlist_count;
static struct mutex idlist_lock;
static uint32_t id_max = 0;

static void resize_idlist(void)
{
	if(idlist_len == idlist_count) {
		idlist_len = idlist_len ? idlist_len * 2 : 8;
	}
	idlist = realloc(idlist, idlist_len * sizeof(uint32_t));
}

uint32_t get_new_twix_conn_id(void)
{
	mutex_acquire(&idlist_lock);
	if(idlist == NULL || idlist_count == 0) {
		mutex_release(&idlist_lock);
		return ++id_max;
	}
	return idlist[--idlist_count];
	mutex_release(&idlist_lock);
}

void release_twix_conn_id(uint32_t id)
{
	mutex_acquire(&idlist_lock);
	resize_idlist();
	idlist[idlist_count++] = id;
	mutex_release(&idlist_lock);
}

__attribute__((const)) uint32_t get_twix_thr_id(void)
{
	uint64_t gs;
	asm volatile("rdgsbase %0" : "=r"(gs)::"memory");
	uint32_t id = gs & (OBJ_MAXSIZE - 1);
	return id;
}

struct twix_conn *get_twix_conn(void)
{
	uint32_t id = get_twix_thr_id();
	// debug_printf("get_twix_conn id = %d (%d)\n", id, conn_obj_init);

	if(conn_obj_init == false) {
		if(atomic_exchange(&conn_obj_lock, 1) == 0) {
			twz_object_new(&conn_obj,
			  NULL,
			  NULL,
			  OBJ_VOLATILE,
			  TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_VIEW);
			twz_object_delete(&conn_obj, 0);
			conn_obj_init = true;
			conn_obj_lock = 0;
		} else {
			while(conn_obj_lock != 0)
				twz_thread_yield();
		}
	}

	struct twix_conn *conn = twz_object_base(&conn_obj);
	return &conn[id];
}
void resetup_queue(long is_thread)
{
	/* TODO do we need to serialize this? */
	// debug_printf("child: here! %ld\n", is_thread);
	// for(long i = 0; i < 100000; i++) {
	//	__syscall6(0, 0, 0, 0, 0, 0, 0);
	//}
	// debug_printf("child: done\n");

	twz_fault_set(FAULT_SIGNAL, __twix_signal_handler, NULL);
	// debug_printf("::: %d %p %p\n", is_thread, &userver, userver.api.hdr);
	if(!is_thread) {
		userver.ok = false;
		userver.inited = true;
	}
	objid_t qid, bid;

	if(!is_thread) {
		if(twz_secure_api_open_name("/dev/unix", &userver.api)) {
			abort();
		}
	}
	int r = twix_open_queue(&userver.api, 0, &qid, &bid);
	if(r) {
		abort();
	}

	// twix_log("reopen! " IDFMT "\n", IDPR(qid));

	// objid_t stateid;
	// if(twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_NONE, 0, 0, &stateid))
	// { 	abort();
	//}

	// twz_view_fixedset(NULL, TWZSLOT_UNIX, stateid, VE_READ | VE_WRITE | VE_FIXED | VE_VALID);
	// twz_object_init_ptr(&state_object, SLOT_TO_VADDR(TWZSLOT_UNIX));

	struct twix_conn *conn = get_twix_conn();
	conn->bid = bid;
	conn->qid = qid;
	// debug_printf(
	//  "resetup objects: " IDFMT " " IDFMT " " IDFMT "\n", IDPR(stateid), IDPR(bid), IDPR(qid));

	twz_view_set(NULL,
	  TWZSLOT_THREAD_OBJ(get_twix_thr_id(), TWZSLOT_THREAD_OFFSET_QUEUE),
	  conn->qid,
	  VE_READ | VE_WRITE);
	twz_view_set(NULL,
	  TWZSLOT_THREAD_OBJ(get_twix_thr_id(), TWZSLOT_THREAD_OFFSET_BUFFER),
	  conn->bid,
	  VE_READ | VE_WRITE);
	twz_object_init_ptr(&conn->cmdqueue,
	  SLOT_TO_VADDR(TWZSLOT_THREAD_OBJ(get_twix_thr_id(), TWZSLOT_THREAD_OFFSET_QUEUE)));
	twz_object_init_ptr(&conn->buffer,
	  SLOT_TO_VADDR(TWZSLOT_THREAD_OBJ(get_twix_thr_id(), TWZSLOT_THREAD_OFFSET_BUFFER)));

	// twz_object_tie_guid(twz_object_guid(&conn->cmdqueue), stateid, 0);
	// twz_object_delete_guid(stateid, 0);
	userver.ok = true;
	userver.inited = true;
	// debug_printf("AFTER SETUP %p\n", userver.api.hdr);
}

bool setup_queue(void)
{
	if(userver.inited)
		return userver.ok;
	userver.ok = false;
	userver.inited = true;
	uint32_t flags;
	objid_t qid, bid;
	twz_view_get(NULL, TWZSLOT_THREAD_OBJ(0, TWZSLOT_THREAD_OFFSET_BUFFER), &bid, &flags);
	twz_view_get(NULL, TWZSLOT_THREAD_OBJ(0, TWZSLOT_THREAD_OFFSET_QUEUE), &qid, &flags);
	bool already = (flags & VE_VALID) && qid && bid;
	if(twz_secure_api_open_name("/dev/unix", &userver.api)) {
		return false;
	}
	// debug_printf("Calling OPEN " IDFMT "\n", IDPR(twz_thread_repr_base()->reprid));
	if(!already) {
		int r = twix_open_queue(&userver.api, 0, &qid, &bid);
		if(r) {
			return false;
		}
	}

	// twz_object_init_ptr(&state_object, SLOT_TO_VADDR(TWZSLOT_UNIX));

	struct twix_conn *conn = get_twix_conn();
	conn->qid = qid;
	conn->bid = bid;
	// debug_printf("OPEN WITH " IDFMT "  " IDFMT "\n", IDPR(conn->qid), IDPR(conn->bid));

	twz_view_set(NULL,
	  TWZSLOT_THREAD_OBJ(get_twix_thr_id(), TWZSLOT_THREAD_OFFSET_QUEUE),
	  conn->qid,
	  VE_READ | VE_WRITE);
	twz_view_set(NULL,
	  TWZSLOT_THREAD_OBJ(get_twix_thr_id(), TWZSLOT_THREAD_OFFSET_BUFFER),
	  conn->bid,
	  VE_READ | VE_WRITE);
	twz_object_init_ptr(&conn->cmdqueue,
	  SLOT_TO_VADDR(TWZSLOT_THREAD_OBJ(get_twix_thr_id(), TWZSLOT_THREAD_OFFSET_QUEUE)));
	twz_object_init_ptr(&conn->buffer,
	  SLOT_TO_VADDR(TWZSLOT_THREAD_OBJ(get_twix_thr_id(), TWZSLOT_THREAD_OFFSET_BUFFER)));
	userver.ok = true;
	userver.inited = true;

#if 0
	static bool file_inited = false;
	/* TODO: is this necessary at all? */
	if(!file_inited && 0) {
		file_inited = true;
		for(int fd = 0; fd < MAX_FD; fd++) {
			struct file *file = twix_get_fd(fd);
			if(file) {
				objid_t id = twz_object_guid(&file->obj);
				debug_printf("REOPEN! %d " IDFMT "\n", fd, IDPR(id));
				struct twix_queue_entry tqe = build_tqe(TWIX_CMD_REOPEN_V1_FD,
				  0,
				  0,
				  5,
				  (long)fd,
				  ID_LO(id),
				  ID_HI(id),
				  (long)file->fcntl_fl | 3 /*TODO*/,
				  file->pos);
				twix_sync_command(&tqe);
			}
		}
	}
#endif
	twz_fault_set(FAULT_SIGNAL, __twix_signal_handler, NULL);

	return true;
}

bool twix_force_v2_retry(void)
{
	userver.inited = false;
	return setup_queue();
}
