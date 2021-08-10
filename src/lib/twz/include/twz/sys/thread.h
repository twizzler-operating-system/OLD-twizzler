#pragma once

#ifdef __cplusplus
#include <atomic>
using std::atomic_uint_least64_t;
#else /* not __cplusplus */
#include <stdatomic.h>
#endif /* __cplusplus */

#include <twz/objid.h>
#include <twz/sys/fault.h>
#include <twz/sys/kso.h>
#include <twz/sys/view.h>

#ifdef __cplusplus
extern "C" {
#endif

#define THRD_SYNCPOINTS 128

#define THRD_SYNC_STATE 0
#define THRD_SYNC_READY 1
#define THRD_SYNC_EXIT 2

enum {
	THRD_SYNC_STATE_RUNNING,
	THRD_SYNC_STATE_BLOCKED,
};

#define TWZ_THRD_MAX_SCS 32

struct twzthread_repr {
	struct kso_hdr hdr;
	objid_t reprid;
	atomic_uint_least64_t syncs[THRD_SYNCPOINTS];
	uint64_t syncinfos[THRD_SYNCPOINTS];
	struct kso_attachment attached[TWZ_THRD_MAX_SCS];
};

#ifndef __KERNEL__

void twz_thread_yield(void);
void twz_thread_set_name(const char *name);
__attribute__((noreturn)) void twz_thread_exit(uint64_t ecode);
struct twzthread_repr *twz_thread_repr_base(void);
#endif

#ifdef __cplusplus
}
#endif
