#include <object.h>
#include <page.h>
#include <slots.h>

void obj_read_data(struct object *obj, size_t start, size_t len, void *ptr)
{
	start += OBJ_NULLPAGE_SIZE;
	assert(start < OBJ_MAXSIZE && start + len <= OBJ_MAXSIZE && len < OBJ_MAXSIZE);
	panic("A");
}

void obj_write_data(struct object *obj, size_t start, size_t len, void *ptr)
{
	start += OBJ_NULLPAGE_SIZE;
	assert(start < OBJ_MAXSIZE && start + len <= OBJ_MAXSIZE && len < OBJ_MAXSIZE);
	panic("A");
}

void obj_write_data_atomic64(struct object *obj, size_t off, uint64_t val)
{
	off += OBJ_NULLPAGE_SIZE;
	assert(off < OBJ_MAXSIZE && off + 8 <= OBJ_MAXSIZE);
	panic("A");
}
