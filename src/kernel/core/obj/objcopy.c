#include <lib/rb.h>
#include <memory.h>
#include <object.h>
#include <objspace.h>
#include <page.h>

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

#if 0
	printk("  cow range %ld %ld %ld --> %ld ((%ld %ld))\n",
	  srcrange->start,
	  len,
	  srcrange->pv_offset,
	  dstpg,
	  srcrange->len,
	  srcoff);
#endif

	struct range *dstrange = object_find_range(dest, dstpg);
	if(!dstrange) {
		/* make new range */
		// printk("      new dstrange!\n");
		dstrange = object_add_range(dest, NULL, dstpg, len, srcrange->pv_offset + srcoff);
	} else {
#if 0
		printk("      existing dstrange: %ld %ld %ld\n",
		  dstrange->start,
		  dstrange->len,
		  dstrange->pv_offset);
#endif
		if(len >= dstrange->len) {
			len = dstrange->len;
		} else {
			/* split range */
			range_cut_half(dstrange, len);
		}
		range_toss(dstrange);
	}

	srcrange->pv->refs++;
	dstrange->pv = srcrange->pv;
	dstrange->pv_offset = srcrange->pv_offset + srcoff;
	list_insert(&dstrange->pv->ranges, &dstrange->entry);

	// struct page_entry *p = vector_get(&srcrange->pv->pages, 0);
	// printk(":: %p %lx\n", dstrange, p && p->page ? p->page->addr : 0);

	return len;
}

static void invl_omap(struct omap *omap, size_t pgoff, size_t pgnum)
{
	printk("INVL OMAP:: %ld :: %ld %ld\n", omap->regnr, pgoff, pgnum);
	arch_objspace_region_cow(omap->region, pgoff, pgnum);
}

static void object_make_cow(struct object *obj, size_t pagenr, size_t pgcount)
{
	/* TODO: verify */
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
				invl_omap(omap, s, l);
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

static void object_invalidate(struct object *obj, size_t pagenr, size_t pgcount)
{
	/* TODO: verify */
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
				printk("   unmap %ld %ld %ld\n", omap->regnr, s, l);
				arch_objspace_region_unmap(omap->region, s, l);
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
	/* TODO: when discovering an empty srcrange, need to create one and a dummy pagevec to share */
	struct rwlock_result dres = rwlock_wlock(&dest->rwlock, 0);
	for(size_t i = 0; i < count; i++) {
		struct object_copy_spec *spec = &specs[i];

		if(!spec->src) {
			panic("A");
		}

		printk("doing copy: %ld %ld %ld :: " IDFMT " <= " IDFMT "\n",
		  spec->start_src,
		  spec->start_dst,
		  spec->length,
		  IDPR(dest->id),
		  IDPR(spec->src->id));
		struct rwlock_result sres = rwlock_wlock(&spec->src->rwlock, 0);
		for(size_t j = 0; j < spec->length;) {
			size_t srcpg = spec->start_src + j;
			size_t dstpg = spec->start_dst + j;

			size_t rem = spec->length - j;

			struct range *srcrange = object_find_range(spec->src, srcpg);
			if(!srcrange) {
				/* TODO: verify */
				/* TODO: A */
				// printk("TODO: do the 'next range' opt\n");
				j++;
				continue;
				srcrange = object_find_next_range(spec->src, srcpg);
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
		object_make_cow(spec->src, spec->start_src, spec->length);
		object_invalidate(dest, spec->start_dst, spec->length);
		rwlock_wunlock(&sres);
	}
	arch_mm_objspace_invalidate(0, 0xffffffffffffffff, 0);
	rwlock_wunlock(&dres);
}
