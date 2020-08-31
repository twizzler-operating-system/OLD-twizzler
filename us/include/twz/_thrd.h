#pragma once

#ifdef __cplusplus
#include <atomic>
using std::atomic_uint_least64_t;
#else /* not __cplusplus */
#include <stdatomic.h>
#endif /* __cplusplus */

#include <twz/_fault.h>
#include <twz/_kso.h>
#include <twz/_objid.h>
#include <twz/_view.h>

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

struct twzthread_ctrl_repr {
	struct kso_hdr hdr;
	objid_t reprid, ctrl_reprid;
	struct viewentry fixed_points[];
};
