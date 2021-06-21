#pragma once

#ifdef __cplusplus
#include <atomic>
extern "C" {
#else
#include <stdatomic.h>
#endif

struct mutex {
#ifdef __cplusplus
	std::atomic_uint_least64_t sleep;
	std::atomic_uint_least64_t resetcode;
#else
	atomic_uint_least64_t sleep;
	atomic_uint_least64_t resetcode;
#endif
};

#define MUTEX_INIT                                                                                 \
	(struct mutex)                                                                                 \
	{                                                                                              \
		.sleep = 0;                                                                                \
	}

static inline void mutex_init(struct mutex *m)
{
	atomic_store(&m->sleep, 0);
	atomic_store(&m->resetcode, 0);
}

void mutex_acquire(struct mutex *m);
void mutex_release(struct mutex *m);

#ifdef __cplusplus
}
#endif
