#pragma once

#include <interrupt.h>
#include <twz/sys/dev/device.h>

struct object;
struct device {
	uint64_t uid;
	struct object *co;
	struct interrupt_alloc_req irs[MAX_DEVICE_INTERRUPTS];
	uint32_t flags;
};
void iommu_object_map_slot(struct device *dev, struct object *obj);
void iommu_invalidate_tlb(void);

struct object *device_register(uint32_t bustype, uint32_t devid);
void device_unregister(struct object *obj);
void device_signal_interrupt(struct object *obj, int inum, uint64_t val);
void device_signal_sync(struct object *obj, int snum, uint64_t val);
struct object *bus_register(uint32_t bustype, uint32_t busid, size_t bssz);
struct object *device_get_misc_bus(void);

#define DEVICE 0
#define BUS 1

void device_rw_specific(struct object *obj, int dir, void *ptr, int type, size_t rwlen);
void device_rw_header(struct object *obj, int dir, void *ptr, int type);
