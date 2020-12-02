#pragma once
#include <stdint.h>

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
