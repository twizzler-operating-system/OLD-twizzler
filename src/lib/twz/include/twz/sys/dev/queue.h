#pragma once

#define __QUEUE_TYPES_ONLY
#include <twz/_queue.h>

#include <twz/objid.h>

struct queue_entry_pager {
	struct queue_entry qe;
	objid_t id;
	objid_t tmpobjid;
	objid_t reqthread;
	uint64_t linaddr;
	uint64_t page;
	uint32_t cmd;
	uint16_t result;
	uint16_t pad;
};

#define PAGER_RESULT_DONE 0
#define PAGER_RESULT_ERROR 1
#define PAGER_RESULT_ZERO 2
#define PAGER_RESULT_COPY 3

#define PAGER_CMD_OBJECT 1
#define PAGER_CMD_OBJECT_PAGE 2

enum bio_result {
	BIO_RESULT_OK,
	BIO_RESULT_ERR,
};

struct queue_entry_bio {
	struct queue_entry qe;
	objid_t tmpobjid;
	uint64_t linaddr;
	uint64_t blockid;
	int result;
	int pad;
};

/* TODO: deprecate */
#define PACKET_CMD_SEND 0
#define PACKET_FLAGS_EOP 1
#define PACKET_FLAGS_INTERNAL_BUF 2

struct packet_queue_entry {
	struct queue_entry qe;
	void *ptr;
	uint32_t flags;
	uint16_t len;
	uint16_t cmd;
};
