#pragma once

#include <twz/_fault.h>
#include <twz/_objid.h>
#include <twz/_view.h>

#define THRD_SYNCPOINTS 128

struct twzthread_repr {
	objid_t reprid;
	uint64_t syncs[THRD_SYNCPOINTS];
	struct faultinfo faults[NUM_FAULTS];
	struct viewentry fixed_points[];
};
