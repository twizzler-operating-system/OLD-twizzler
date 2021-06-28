#include <__mm_bits.h>
#include <object.h>
#include <pagevec.h>
#include <range.h>
void object_insert_page(struct object *obj, size_t pagenr, struct page *page)
{
	struct rwlock_result rwres = rwlock_wlock(&obj->rwlock, 0);
	struct range *range = object_find_range(obj, pagenr);
	if(!range) {
		size_t off;
		struct pagevec *pv = object_new_pagevec(obj, pagenr, &off);
		range = object_add_range(obj, pv, pagenr, pagevec_len(pv) - off, off);
	}

	size_t pvidx = range_pv_idx(range, pagenr);

	if(pvidx > PAGEVEC_MAX_IDX) {
		size_t off;
		struct pagevec *pv = object_new_pagevec(obj, pagenr, &off);
		range = object_add_range(obj, pv, pagenr, pagevec_len(pv) - off, off);
	}

	pagevec_set_page(range->pv, pvidx, page);

	rwlock_wunlock(&rwres);
}

int object_operate_on_locked_page(struct object *obj,
  size_t pagenr,
  int flags,
  void (*fn)(struct object *obj, size_t pagenr, struct page *page, void *data, uint64_t cb_fl),
  void *data)
{
	uint64_t cb_fl = 0;
	struct rwlock_result rwres = rwlock_rlock(&obj->rwlock, 0);
	struct range *range = object_find_range(obj, pagenr);
	if(!range) {
		if(flags & OP_LP_ZERO_OK) {
			fn(obj, pagenr, NULL, data, cb_fl);
			rwlock_runlock(&rwres);
			return 0;
		}
		rwres = rwlock_upgrade(&rwres, 0);
		size_t off;
		struct pagevec *pv = object_new_pagevec(obj, pagenr, &off);
		range = object_add_range(obj, pv, pagenr, pagevec_len(pv) - off, off);
		rwres = rwlock_downgrade(&rwres);
	}

	size_t pvidx = range_pv_idx(range, pagenr);

	struct page *page;
	struct pagevec *locked_pv = range->pv;
	pagevec_lock(locked_pv);
	int ret = pagevec_get_page(range->pv, pvidx, &page, GET_PAGE_BLOCK);

	if(ret == GET_PAGE_BLOCK) {
		printk("TODO: A implement this\n");
		/* TODO: return a "def resched" thing */
	} else {
		if(range->pv->refs > 1) {
			if(flags & OP_LP_DO_COPY) {
				rwres = rwlock_upgrade(&rwres, 0);
				range = range_split(range, pagenr - range->start);
				range_clone(range);
				pagevec_unlock(locked_pv);
				locked_pv = range->pv;
				pagevec_lock(locked_pv);
				rwres = rwlock_downgrade(&rwres);

				pvidx = range_pv_idx(range, pagenr);
				ret = pagevec_get_page(range->pv, pvidx, &page, GET_PAGE_BLOCK);
				assert(ret == 0);
			} else {
				cb_fl |= PAGE_MAP_COW;
			}
		}
		fn(obj, pagenr, page, data, cb_fl);
	}
	pagevec_unlock(range->pv);

	rwlock_runlock(&rwres);

	return 0;
}
