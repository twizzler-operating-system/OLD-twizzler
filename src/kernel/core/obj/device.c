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
	dev->co = obj;
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

void device_rw_header(struct object *obj, int dir, void *ptr, int type)
{
	assert(type == DEVICE || type == BUS);
	size_t len = type == DEVICE ? sizeof(struct device_repr) : sizeof(struct bus_repr);
	if(dir == READ) {
		obj_read_data(obj, 0, len, ptr);
	} else if(dir == WRITE) {
		obj_write_data(obj, 0, len, ptr);
	} else {
		panic("unknown IO direction");
	}
}

void device_rw_specific(struct object *obj, int dir, void *ptr, int type, size_t rwlen)
{
	assert(type == DEVICE || type == BUS);
	size_t len = type == DEVICE ? sizeof(struct device_repr) : sizeof(struct bus_repr);
	if(dir == READ) {
		obj_read_data(obj, len, rwlen, ptr);
	} else if(dir == WRITE) {
		obj_write_data(obj, len, rwlen, ptr);
	} else {
		panic("unknown IO direction");
	}
}

void device_signal_interrupt(struct object *obj, int inum, uint64_t val)
{
	/* TODO: try to make this more efficient */
	obj_write_data_atomic64(obj, offsetof(struct device_repr, interrupts[inum]), val);
	thread_wake_object(
	  obj, offsetof(struct device_repr, interrupts[inum]) + OBJ_NULLPAGE_SIZE, INT_MAX);
}

void device_signal_sync(struct object *obj, int snum, uint64_t val)
{
	obj_write_data_atomic64(obj, offsetof(struct device_repr, syncs[snum]), val);
	thread_wake_object(obj, offsetof(struct device_repr, syncs[snum]) + OBJ_NULLPAGE_SIZE, INT_MAX);
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
	struct device_repr *repr = device_get_repr(obj);
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

struct object *device_register(uint32_t bustype, uint32_t devid)
{
	int r;
	objid_t psid;
	/* TODO: restrict write access. In fact, do this for ALL KSOs. */
	r = syscall_ocreate(0, 0, 0, 0, MIP_DFL_READ | MIP_DFL_WRITE, &psid);
	if(r < 0)
		panic("failed to create device object: %d", r);
	struct object *obj = obj_lookup(psid, OBJ_LOOKUP_HIDDEN);
	assert(obj != NULL);
	obj->kaction = __device_kaction;
	struct device *data = object_get_kso_data_checked(obj, KSO_DEVICE);
	data->uid = ((uint64_t)bustype << 32) | devid;

	struct device_repr repr = {
		.device_bustype = bustype,
		.device_id = devid,
	};
	device_rw_header(obj, WRITE, &repr, DEVICE);
	return obj; /* krc: move */
}

struct object *bus_register(uint32_t bustype, uint32_t busid, size_t bssz)
{
	int r;
	objid_t psid;
	/* TODO: restrict write access. In fact, do this for ALL KSOs. */
	r = syscall_ocreate(0, 0, 0, 0, MIP_DFL_READ | MIP_DFL_WRITE, &psid);
	if(r < 0)
		panic("failed to create bus object: %d", r);
	struct object *obj = obj_lookup(psid, OBJ_LOOKUP_HIDDEN);
	assert(obj != NULL);
	// obj->kaction = __device_kaction;
	obj_kso_init(obj, KSO_DEVBUS);

	struct bus_repr repr = {
		.bus_id = busid,
		.bus_type = bustype,
		.children = (void *)align_up(sizeof(struct bus_repr) + bssz + OBJ_NULLPAGE_SIZE, 16),
	};
	device_rw_header(obj, WRITE, &repr, BUS);
	return obj; /* krc: move */
}

void device_unregister(struct object *obj)
{
	panic("NI - device_unregister %p", obj);
}

struct object *device_get_misc_bus(void)
{
	static struct spinlock lock = SPINLOCK_INIT;
	static struct object *_Atomic misc_bus = NULL;
	if(!misc_bus) {
		spinlock_acquire_save(&lock);
		if(!misc_bus) {
			/* krc: move */
			misc_bus = bus_register(DEVICE_BT_MISC, 0, 0);
			kso_setname(misc_bus, "Misc Bus");
			kso_root_attach(misc_bus, 0, KSO_DEVBUS);
		}
		spinlock_release_restore(&lock);
	}
	return misc_bus;
}
