#pragma once
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <twz/_objid.h>
#include <unistd.h>

ssize_t __getrandom(void *buf, size_t len, unsigned int flags)
{
	(void)flags;
	int fd = open("/dev/urandom", O_RDONLY);
	if(fd == -1) {
		return -1;
	}
	ssize_t ret = read(fd, buf, len);
	close(fd);
	return ret;
}

objid_t str_to_objid(char *s)
{
	if(!s)
		return 0;
	objid_t res = 0;
	char *o = s;
	for(; *s; s++) {
		if(*s == ':')
			continue;
		if(*s == 'x')
			continue;
		if(*s == '0' && *(s + 1) == 'x')
			continue;
		res <<= 4;
		if(*s >= '0' && *s <= '9')
			res += *s - '0';
		else if(*s >= 'a' && *s <= 'f')
			res += *s - 'a' + 10;
		else if(*s >= 'A' && *s <= 'F')
			res += *s - 'A' + 10;
		else {
			fprintf(stderr, "invalid ID string: %s (%c)\n", o, *s);
			exit(1);
		}
	}
	return res;
}

int str_to_objid_try(char *s, objid_t *id)
{
	objid_t res = 0;
	for(; *s; s++) {
		if(*s == ':')
			continue;
		if(*s == 'x')
			continue;
		if(*s == '0' && *(s + 1) == 'x')
			continue;
		res <<= 4;
		if(*s >= '0' && *s <= '9')
			res += *s - '0';
		else if(*s >= 'a' && *s <= 'f')
			res += *s - 'a' + 10;
		else if(*s >= 'A' && *s <= 'F')
			res += *s - 'A' + 10;
		else {
			return -1;
		}
	}
	*id = res;
	return 0;
}
