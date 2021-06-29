#include <object.h>
#include <page.h>
#include <tmpmap.h>

struct io {
	size_t len;
	void *ptr;
	uint32_t off;
	int dir;
};

static void do_io(struct object *obj, size_t pagenr, struct page *page, void *data, uint64_t flags)
{
	struct io *io = data;
	assert(io->off + io->len <= mm_page_size(0));
	if(page) {
		void *addr = tmpmap_map_pages(&page, 1);
		if(io->dir == READ) {
			memcpy(io->ptr, (char *)addr + io->off, io->len);
		} else if(io->dir == WRITE) {
			memcpy((char *)addr + io->off, io->ptr, io->len);
		} else {
			panic("unknown IO direction");
		}
	} else {
		if(io->dir == READ) {
			memset(io->ptr, 0, io->len);
		} else {
			panic("got zero page when trying to write to object");
		}
	}
}

static void loop_io(struct object *obj, size_t start, size_t len, struct io *io)
{
	size_t i = 0;
	while(i < len) {
		size_t thislen = mm_page_size(0);
		size_t offset = (start + i) % mm_page_size(0);
		thislen -= offset;
		if((thislen + i) > len)
			thislen = len - i;

		io->len = thislen;
		io->off = offset;
		object_operate_on_locked_page(obj,
		  (start + i) / mm_page_size(0),
		  io->dir == READ ? OP_LP_ZERO_OK : OP_LP_DO_COPY,
		  do_io,
		  io);
		/* TODO: what happens during failure */
		io->ptr = (char *)io->ptr + thislen;
		i += thislen;
	}
}

void obj_read_data(struct object *obj, size_t start, size_t len, void *ptr)
{
	start += OBJ_NULLPAGE_SIZE;
	assert(start < OBJ_MAXSIZE && start + len <= OBJ_MAXSIZE && len < OBJ_MAXSIZE);

	struct io io = {
		.ptr = ptr,
		.dir = READ,
	};
	loop_io(obj, start, len, &io);
	/* TODO: what happens during failure */
}

void obj_write_data(struct object *obj, size_t start, size_t len, void *ptr)
{
	start += OBJ_NULLPAGE_SIZE;
	assert(start < OBJ_MAXSIZE && start + len <= OBJ_MAXSIZE && len < OBJ_MAXSIZE);
	struct io io = {
		.ptr = ptr,
		.dir = WRITE,
	};
	loop_io(obj, start, len, &io);
	/* TODO: what happens during failure */
}

struct atomic_op {
	size_t pgoff;
	uint64_t value;
};

static void do_atomic(struct object *obj __unused,
  size_t pagenr __unused,
  struct page *page,
  void *data,
  uint64_t flags __unused)
{
	struct atomic_op *op = data;
	void *addr = tmpmap_map_pages(&page, 1);
	_Atomic uint64_t *ptr = (_Atomic uint64_t *)((char *)addr + op->pgoff);
	atomic_store(ptr, op->value);
}

void obj_write_data_atomic64(struct object *obj, size_t off, uint64_t val)
{
	off += OBJ_NULLPAGE_SIZE;
	assert(off < OBJ_MAXSIZE && off + 8 <= OBJ_MAXSIZE);

	struct atomic_op atom = {
		.pgoff = off % mm_page_size(0),
		.value = val,
	};
	object_operate_on_locked_page(obj, off / mm_page_size(0), OP_LP_DO_COPY, do_atomic, &atom);
}
