/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <ctype.h>
#include <nstack/net.h>
struct netaddr netaddr_from_ipv4_string(const char *str, uint16_t port)
{
	struct netaddr addr;
	addr.type = NETADDR_IPV4;
	addr.port = port;
	addr.pad = 0;
	uint32_t digits[4] = {};
	int index = 0;
	for(const char *c = str; index < 4 && c && *c; c++) {
		if(isdigit(*c)) {
			digits[index] *= 10;
			digits[index] += *c - '0';
		} else {
			index++;
		}
	}
	addr.ipv4 = ((digits[0] & 0xff) << 24) | ((digits[1] & 0xff) << 16) | ((digits[2] & 0xff) << 8)
	            | (digits[3] & 0xff);
	return addr;
}
