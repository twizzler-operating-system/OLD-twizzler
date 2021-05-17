#pragma once

#include <arch/objspace.h>

struct object_space {
	struct arch_object_space arch;
	struct krc refs;
};

void object_space_map_slot(struct object_space *space, struct slot *slot, uint64_t flags);
void object_space_release_slot(struct slot *slot);
void arch_object_space_fini(struct object_space *space);
void arch_object_space_init(struct object_space *space);
void object_space_free(struct object_space *space);
struct object_space *object_space_alloc(void);
