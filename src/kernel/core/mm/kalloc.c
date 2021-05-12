#include <kalloc.h>
#include <memory.h>
#include <slab.h>

#define CANARY 0x55667788

struct header {
	uint32_t canary;
	int32_t size_class;
	struct kheap_run *run;
};

#define NR_CACHES 8

static struct slabcache caches[NR_CACHES];

static size_t get_size(int i)
{
	return (1 << (i + 4)) + 16;
}

static int get_class(size_t len)
{
	for(int i = 0; i < NR_CACHES; i++) {
		if(get_size(i) > len)
			return i;
	}
	return -1;
}

void kalloc_system_init(void)
{
	printk("[mm] creating %d kalloc bins\n", NR_CACHES);
	for(int i = 0; i < NR_CACHES; i++) {
		slabcache_init(&caches[i], "kalloc", get_size(i), NULL, NULL, NULL, NULL, NULL);
	}
}

void *kalloc(size_t len, int flags)
{
	len += sizeof(struct header);
	int class = get_class(len);
	if(class == -1) {
		struct kheap_run *run = kheap_allocate(len);
		struct header *hdr = run->start;
		hdr->run = run;
		hdr->size_class = -1;
		hdr->canary = CANARY;
		return (void *)(hdr + 1);
	}
	struct header *obj = slabcache_alloc(&caches[class]);
	obj->canary = CANARY;
	obj->size_class = class;
	if(flags & KALLOC_ZERO)
		memset((void *)(obj + 1), 0, len - sizeof(struct header));
	assert(is_aligned(obj + 1, 8));
	return (void *)(obj + 1);
}

void *kcalloc(size_t a, size_t b, int flags)
{
	void *p = kalloc(a * b, flags);
	return p;
}

void *krealloc(void *p, size_t len, int flags)
{
	struct header *hdr = (void *)((char *)p - sizeof(struct header));
	assert(hdr->canary == CANARY);
	size_t oldsz = get_size(hdr->size_class);

	void *newreg = kalloc(len, flags);
	size_t copy_len = oldsz;
	if(len < oldsz)
		copy_len = len;
	memcpy(newreg, p, copy_len);
	kfree(p);
	return newreg;
}

void *krecalloc(void *p, size_t a, size_t b, int flags)
{
	return krealloc(p, a * b, flags);
}

void kfree(void *p)
{
	struct header *hdr = (void *)((char *)p - sizeof(struct header));
	assert(hdr->canary == CANARY);
	if(hdr->size_class == -1) {
		kheap_free(hdr->run);
	} else {
		slabcache_free(&caches[hdr->size_class], hdr);
	}
}
