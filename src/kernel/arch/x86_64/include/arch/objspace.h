#pragma once

#include <arch/memory.h>
struct arch_object_space {
	struct table_level root;
};
struct arch_objspace_region {
	struct table_level table;
};
