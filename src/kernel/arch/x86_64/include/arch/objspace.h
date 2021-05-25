#pragma once

#include <arch/memory.h>
struct arch_object_space {
	struct table_level root;
};
struct arch_objspace_region {
	struct table_level table;
};

struct object_space;
void arch_objspace_map(struct object_space *space,
  uintptr_t virt,
  struct page *pages[],
  size_t,
  uint64_t flags);
