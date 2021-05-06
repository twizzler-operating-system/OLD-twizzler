#include <page.h>
#include <pager.h>
#include <processor.h>
#include <rwlock.h>
#include <secctx.h>
#include <slots.h>
#include <thread.h>
#include <tmpmap.h>
#include <twz/meta.h>
int obj_check_permission_ip(struct object *obj, uint64_t flags, uint64_t ip)
{
	// printk("Checking permission of object %p: " IDFMT "\n", obj, IDPR(obj->id));
	bool w = (flags & MIP_DFL_WRITE);
	if(!obj_verify_id(obj, !w, w)) {
		return -EINVAL;
	}

	/*
	uint32_t p_flags;
	if(!obj_get_pflags(obj, &p_flags))
	    return 0;
	uint32_t dfl = p_flags & (MIP_DFL_READ | MIP_DFL_WRITE | MIP_DFL_EXEC | MIP_DFL_USE);

	if((dfl & flags) == flags) {
	    //	return 0;
	}
	*/
	if(!current_thread) {
		return 0;
	}
	int r = secctx_check_permissions((void *)ip, obj, flags);
	return r;
}

int obj_check_permission(struct object *obj, uint64_t flags)
{
	// printk("Checking permission of object %p: " IDFMT "\n", obj, IDPR(obj->id));
	bool w = (flags & MIP_DFL_WRITE);
	if(!obj_verify_id(obj, !w, w)) {
		return -EINVAL;
	}

	/*
	uint32_t p_flags;
	if(!obj_get_pflags(obj, &p_flags))
	    return 0;
	uint32_t dfl = p_flags & (MIP_DFL_READ | MIP_DFL_WRITE | MIP_DFL_EXEC | MIP_DFL_USE);

	if((dfl & flags) == flags) {
	    //	return 0;
	}
	*/
	if(!current_thread) {
		return 0;
	}
	int r = secctx_check_permissions((void *)arch_thread_instruction_pointer(), obj, flags);
	return r;
}

#include <twz/sys/sctx.h>
static uint32_t __conv_objperm_to_scp(uint64_t p)
{
	uint32_t perms = 0;
	if(p & OBJSPACE_READ) {
		perms |= SCP_READ;
	}
	if(p & OBJSPACE_WRITE) {
		perms |= SCP_WRITE;
	}
	if(p & OBJSPACE_EXEC_U) {
		perms |= SCP_EXEC;
	}
	return perms;
}

static uint64_t __conv_scp_to_objperm(uint32_t p)
{
	uint64_t perms = 0;
	if(p & SCP_READ) {
		perms |= OBJSPACE_READ;
	}
	if(p & SCP_WRITE) {
		perms |= OBJSPACE_WRITE;
	}
	if(p & SCP_EXEC) {
		perms |= OBJSPACE_EXEC_U;
	}
	return perms;
}

static bool __objspace_fault_calculate_perms(struct object *o,
  uint32_t flags,
  uintptr_t loaddr,
  uintptr_t vaddr,
  uintptr_t ip,
  uint64_t *perms)
{
	/* optimization: just check if default permissions are enough */
#if 0
	uint32_t p_flags;
	if(!obj_get_pflags(o, &p_flags)) {
		struct fault_object_info info =
		  twz_fault_build_object_info(o->id, (void *)ip, (void *)vaddr, FAULT_OBJECT_INVALID);
		thread_raise_fault(current_thread, FAULT_OBJECT, &info, sizeof(info));

		return false;
	}
	uint32_t dfl = p_flags & (MIP_DFL_READ | MIP_DFL_WRITE | MIP_DFL_EXEC | MIP_DFL_USE);
	bool ok = true;
	if(flags & OBJSPACE_FAULT_READ) {
		ok = ok && (dfl & MIP_DFL_READ);
	}
	if(flags & OBJSPACE_FAULT_WRITE) {
		ok = ok && (dfl & MIP_DFL_WRITE);
	}
	if(flags & OBJSPACE_FAULT_EXEC) {
		ok = ok && (dfl & MIP_DFL_EXEC);
	}
	if(dfl & MIP_DFL_READ)
		*perms |= OBJSPACE_READ;
	if(dfl & MIP_DFL_WRITE)
		*perms |= OBJSPACE_WRITE;
	if(dfl & MIP_DFL_EXEC)
		*perms |= OBJSPACE_EXEC_U;
#else
	bool ok = false;
#endif
	if(!ok) {
		*perms = 0;
		uint32_t res;
		if(secctx_fault_resolve(
		     (void *)ip, loaddr, (void *)vaddr, o, __conv_objperm_to_scp(flags), &res, true, 0)
		   == -1) {
			return false;
		}
		*perms = __conv_scp_to_objperm(res);
	}

	bool w = (*perms & OBJSPACE_WRITE);
	if(!obj_verify_id(o, !w, w)) {
		struct fault_object_info info =
		  twz_fault_build_object_info(o->id, (void *)ip, (void *)vaddr, FAULT_OBJECT_INVALID);
		thread_raise_fault(current_thread, FAULT_OBJECT, &info, sizeof(info));
		return false;
	}

	if(((*perms & flags) & (OBJSPACE_READ | OBJSPACE_WRITE | OBJSPACE_EXEC_U))
	   != (flags & (OBJSPACE_READ | OBJSPACE_WRITE | OBJSPACE_EXEC_U))) {
		panic("Insufficient permissions for mapping (should be handled earlier)");
	}
	return true;
}

struct object *obj_lookup_slot(uintptr_t oaddr, struct slot **slot)
{
	ssize_t tl = oaddr / mm_page_size(MAX_PGLEVEL);
	*slot = slot_lookup(tl);
	if(!*slot) {
		return NULL;
	}
	struct object *obj = (*slot)->obj;
	if(obj) {
		krc_get(&obj->refs);
	}
	return obj;
}

void kernel_objspace_fault_entry(uintptr_t ip, uintptr_t loaddr, uintptr_t vaddr, uint32_t flags)
{
	size_t idx = (loaddr % mm_page_size(MAX_PGLEVEL)) / mm_page_size(0);
	if(idx == 0 && !VADDR_IS_KERNEL(vaddr)) {
		struct fault_null_info info = twz_fault_build_null_info((void *)ip, (void *)vaddr);
		thread_raise_fault(current_thread, FAULT_NULL, &info, sizeof(info));
		return;
	}

	struct slot *slot;
	struct object *o = obj_lookup_slot(loaddr, &slot);

	if(o == NULL) {
		panic(
		  "no object mapped to slot during object fault: vaddr=%lx, oaddr=%lx, ip=%lx, slot=%ld",
		  vaddr,
		  loaddr,
		  ip,
		  loaddr / OBJ_MAXSIZE);
	}

	uint64_t perms = 0;
	uint64_t existing_flags;

	bool do_map = !arch_object_getmap_slot_flags(NULL, slot, &existing_flags);
	do_map = do_map || (existing_flags & flags) != flags;

	if(do_map) {
		if(!VADDR_IS_KERNEL(vaddr) && !(o->flags & OF_KERNEL)) {
			if(o->flags & OF_PAGER) {
				perms = OBJSPACE_READ | OBJSPACE_WRITE | OBJSPACE_EXEC_U;
			} else {
				if(!__objspace_fault_calculate_perms(o, flags, loaddr, vaddr, ip, &perms)) {
					goto done;
				}
			}
			perms &= (OBJSPACE_READ | OBJSPACE_WRITE | OBJSPACE_EXEC_U);
		} else {
			perms = OBJSPACE_READ | OBJSPACE_WRITE;
		}
		if((flags & perms) != flags) {
			panic("TODO: this mapping will never work");
		}

		spinlock_acquire_save(&slot->lock);
		if(!arch_object_getmap_slot_flags(NULL, slot, &existing_flags)) {
			object_space_map_slot(NULL, slot, perms);
		} else if((existing_flags & flags) != flags) {
			arch_object_map_slot(NULL, o, slot, perms);
		}
		spinlock_release_restore(&slot->lock);
	}

	struct rwlock_result rwres = rwlock_rlock(&o->rwlock, 0);
	struct range *range = object_find_range(o, idx);
	if(!range) {
		rwres = rwlock_upgrade(&rwres, 0);
		size_t off;
		struct pagevec *pv = object_new_pagevec(o, idx, &off);
		range = object_add_range(o, pv, idx, pagevec_len(pv) - off, off);
		rwres = rwlock_downgrade(&rwres);
	}

	size_t pvidx = range_pv_idx(range, idx);

	struct page *page;
	int ret = pagevec_get_page(range->pv, pvidx, &page, GET_PAGE_BLOCK);

	if(ret == GET_PAGE_BLOCK) {
		/* TODO: return a "def resched" thing */
	} else {
		int mapflags = 0;
		if(range->pv->refs > 1) {
			if(flags & OBJSPACE_FAULT_WRITE) {
				rwres = rwlock_upgrade(&rwres, 0);
				range = range_split(range, idx - range->start);
				range_clone(range);
				rwres = rwlock_downgrade(&rwres);

				ret = pagevec_get_page(range->pv, pvidx, &page, GET_PAGE_BLOCK);
				assert(ret == 0);
			} else {
				flags |= PAGE_MAP_COW;
			}
		}
		arch_object_map_page(o, idx, page, mapflags);
	}

	rwlock_runlock(&rwres);

#if 0
	if(o->flags & OF_ALLOC) {
		struct objpage p = { 0 };
		p.page = page_alloc(PAGE_TYPE_VOLATILE,
		  (current_thread && current_thread->page_alloc) ? PAGE_CRITICAL : 0,
		  0); /* TODO: refcount, largepage */
		p.idx = (loaddr % OBJ_MAXSIZE) / mm_page_size(p.page->level);
		p.page->flags = PAGE_CACHE_WB;
		//	printk("Y\n");
		spinlock_acquire_save(&p.page->lock);
		arch_object_map_page(o, &p);
		spinlock_release_restore(&p.page->lock);
	} else {
		struct objpage *p = NULL;
		enum obj_get_page_result gpr =
		  obj_get_page(o, loaddr % OBJ_MAXSIZE, &p, OBJ_GET_PAGE_ALLOC | OBJ_GET_PAGE_PAGEROK);
		switch(gpr) {
			case GETPAGE_OK:
				break;
			case GETPAGE_PAGER:
				if(p) {
					objpage_release(p, 0);
				}
				goto done;
			case GETPAGE_NOENT: {
				panic("TODO: raise fault");
			} break;
		}
		assert(p && p->page);

		if(!(o->flags & OF_KERNEL)) {
			spinlock_acquire_save(&p->lock);
			//		spinlock_acquire_save(&o->lock);
			//		spinlock_acquire_save(&p->page->lock);
			if(p->page->cowcount <= 1 && (p->flags & OBJPAGE_COW)) {
				p->flags &= ~(OBJPAGE_COW | OBJPAGE_MAPPED);
				p->page->cowcount = 1;
			}

			if((p->flags & OBJPAGE_COW) && (flags & OBJSPACE_FAULT_WRITE)) {
				uint32_t old_count = atomic_fetch_sub(&p->page->cowcount, 1);
				if(old_count > 1) {
					objpage_do_cow_write(p);
				} else {
					p->page->cowcount = 1;
				}

				p->flags &= ~(OBJPAGE_COW | OBJPAGE_MAPPED);
				spinlock_release_restore(&p->lock);

				spinlock_acquire_save(&o->lock);
				arch_object_map_page(o, p);
				spinlock_release_restore(&o->lock);
				p->flags |= OBJPAGE_MAPPED;
			} else {
				spinlock_release_restore(&p->lock);
				spinlock_acquire_save(&o->lock);
				if((flags & OBJSPACE_FAULT_WRITE) && (p->flags & OBJPAGE_MAPPED)
				   && !(p->flags & OBJPAGE_COW)) {
					/* possibility that we're faulting after this page was COW'd, and then the COW
					 * page was freed, and then we want to write to the original. In this case, the
					 * object page might be marked as non-cow, non-write, mapped. TODO: we could
					 * probably tag the page flags with this, somehow, but it's just as easy to
					 * check the page tables */
					uint64_t fl;
					if(!arch_object_getmap(o, loaddr % OBJ_MAXSIZE, NULL, NULL, &fl)
					   || !(fl & OBJSPACE_WRITE)) {
						p->flags &= ~OBJPAGE_MAPPED;
					}
				}
				if(!(p->flags & OBJPAGE_MAPPED)) {
					arch_object_map_page(o, p);
					p->flags |= OBJPAGE_MAPPED;
				}
				spinlock_release_restore(&o->lock);
			}

			//		spinlock_release_restore(&p->page->lock);
			objpage_release(p, 0);
			//		spinlock_release_restore(&o->lock);
		} else {
			spinlock_acquire_save(&o->lock);
			if(!(p->flags & OBJPAGE_MAPPED)) {
				arch_object_map_page(o, p);
				p->flags |= OBJPAGE_MAPPED;
			}
			spinlock_release_restore(&o->lock);
		}
	}
#endif
done:
	obj_put(o);
	slot_release(slot);
}
