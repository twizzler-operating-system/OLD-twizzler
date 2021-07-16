/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <device.h>
#include <kso.h>
#include <object.h>
#include <spinlock.h>
#include <twz/sys/dev/device.h>
#include <twz/sys/kso.h>
#include <twz/sys/thread.h>
#include <vmm.h>

static size_t get_doff(int type)
{
	size_t doff = 0;
	switch(type) {
		case KSO_DIRECTORY:
			doff = offsetof(struct kso_dir_hdr, dir);
			break;
		case KSO_ROOT:
			doff = offsetof(struct kso_root_hdr, dir);
			break;
		case KSO_DEVICE:
			doff = offsetof(struct kso_device_hdr, dir);
			break;
		default:
			break;
	}
	return doff;
}

static struct kso_dir *object_get_dir(struct object *obj)
{
	assert(obj->kso_type != KSO_NONE);
	if(obj->dir == NULL) {
		spinlock_acquire_save(&obj->lock);
		if(obj->dir == NULL) {
			obj->dir = kalloc(sizeof(struct kso_dir), 0);
			vector_init(&obj->dir->idxs, sizeof(uint64_t), _Alignof(uint64_t));
			obj->dir->max = 0;
			obj->dir->lock = SPINLOCK_INIT;
			obj->dir->doff = get_doff(obj->kso_type);
			if(obj->dir->doff == 0) {
				panic("unsupported dir type");
			}
		}
		spinlock_release_restore(&obj->lock);
	}
	return obj->dir;
}

void object_kso_dir_destroy(struct object *obj)
{
	if(obj->dir) {
		vector_destroy(&obj->dir->idxs);
		kfree(obj->dir);
		obj->dir = NULL;
	}
}

struct object *object_get_kso_root(void)
{
	static struct object *_Atomic root = NULL;
	static struct spinlock lock = SPINLOCK_INIT;
	if(root == NULL) {
		spinlock_acquire_save(&lock);
		if(root == NULL) {
			root = obj_lookup(KSO_ROOT_ID, 0);
		}
		spinlock_release_restore(&lock);
	}
	return root;
}

void kso_tree_detach_child(struct object *parent, size_t chnr)
{
	struct kso_dir *kd = object_get_dir(parent);
	assert(kd);

	struct kso_attachment kat = {
		.flags = 0,
		.id = 0,
		.info = 0,
		.type = 0,
	};
	obj_write_data(parent,
	  kd->doff + sizeof(struct kso_dir_attachments) + sizeof(struct kso_attachment) * chnr,
	  sizeof(kat),
	  &kat);
	spinlock_acquire_save(&kd->lock);

	uint64_t _n = chnr;
	vector_push(&kd->idxs, &_n);

	spinlock_release_restore(&kd->lock);
}

static void _attach_init_obj_dir(struct object *obj)
{
	uint32_t doff = get_doff(obj->kso_type);
	if(!doff)
		return;
	obj_write_data(obj, offsetof(struct kso_hdr, dir_offset), 4, &doff);
}

size_t kso_tree_attach_child(struct object *parent, struct object *child, uint64_t info)
{
	assert(child->kso_type != KSO_NONE);
	_attach_init_obj_dir(child);
	struct kso_attachment kat = {
		.type = child->kso_type,
		.id = child->id,
		.info = info,
		.flags = 0,
	};

	struct kso_dir *kd = object_get_dir(parent);
	assert(kd);

	spinlock_acquire_save(&kd->lock);
	size_t next;
	if(kd->idxs.length == 0) {
		next = kd->max++;
	} else {
		uint64_t *n = vector_pop(&kd->idxs);
		assert(n);
		next = *n;
	}

	obj_write_data(parent,
	  kd->doff + sizeof(struct kso_dir_attachments) + sizeof(struct kso_attachment) * next,
	  sizeof(kat),
	  &kat);

	struct kso_dir_attachments hdr;
	obj_read_data(parent, kd->doff, sizeof(hdr), &hdr);
	if(hdr.count != kd->max) {
		hdr.count = kd->max;
		obj_write_data(parent, kd->doff, sizeof(hdr), &hdr);
	}
	spinlock_release_restore(&kd->lock);
	return next;
}

#include <string.h>
void kso_setname(struct object *obj, const char *name)
{
	obj_write_data(obj, offsetof(struct kso_hdr, name), strlen(name) + 1, (void *)name);
}

void kso_setnamef(struct object *obj, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);

	char buf[1024];
	vsnprintf(buf, sizeof(buf), fmt, va);
	va_end(va);

	kso_setname(obj, buf);
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

static struct kso_calls *_kso_calls[KSO_MAX] = {};

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
