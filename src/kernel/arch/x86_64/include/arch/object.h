#pragma once

#include <arch/memory.h>

struct arch_object {
	uintptr_t pt_root;
	uint64_t *pd;
	uint64_t **pts;
};
