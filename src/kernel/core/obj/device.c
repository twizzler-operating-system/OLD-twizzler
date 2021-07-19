/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <device.h>
#include <kalloc.h>
#include <kso.h>
#include <limits.h>
#include <memory.h>
#include <object.h>
#include <page.h>
#include <syscall.h>
#include <thread.h>
#include <twz/meta.h>
#include <twz/sys/dev/bus.h>

static void __kso_device_ctor(struct object *obj)
{
	struct device *dev = obj->kso_data = kalloc(sizeof(struct device), 0);
	dev->root = obj;
	dev->flags = 0;
}

static void __kso_device_dtor(struct object *obj)
{
	kfree(obj->kso_data);
}

static struct kso_calls _kso_device = {
	.ctor = __kso_device_ctor,
	.dtor = __kso_device_dtor,
};

__initializer static void __device_init(void)
{
	kso_register(KSO_DEVICE, &_kso_device);
}

void device_signal_interrupt(struct object *obj, int inum, uint64_t val)
{
	/* TODO: try to make this more efficient */
	obj_write_data_atomic64(obj, offsetof(struct kso_device_hdr, interrupts[inum]), val);
	thread_wake_object(
	  obj, offsetof(struct kso_device_hdr, interrupts[inum]) + OBJ_NULLPAGE_SIZE, INT_MAX);
}

void device_signal_sync(struct object *obj, int snum, uint64_t val)
{
	/* TODO: try to make this more efficient */
	obj_write_data_atomic64(obj, offsetof(struct kso_device_hdr, syncs[snum]), val);
	thread_wake_object(
	  obj, offsetof(struct kso_device_hdr, syncs[snum]) + OBJ_NULLPAGE_SIZE, INT_MAX);
}

static void __device_interrupt(int v, struct interrupt_handler *ih)
{
	device_signal_interrupt(ih->devobj, ih->arg, v);
}

static int __device_alloc_interrupts(struct object *obj, size_t count)
{
	if(count > MAX_DEVICE_INTERRUPTS)
		return -EINVAL;

	printk("TODO: alloc interrupts\n");
	return 0;
#if 0
	int ret;
	struct kso_device_hdr *repr = device_get_repr(obj);
	struct device *data = obj->data;
	assert(data != NULL);

	for(size_t i = 0; i < count; i++) {
		data->irs[i] = (struct interrupt_alloc_req){
			.flags = INTERRUPT_ALLOC_REQ_VALID,
			.handler.fn = __device_interrupt,
			.handler.devobj = obj,
			.handler.arg = i,
		};
	}
	if(interrupt_allocate_vectors(count, data->irs)) {
		ret = -EIO;
		goto out;
	}

	for(size_t i = 0; i < count; i++) {
		if(data->irs[i].flags & INTERRUPT_ALLOC_REQ_ENABLED) {
			repr->interrupts[i].vec = data->irs[i].vec;
		}
	}

	ret = 0;
out:
	device_release_headers(obj);
	return ret;
#endif
}

static long __device_kaction(struct object *obj, long op, long arg)
{
	int ret = -ENOTSUP;
	switch(op) {
		case KACTION_CMD_DEVICE_SETUP_INTERRUPTS:
			ret = __device_alloc_interrupts(obj, arg);
			break;
	}
	return ret;
}

static void append_child(struct device *dev, struct object *obj, uint64_t flavor)
{
	spinlock_acquire_save(&dev->lock);
	if(dev->children_idx >= dev->children_len) {
		dev->children_len = (dev->children_len == 0) ? 4 : (dev->children_len * 2);
		dev->children =
		  krealloc(dev->children, sizeof(struct device_child) * dev->children_len, KALLOC_ZERO);
	}
	krc_get(&obj->refs);
	dev->children[dev->children_idx].child = obj;
	dev->children[dev->children_idx].flavor = flavor;
	dev->children_idx++;
	spinlock_release_restore(&dev->lock);
}

static struct object *_dev_new_obj(void)
{
	objid_t oid;
	int r = syscall_ocreate(0, 0, 0, 0, MIP_DFL_READ | MIP_DFL_WRITE, &oid);
	if(r < 0)
		return NULL;
	struct object *obj = obj_lookup(oid, 0);
	assert(obj != NULL);
	return obj;
}

struct device *device_create(struct device *parent,
  uint64_t bustype,
  uint64_t devtype,
  uint64_t devid,
  uint64_t info)
{
	struct object *obj = _dev_new_obj();
	if(!obj)
		return NULL;

	obj->kaction = __device_kaction;
	struct device *data = object_get_kso_data_checked(obj, KSO_DEVICE);
	data->children_len = 0;
	data->children_idx = 0;
	data->children = NULL;
	data->lock = SPINLOCK_INIT;
	data->root = obj;

	struct kso_device_hdr repr = {
		.device_bustype = bustype,
		.device_type = devtype,
		.device_id = devid,
	};
	obj_write_data(obj, 0, sizeof(repr), &repr);

	if(parent) {
		append_child(parent, obj, DEVICE_CHILD_DEVICE);
		kso_tree_attach_child(parent->root, obj, (info << 32) | DEVICE_CHILD_DEVICE);
	}

	return data;
}

struct object *device_add_mmio(struct device *dev,
  uintptr_t addr,
  size_t len,
  uint64_t cache_type,
  uint64_t info)
{
	struct object *obj = _dev_new_obj();
	if(!obj)
		return NULL;

	struct device_mmio_hdr repr = {
		.info = info,
		.flags = 0,
		.length = len,
	};
	obj_write_data(obj, 0, sizeof(repr), &repr);

	assert(align_up(addr, mm_page_size(0)) == addr);

	len = align_up(len, mm_page_size(0));

	for(uint64_t i = 0; i < len / mm_page_size(0); i++) {
		struct page *pg = mm_page_fake_create(addr + i * mm_page_size(0), cache_type);
		object_insert_page(obj, i + 2, pg);
	}

	object_init_kso_data(obj, KSO_DATA);
	append_child(dev, obj, DEVICE_CHILD_MMIO);
	kso_tree_attach_child(dev->root, obj, (info << 32) | DEVICE_CHILD_MMIO);
	return obj;
}

struct object *device_add_info(struct device *dev, void *data, size_t len, uint64_t info)
{
	struct object *obj = _dev_new_obj();
	if(!obj)
		return NULL;

	obj_write_data(obj, 0, len, data);
	object_init_kso_data(obj, KSO_DATA);
	append_child(dev, obj, DEVICE_CHILD_INFO);
	kso_tree_attach_child(dev->root, obj, (info << 32) | DEVICE_CHILD_INFO);
	return obj;
}

void device_attach_busroot(struct device *dev, uint64_t type)
{
	struct object *obj = device_get_busroot();
	kso_tree_attach_child(obj, dev->root, type);
}

struct object *device_get_busroot(void)
{
	static struct spinlock lock = SPINLOCK_INIT;
	static struct object *_Atomic busroot = NULL;
	if(!busroot) {
		spinlock_acquire_save(&lock);
		if(!busroot) {
			/* krc: move */
			busroot = _dev_new_obj();
			object_init_kso_data(busroot, KSO_DIRECTORY);
			kso_setname(busroot, "Device Tree");
			kso_tree_attach_child(kso_root, busroot, KSO_DEVICE);
		}
		spinlock_release_restore(&lock);
	}
	return busroot;
}

struct device *device_get_misc_bus(void)
{
	static struct spinlock lock = SPINLOCK_INIT;
	static struct device *_Atomic misc_bus = NULL;
	if(!misc_bus) {
		spinlock_acquire_save(&lock);
		if(!misc_bus) {
			/* krc: move */
			misc_bus = device_create(NULL, DEVICE_BT_MISC, DEVICE_TYPE_BUSROOT, 0, 0);
			kso_setname(misc_bus->root, "Misc Bus");
			kso_tree_attach_child(device_get_busroot(), misc_bus->root, DEVICE_BT_MISC);
		}
		spinlock_release_restore(&lock);
	}
	return misc_bus;
}
