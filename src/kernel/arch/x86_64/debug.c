#include <debug.h>
#include <memory.h>
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
		/* TODO: verify mapping */
	} else {
		if(frame->fp < KERNEL_REGION_START)
			return false;
	}
	void *fp = (void *)frame->fp;
	frame->fp = *(uintptr_t *)(fp);
	frame->pc = *(uintptr_t *)((uintptr_t)fp + 8) - 5;
	return true;
}
