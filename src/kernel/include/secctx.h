#pragma once

#include <arch/secctx.h>
#include <krc.h>
#include <object.h>

#include <twz/objid.h>

#include <lib/rb.h>

#include <twz/sys/sctx.h>

struct sctx_cache_entry {
	objid_t id;
	struct scgates *gates;
	size_t gate_count;
	struct rbnode node;
	uint32_t perms;
};

struct object_space;
struct sctx {
	struct object_space *space;
	struct object *obj;
	struct krc refs;
	struct rbroot cache;
	struct spinlock cache_lock;
	bool superuser;
};

void arch_secctx_init(struct sctx *sc);
void arch_secctx_destroy(struct sctx *sc);

struct sctx *secctx_alloc(struct object *);
void secctx_free(struct sctx *s);
void secctx_switch(int i);
struct thread;
int secctx_fault_resolve(void *ip,
  uintptr_t loaddr,
  void *vaddr,
  struct object *target,
  uint32_t flags,
  uint32_t *perms,
  bool,
  objid_t);
struct object;
int secctx_check_permissions(void *, struct object *, uint32_t flags);
int secctx_check_permissions_hint(void *ip, struct object *to, uint32_t flags, objid_t hint_id);
