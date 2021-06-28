#include <clksrc.h>
#include <lib/rb.h>
#include <memory.h>
#include <object.h>
#include <objspace.h>
#include <page.h>
#include <pagevec.h>
#include <range.h>

static size_t cow_range(struct object *dest,
  struct range *srcrange,
  size_t dstpg,
  size_t srcpg,
  size_t maxlen)
{
	size_t srcoff = srcpg - srcrange->start;
	size_t len = maxlen;
	if(srcrange->len < len) {
		len = srcrange->len;
	}
	assert(len > 0);

	struct range *dstrange = object_find_range(dest, dstpg);
	if(!dstrange) {
		/* make new range */
		dstrange = object_add_range(dest, NULL, dstpg, len, srcrange->pv_offset + srcoff);
	} else {
		if(len >= dstrange->len) {
			len = dstrange->len;
		} else {
			/* split range */
			range_cut_half(dstrange, len);
		}
		range_toss(dstrange);
	}

	assert(srcrange->pv->refs > 0);
	assert(dstrange->pv == NULL);
	pagevec_lock(srcrange->pv);
	srcrange->pv->refs++;
	dstrange->pv = srcrange->pv;
	dstrange->pv_offset = srcrange->pv_offset + srcoff;
	list_insert(&dstrange->pv->ranges, &dstrange->entry);
	assert(list_len(&dstrange->pv->ranges) == dstrange->pv->refs);
	pagevec_unlock(srcrange->pv);

	return len;
}

#define OP_INVL 1
#define OP_COW 2

static void object_op_on_pages(struct object *obj, size_t pagenr, size_t pgcount, int type)
{
	while(pgcount) {
		size_t omapnr = pagenr / (mm_objspace_region_size() / mm_page_size(0));
		struct rbnode *node =
		  rb_search(&obj->omap_root, omapnr, struct omap, objnode, omap_compar_key);

		while(1) {
			size_t s = pagenr % (mm_objspace_region_size() / mm_page_size(0));
			size_t l = (mm_objspace_region_size() / mm_page_size(0));
			if(s + l > mm_objspace_region_size() / mm_page_size(0)) {
				l = (mm_objspace_region_size() / mm_page_size(0)) - s;
			}
			if(pgcount < l)
				l = pgcount;

			if(node) {
				struct omap *omap = rb_entry(node, struct omap, objnode);
				switch(type) {
					case OP_INVL:
						arch_objspace_region_unmap(omap->region, s, l);
						break;
					case OP_COW:
						arch_objspace_region_cow(omap->region, s, l);
						break;
					default:
						panic("invalid op type");
				}
				pgcount -= l;
				pagenr += l;

				node = rb_next(node);
				if(node) {
					struct omap *next_omap = rb_entry(node, struct omap, objnode);
					size_t dist = (next_omap->regnr - omap->regnr)
					              * (mm_objspace_region_size() / mm_page_size(0));
					if(dist >= pgcount) {
						return;
					}

					pagenr += dist;
					pgcount -= dist;
				} else {
					return;
				}
			} else {
				pgcount -= l;
				pagenr += l;
				break;
			}
		}
	}
}

void object_copy(struct object *dest, struct object_copy_spec *specs, size_t count)
{
	/* TODO (high): when discovering an empty srcrange, need to create one and a dummy pagevec to
	 * share */
	struct rwlock_result dres = rwlock_wlock(&dest->rwlock, 0);
	size_t nrpages = 0;
	for(size_t i = 0; i < count; i++) {
		struct object_copy_spec *spec = &specs[i];
		nrpages += specs[i].length;

		if(!spec->src) {
			/* TODO (high): zero-out pages in dest for this range */
			printk("warning -- zero-out copy unimplemented");
			continue;
		}

#if 0
		printk("doing copy: %ld %ld %ld :: " IDFMT " <= " IDFMT "\n",
		  spec->start_src,
		  spec->start_dst,
		  spec->length,
		  IDPR(dest->id),
		  IDPR(spec->src->id));
#endif
		struct rwlock_result sres = rwlock_wlock(&spec->src->rwlock, 0);
		for(size_t j = 0; j < spec->length;) {
			size_t srcpg = spec->start_src + j;
			size_t dstpg = spec->start_dst + j;

			size_t rem = spec->length - j;

			struct range *srcrange = object_find_range(spec->src, srcpg);
			if(!srcrange) {
				srcrange = object_find_next_range(spec->src, srcpg);
				assert(srcrange->start > srcpg);
				if(!srcrange || (srcrange->start >= srcpg + rem)) {
					j += rem;
					continue;
				} else {
					j += srcrange->start - srcpg;
					continue;
				}
			}

			size_t x = cow_range(dest, srcrange, dstpg, srcpg, rem);
			j += x;
		}
		object_op_on_pages(spec->src, spec->start_src, spec->length, OP_COW);
		object_op_on_pages(dest, spec->start_dst, spec->length, OP_INVL);
		rwlock_wunlock(&sres);
	}
	/* TODO (opt): don't invalidate the whole address space */
	arch_mm_objspace_invalidate(NULL, 0, 0xffffffffffffffff, 0);
	rwlock_wunlock(&dres);
}
