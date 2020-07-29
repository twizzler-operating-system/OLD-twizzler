#pragma once

#ifdef __cplusplus
#include <atomic>
using std::atomic_uint_least64_t;
#else /* not __cplusplus */
#include <stdatomic.h>
#endif /* __cplusplus */

#define NIC_FL_UP 1
#define NIC_FL_MAC_VALID 2

struct nic_header {
	atomic_uint_least64_t flags;
	uint8_t mac[6];
	uint8_t pad[2];
};
