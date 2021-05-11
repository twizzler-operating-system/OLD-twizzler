#pragma once

#include <arch/memory.h>

struct arch_object {
	uintptr_t pt_root;
	uint64_t *pd;
	uint64_t **pts;
};

struct arch_object_space {
	struct table_level root;
};
