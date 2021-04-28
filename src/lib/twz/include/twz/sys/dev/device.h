#pragma once

#include <twz/sys/kso.h>

#ifdef __cplusplus
#include <atomic>
#else
#include <stdatomic.h>
#endif

enum device_sync { DEVICE_SYNC_READY, DEVICE_SYNC_ERROR, DEVICE_SYNC_IOV_FAULT, MAX_DEVICE_SYNCS };

#define DEVICE_INPUT 1
#define DEVICE_IO 2

#define DEVICE_ID_KEYBOARD 1
#define DEVICE_ID_SERIAL 2

#define DEVICE_ID_FRAMEBUFFER 3

struct device_interrupt {
#ifdef __cplusplus
	std::atomic_uint_least64_t sp;
#else
	atomic_uint_least64_t sp;
#endif
	uint32_t flags;
	uint16_t resv;
	uint16_t vec;
};

#define MAX_DEVICE_INTERRUPTS 32

struct device_repr {
	struct kso_hdr hdr;
	uint64_t device_type;
	uint32_t device_bustype;
	uint32_t device_id;
#ifdef __cplusplus
	std::atomic_uint_least64_t syncs[MAX_DEVICE_SYNCS];
#else
	atomic_uint_least64_t syncs[MAX_DEVICE_SYNCS];
#endif
	struct device_interrupt interrupts[MAX_DEVICE_INTERRUPTS];
};

#define DEVICE_BT_ISA 0
#define DEVICE_BT_PCIE 1
#define DEVICE_BT_USB 2
#define DEVICE_BT_MISC 3
#define DEVICE_BT_NV 4
#define DEVICE_BT_SYSTEM 1024

#define KACTION_CMD_DEVICE_SETUP_INTERRUPTS 1

#ifndef __KERNEL__

#include <twz/obj.h>
#include <twz/sys/obj.h>
#include <twz/sys/syscall.h>

static inline struct device_repr *twz_device_getrepr(twzobj *obj)
{
	return (struct device_repr *)twz_object_base(obj);
}

static inline void *twz_device_getds(twzobj *obj)
{
	return (void *)(twz_device_getrepr(obj) + 1);
}

static inline int twz_device_map_object(twzobj *dev, twzobj *obj, size_t off, size_t len)
{
	objid_t cid = twz_object_guid(dev);
	return twz_object_ctl(obj, OCO_MAP, off, len, &cid);
}

#endif
