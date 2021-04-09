#pragma once
#include <twz/sys.h>

#ifdef __cplusplus
#include <atomic>
using std::atomic_uint_least64_t;
extern "C" {
#else /* not __cplusplus */
#include <stdatomic.h>
#endif /* __cplusplus */

struct mutex {
	atomic_uint_least64_t sleep;
	atomic_uint_least64_t resetcode;
};

#define MUTEX_INIT                                                                                 \
	(struct mutex)                                                                                 \
	{                                                                                              \
		.sleep = 0;                                                                                \
	}

static inline void mutex_init(struct mutex *m)
{
	atomic_store(&m->sleep, 0);
}

void mutex_acquire(struct mutex *m);
void mutex_release(struct mutex *m);

#ifdef __cplusplus
}
#endif
