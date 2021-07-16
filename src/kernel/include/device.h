#pragma once

#include <interrupt.h>
#include <spinlock.h>
#include <twz/sys/dev/device.h>

struct object;

struct device_child {
	struct object *child;
	uint64_t flavor;
};

struct device {
	struct object *root;
	struct device_child *children;
	size_t children_len, children_idx;
	struct interrupt_alloc_req irs[MAX_DEVICE_INTERRUPTS];
	struct spinlock lock;
	uint32_t flags;
};

struct device *device_get_misc_bus(void);
struct object *device_get_busroot(void);
struct object *device_add_info(struct device *dev, void *data, size_t len, uint64_t info);
struct object *device_add_mmio(struct device *dev,
  uintptr_t addr,
  size_t len,
  uint64_t cache_type,
  uint64_t info);
struct device *device_create(struct device *parent,
  uint64_t bustype,
  uint64_t devtype,
  uint64_t devid,
  uint64_t info);
void device_attach_busroot(struct device *dev, uint64_t type);

void iommu_object_map_slot(struct device *dev, struct object *obj);
void iommu_invalidate_tlb(void);

void device_signal_interrupt(struct object *obj, int inum, uint64_t val);
void device_signal_sync(struct object *obj, int snum, uint64_t val);
struct device *device_get_misc_bus(void);
