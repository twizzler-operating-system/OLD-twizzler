#pragma once

#include <stddef.h>
#include <stdint.h>

#include <twz/objid.h>

#ifdef __cplusplus
extern "C" {
#endif

enum kso_type {
	KSO_NONE,
	KSO_VIEW,
	KSO_SECCTX,
	KSO_THREAD,
	KSO_ROOT,
	KSO_DEVICE,
	KSO_DIRECTORY,
	KSO_DATA,
	KSO_MAX,
};

#define KSO_NAME_MAXLEN 1024

struct kso_attachment {
	objid_t id;
	uint64_t info;
	uint32_t type;
	uint32_t flags;
};

struct kso_hdr {
	char name[KSO_NAME_MAXLEN];
	uint32_t version;
	uint32_t dir_offset;
	uint64_t resv2;
};

struct kso_dir_attachments {
	uint64_t flags;
	size_t count;
	struct kso_attachment children[];
};

struct kso_dir_hdr {
	struct kso_hdr hdr;
	uint64_t flags;
	struct kso_dir_attachments dir;
};

struct kso_root_hdr {
	struct kso_hdr hdr;
	struct kso_dir_attachments dir;
};

#define KSO_ROOT_ID 1

#ifndef __KERNEL__
#include <twz/_types.h>
struct __twzobj;
typedef struct __twzobj twzobj;
int kso_set_name(twzobj *obj, const char *name, ...);
#endif

#ifdef __cplusplus
}
#endif
