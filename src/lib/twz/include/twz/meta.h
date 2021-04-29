#pragma once

#include <stddef.h>
#include <stdint.h>
#include <twz/_types.h>

#include <twz/obj.h>

#ifdef __cplusplus
#include <atomic>
extern "C" {
#endif

typedef unsigned __int128 nonce_t;

struct metaext {
#ifdef __cplusplus
	std::
#endif
	  atomic_uint_least64_t tag;
#ifdef __cplusplus
	std::atomic<void *> ptr;
#else
	void *_Atomic ptr;
#endif
};

#define MI_MAGIC 0x54575A4F

#define MIF_SZ 0x1

#define MIP_HASHDATA 0x1
#define MIP_DFL_READ 0x4
#define MIP_DFL_WRITE 0x8
#define MIP_DFL_EXEC 0x10
#define MIP_DFL_USE 0x20
#define MIP_DFL_DEL 0x40

struct metainfo {
	uint32_t magic;
	uint16_t flags;
	uint16_t p_flags;
#ifdef __cplusplus
	std::atomic_uint_least32_t fotentries;
#else
	_Atomic uint32_t fotentries;
#endif
	uint32_t milen;
	nonce_t nonce;
	objid_t kuid;
	uint64_t sz;
	uint64_t pad;
	_Alignas(16) struct metaext exts[];
} __attribute__((packed));

#define FE_READ 0x4
#define FE_WRITE 0x8
#define FE_EXEC 0x10
#define FE_USE 0x20
#define FE_NAME 0x1000
#define FE_DERIVE 0x2000

struct fotentry {
	union {
		objid_t id;
		struct {
			char *data;
			void *nresolver;
		} name;
	};

#ifdef __cplusplus
	std::atomic_uint_least64_t flags;
#else
	_Atomic uint64_t flags;
#endif
	uint64_t info;
};

#ifndef __cplusplus
_Static_assert(sizeof(struct fotentry) == 32, "");
#endif

#define OBJ_MAXFOTE ((1ul << 20) - 0x1000 / sizeof(struct fotentry))
#define OBJ_TOPDATA (OBJ_MAXSIZE - (0x1000 + OBJ_MAXFOTE * sizeof(struct fotentry)))

#ifndef __cplusplus
_Static_assert(OBJ_TOPDATA == 0x3E000000, "");
#endif

#ifndef __KERNEL__
struct __twzobj;
typedef struct __twzobj twzobj;

struct metainfo *twz_object_meta(twzobj *);

void *twz_object_getext(twzobj *obj, uint64_t tag);
int twz_object_addext(twzobj *obj, uint64_t tag, void *ptr);
int twz_object_delext(twzobj *obj, uint64_t tag, void *ptr);

enum twz_object_setsz_mode {
	TWZ_OSSM_ABSOLUTE,
	TWZ_OSSM_RELATIVE,
};

void twz_object_setsz(twzobj *obj, enum twz_object_setsz_mode mode, ssize_t amount);

#endif

#ifdef __cplusplus
}
#endif
