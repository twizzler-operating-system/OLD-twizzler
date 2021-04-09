#pragma once
#include <stdint.h>

enum {
	NETADDR_UNKNOWN,
	NETADDR_IPV4,
	NUM_NETADDR_TYPES,
};

/* TODO: figure out how we want to represent an address */
struct netaddr {
	uint32_t type;
	union {
		char addr[28];
		uint32_t ipv4;
		unsigned __int128 ipv6;
	};
	uint16_t port;
	uint16_t pad;
};
