#include <object.h>

struct range *object_find_range(struct object *obj, size_t page)
{
	panic("");
}

struct range *object_add_range(struct object *obj,
  struct pagevec *pv,
  size_t start,
  size_t len,
  size_t off)
{
	panic("");
}

struct range *range_split(struct range *range, size_t rp)
{
	panic("");
}

void range_clone(struct range *range)
{
}

size_t range_pv_idx(struct range *range, size_t idx)
{
	return (idx - range->start) + range->pv_offset;
}
