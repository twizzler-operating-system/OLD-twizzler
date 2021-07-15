#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <twz/_types.h>
#include <twz/obj.h>
#include <twz/sys/kso.h>

#ifdef __cplusplus
#include <atomic>
#else
#include <stdatomic.h>
#endif

#define MAX_DEVICE_INTERRUPTS 32

#define INTERRUPT_VALID 1

struct interrupt {
	uint16_t local;
	uint16_t vector;
	uint32_t flags;
#ifdef __cplusplus
	std::atomic_uint_least64_t sp;
#else
	atomic_uint_least64_t sp;
#endif
};

struct device_kso_header {
	struct kso_hdr ksohdr;
	struct interrupt interrupts[MAX_DEVICE_INTERRUPTS];
};

struct device {
	twzobj obj;
	struct device_kso_header *hdr;
};

int device_init(struct device *dev, objid_t id);

ssize_t device_allocate_interrupts(struct device *dev, size_t count);

struct device_map {
	twzobj *obj;
	uint64_t offset;
	uint64_t length;
};

int device_map(struct device *dev, struct device_map *maps, size_t count);

int device_unmap(struct device *dev, struct device_map *maps, size_t count);

void device_destroy(struct device *dev);

#ifdef __cplusplus
}
#endif
