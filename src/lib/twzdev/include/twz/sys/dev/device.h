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

enum device_sync { DEVICE_SYNC_STATUS, DEVICE_SYNC_IOV_FAULT, MAX_DEVICE_SYNCS };

#define DEVICE_CHILD_DEVICE 0
#define DEVICE_CHILD_MMIO 1
#define DEVICE_CHILD_INFO 2

#define INTERRUPT_VALID 1

#define KACTION_CMD_DEVICE_SETUP_INTERRUPTS 1

#define DEVICE_BT_ISA 0
#define DEVICE_BT_PCIE 1
#define DEVICE_BT_USB 2
#define DEVICE_BT_MISC 3
#define DEVICE_BT_NV 4
#define DEVICE_BT_SYSTEM 1024

#define DEVICE_ID_KEYBOARD 1
#define DEVICE_ID_SERIAL 2
#define DEVICE_ID_FRAMEBUFFER 3

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

struct kso_device_hdr {
	struct kso_hdr ksohdr;
	uint64_t device_bustype;
	uint64_t device_type;
	uint64_t device_id;
#ifdef __cplusplus
	std::atomic_uint_least64_t syncs[MAX_DEVICE_SYNCS];
#else
	atomic_uint_least64_t syncs[MAX_DEVICE_SYNCS];
#endif
	struct interrupt interrupts[MAX_DEVICE_INTERRUPTS];
	struct kso_dir_hdr dir;
};

struct device_mmio_hdr {
	struct kso_hdr ksohdr;
	uint64_t info;
	uint64_t flags;
	uint64_t length;
	uint64_t resv;
};

#ifndef __KERNEL__

struct device {
	twzobj obj;
	struct kso_device_hdr *hdr;
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

#endif
#ifdef __cplusplus
}
#endif
