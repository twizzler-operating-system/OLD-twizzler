/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>
#include <memory.h>
#include <object.h>
#include <processor.h>
#include <vmm.h>
#ifdef __clang__
__attribute__((no_sanitize("alignment")))
#else
__attribute__((no_sanitize_undefined))
#endif
bool arch_debug_unwind_frame(struct frame *frame, bool userspace)
{
	if(frame->fp == 0)
		return false;
	if(userspace) {
		if(frame->fp >= KERNEL_REGION_START)
			return false;
		if(frame->fp > USER_REGION_END)
			return false;
		if(frame->fp % OBJ_MAXSIZE < OBJ_NULLPAGE_SIZE)
			return false;
		struct object *obj = vm_context_lookup_object(current_thread->ctx, frame->fp);
		obj_put(obj);
		if(!obj)
			return false;
	} else {
		if(frame->fp < KERNEL_REGION_START)
			return false;
	}
	void *fp = (void *)frame->fp;
	frame->fp = *(uintptr_t *)(fp);
	frame->pc = *(uintptr_t *)((uintptr_t)fp + 8) - 5;
	return true;
}
