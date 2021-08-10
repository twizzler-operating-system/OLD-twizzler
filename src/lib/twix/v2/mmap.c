#include "../syscalls.h"
#include "sys.h"
#include "v2.h"
#include <twix/twix.h>
#include <twz/mutex.h>
#include <twz/obj.h>
#include <twz/sys/view.h>
static struct mutex mmap_mutex;
static uint8_t mmap_bitmap[TWZSLOT_MMAP_NUM / 8];

static ssize_t __twix_mmap_get_slot(void)
{
	for(size_t i = 0; i < TWZSLOT_MMAP_NUM; i++) {
		if(!(mmap_bitmap[i / 8] & (1 << (i % 8)))) {
			mmap_bitmap[i / 8] |= (1 << (i % 8));
			uint32_t flags;
			twz_view_get(NULL, i + TWZSLOT_MMAP_BASE, NULL, &flags);
			if(flags & VE_VALID)
				continue;
			return i + TWZSLOT_MMAP_BASE;
		}
	}
	return -1;
}

static ssize_t __twix_mmap_take_slot(size_t slot)
{
	slot -= TWZSLOT_MMAP_BASE;
	if(mmap_bitmap[slot / 8] & (1 << (slot % 8))) {
		return -1;
	}
	mmap_bitmap[slot / 8] |= (1 << (slot % 8));
	return slot + TWZSLOT_MMAP_BASE;
}

#include <sys/mman.h>
long hook_mmap(struct syscall_args *args)
{
	void *addr = (void *)args->a0;
	size_t len = (args->a1 + 0xfff) & ~0xfff;
	int prot = args->a2;
	int flags = args->a3;
	int fd = args->a4;
	off_t offset = args->a5;
	objid_t id;
	int r;

	// debug_printf("sys_mmap (v2): %p %lx %x %x %d %lx\n", addr, len, prot, flags, fd, offset);
	ssize_t slot = -1;
	size_t adj = OBJ_NULLPAGE_SIZE;
	if(addr && (flags & MAP_FIXED)) {
		adj = (uintptr_t)addr % OBJ_MAXSIZE;
		slot = VADDR_TO_SLOT(addr);
		if(__twix_mmap_take_slot(slot) == -1) {
			/* if we're trying to map to an address with an existing object in an mmap slot... */
			if(!(flags & MAP_ANON)) {
				return -ENOTSUP;
			}
			/* TODO: verify that this is a "private" object */
			twz_view_get(NULL, VADDR_TO_SLOT(addr), &id, NULL);
			if(id) {
				if((r = sys_ocopy(id, 0, (uintptr_t)addr % OBJ_MAXSIZE, 0, len, 0))) {
					return r;
				}
				return (long)addr;
			}
		}
	} else {
		addr = (void *)OBJ_NULLPAGE_SIZE;
	}

	struct twix_queue_entry tqe = build_tqe(TWIX_CMD_MMAP,
	  0,
	  sizeof(objid_t),
	  6,
	  fd,
	  prot,
	  flags,
	  offset,
	  (uintptr_t)addr % OBJ_MAXSIZE,
	  len);
	twix_sync_command(&tqe);
	if(tqe.ret)
		return tqe.ret;
	extract_bufdata(&id, sizeof(id), 0);
	/* TODO: tie object */

	if(slot == -1) {
		slot = __twix_mmap_get_slot();
	}
	if(slot == -1) {
		return -ENOMEM;
	}

	/* TODO: perms */
	long perms = 0;
	perms |= (prot & PROT_READ) ? VE_READ : 0;
	perms |= (prot & PROT_WRITE) ? VE_WRITE : 0;
	perms |= (prot & PROT_EXEC) ? VE_EXEC : 0;

	if(perms == 0) {
		perms = VE_READ | VE_WRITE; // TODO
	}

	twz_view_set(NULL, slot, id, perms);

	return (long)SLOT_TO_VADDR(slot) + adj;
}

long hook_mremap(struct syscall_args *args)
{
	void *old = (void *)args->a0;
	size_t old_sz = args->a1;
	size_t new_sz = args->a2;
	int flags = args->a3;
	void *new = (void *)args->a4;
	if(old_sz > new_sz)
		return (long)old;

	if(flags & 2)
		return -EINVAL;

	if(!(flags & 1)) {
		return -EINVAL;
	}

	struct syscall_args _args = {
		.a0 = 0,
		.a1 = new_sz,
		.a2 = PROT_READ | PROT_WRITE,
		.a3 = MAP_PRIVATE | MAP_ANON,
		.a4 = -1,
		.a5 = 0,
	};
	long p = hook_mmap(&_args);
	if(p < 0)
		return p;

	memcpy((void *)p, old, old_sz);

	return p;
}
