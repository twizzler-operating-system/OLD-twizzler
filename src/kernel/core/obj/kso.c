/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <device.h>
#include <kso.h>
#include <object.h>
#include <spinlock.h>
#include <twz/sys/dev/bus.h>
#include <twz/sys/kso.h>
#include <twz/sys/thread.h>
#include <vmm.h>
/* TODO: better system for tracking slots in the array */
static _Atomic size_t idx = 0;
static struct spinlock lock = SPINLOCK_INIT;

int kso_root_attach(struct object *obj, uint64_t flags, int type)
{
	(void)flags;
	struct object *root = obj_lookup(1, 0);
	spinlock_acquire_save(&lock);
	struct kso_attachment kar = {
		.flags = 0,
		.id = obj->id,
		.info = 0,
		.type = type,
	};
	size_t i = idx++;
	obj_write_data(root,
	  offsetof(struct kso_root_repr, attached) + sizeof(struct kso_attachment) * i,
	  sizeof(kar),
	  &kar);

	i++;
	obj_write_data(root, offsetof(struct kso_root_repr, count), sizeof(i), &i);

	spinlock_release_restore(&lock);
	obj_put(root);
	return i - 1;
}

void kso_root_detach(int i)
{
	struct object *root = obj_lookup(1, 0);
	spinlock_acquire_save(&lock);
	struct kso_attachment kar = {
		.flags = 0,
		.id = 0,
		.info = 0,
		.type = 0,
	};
	obj_write_data(root,
	  offsetof(struct kso_root_repr, attached) + sizeof(struct kso_attachment) * i,
	  sizeof(kar),
	  &kar);

	spinlock_release_restore(&lock);
	obj_put(root);
}

/* TODO: detach threads when they exit */

void kso_attach(struct object *parent, struct object *child, size_t loc)
{
	assert(parent->kso_type);
	struct kso_attachment kar = {
		.type = child->kso_type,
		.id = child->id,
		.info = 0,
		.flags = 0,
	};
	switch(atomic_load(&parent->kso_type)) {
		size_t off;
		struct bus_repr brepr;
		case KSO_DEVBUS:
			device_rw_header(parent, READ, &brepr, BUS);
			off = (size_t)brepr.children - OBJ_NULLPAGE_SIZE;
			obj_write_data(parent, off + loc * sizeof(kar), sizeof(kar), &kar);
			if(brepr.max_children <= loc) {
				brepr.max_children = loc + 1;
				device_rw_header(parent, WRITE, &brepr, BUS);
			}
			break;
		default:
			panic("NI - kso_attach");
	}
}

#include <string.h>
void kso_setname(struct object *obj, const char *name)
{
	obj_write_data(obj, offsetof(struct kso_hdr, name), strlen(name) + 1, (void *)name);
}

void kso_view_write(struct object *obj, size_t slot, struct viewentry *ve)
{
	obj_write_data(
	  obj, __VE_OFFSET + slot * sizeof(struct viewentry), sizeof(struct viewentry), ve);
}

void *object_get_kso_data_checked(struct object *obj, enum kso_type kt)
{
	if(obj->kso_type == kt)
		return obj->kso_data;
	if(obj->kso_type == KSO_NONE) {
		spinlock_acquire_save(&obj->lock);
		if(obj->kso_type == KSO_NONE) {
			object_init_kso_data(obj, kt);
		}
		spinlock_release_restore(&obj->lock);
		return obj->kso_data;
	}
	return NULL;
}
static struct kso_calls *_kso_calls[KSO_MAX];

void kso_register(int t, struct kso_calls *c)
{
	_kso_calls[t] = c;
}

void kso_detach_event(struct thread *thr, bool entry, int sysc)
{
	for(size_t i = 0; i < KSO_MAX; i++) {
		if(_kso_calls[i] && _kso_calls[i]->detach_event) {
			_kso_calls[i]->detach_event(thr, entry, sysc);
		}
	}
}

struct kso_calls *kso_lookup_calls(enum kso_type ksot)
{
	return _kso_calls[ksot];
}

void obj_kso_init(struct object *obj, enum kso_type ksot)
{
	obj->kso_type = ksot;
	obj->kso_calls = _kso_calls[ksot];
	if(obj->kso_calls && obj->kso_calls->ctor) {
		obj->kso_calls->ctor(obj);
	}
}

void object_init_kso_data(struct object *obj, enum kso_type kt)
{
	obj->kso_type = kt;
	obj->kso_calls = _kso_calls[kt];
	if(obj->kso_calls && obj->kso_calls->ctor) {
		obj->kso_calls->ctor(obj);
	}
}
