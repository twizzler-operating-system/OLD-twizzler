#pragma once

#include <twz/sys/kso.h>

enum {
	MISC_FRAMEBUFFER_TYPE_UNKNOWN,
	MISC_FRAMEBUFFER_TYPE_TEXT,
	MISC_FRAMEBUFFER_TYPE_GRAPHICAL,
};

struct misc_framebuffer {
	struct kso_hdr hdr;
	uint64_t offset;
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	uint16_t flags;
	uint8_t bpp;
	uint8_t type;
};
