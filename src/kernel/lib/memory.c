/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>

void *memset(void *ptr, int c, size_t len)
{
	char *p = ptr;
	while(len--) {
		*p++ = c;
	}
	return ptr;
}

size_t strlen(const char *s)
{
	size_t c = 0;
	while(*s++)
		c++;
	return c;
}

/*
 * sizeof(word) MUST BE A POWER OF TWO
 * SO THAT wmask BELOW IS ALL ONES
 */
typedef long word; /* "word" used for optimal copy speed */

#define wsize sizeof(word)
#define wmask (wsize - 1)

/* From FreeBSD.
 * Copy a block of memory, handling overlap.
 * This is the routine that actually implements
 * (the portable versions of) bcopy, memcpy, and memmove.
 */
void *memcpy(void *dst0, const void *src0, size_t length)
{
	char *dst;
	const char *src;

	size_t t;

	dst = dst0;
	src = src0;

	if(length == 0 || dst == src) { /* nothing to do */
		goto done;
	}

	/*
	 * Macros: loop-t-times; and loop-t-times, t>0
	 */
#define TLOOP(s)                                                                                   \
	if(t)                                                                                          \
	TLOOP1(s)
#define TLOOP1(s)                                                                                  \
	do {                                                                                           \
		s;                                                                                         \
	} while(--t)

	if((unsigned long)dst < (unsigned long)src) {
		/*
		 * Copy forward.
		 */
		t = (size_t)src; /* only need low bits */

		if((t | (uintptr_t)dst) & wmask) {
			/*
			 * Try to align operands.  This cannot be done
			 * unless the low bits match.
			 */
			if((t ^ (uintptr_t)dst) & wmask || length < wsize) {
				t = length;
			} else {
				t = wsize - (t & wmask);
			}

			length -= t;
			TLOOP1(*dst++ = *src++);
		}
		/*
		 * Copy whole words, then mop up any trailing bytes.
		 */
		t = length / wsize;
		TLOOP(*(word *)dst = *(const word *)src; src += wsize; dst += wsize);
		t = length & wmask;
		TLOOP(*dst++ = *src++);
	} else {
		/*
		 * Copy backwards.  Otherwise essentially the same.
		 * Alignment works as before, except that it takes
		 * (t&wmask) bytes to align, not wsize-(t&wmask).
		 */
		src += length;
		dst += length;
		t = (uintptr_t)src;

		if((t | (uintptr_t)dst) & wmask) {
			if((t ^ (uintptr_t)dst) & wmask || length <= wsize) {
				t = length;
			} else {
				t &= wmask;
			}

			length -= t;
			TLOOP1(*--dst = *--src);
		}
		t = length / wsize;
		TLOOP(src -= wsize; dst -= wsize; *(word *)dst = *(const word *)src);
		t = length & wmask;
		TLOOP(*--dst = *--src);
	}
done:
	return (dst0);
}

char *strnchr(char *s, int c, size_t n)
{
	while(n--) {
		char *t = s++;
		if(*t == c)
			return t;
		if(*t == 0)
			return NULL;
	}
	return NULL;
}

char *strncpy(char *d, const char *s, size_t n)
{
	char *_d = d;
	while(n-- && (*d++ = *s++))
		;
	return _d;
}

int memcmp(const void *ptr1, const void *ptr2, size_t num)
{
	const unsigned char *vptr1 = (const unsigned char *)ptr1;
	const unsigned char *vptr2 = (const unsigned char *)ptr2;
	while(num) {
		if(*vptr1 > *vptr2)
			return 1;
		else if(*vptr1 < *vptr2)
			return -1;
		vptr1++;
		vptr2++;
		num--;
	}
	return 0;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
	while(n) {
		if(*s1 > *s2)
			return 1;
		else if(*s1 < *s2)
			return -1;
		else if(!*s1 && !*s2)
			return 0;
		s1++;
		s2++;
		n--;
	}
	return 0;
}

int strcmp(const char *s1, const char *s2)
{
	while(true) {
		if(*s1 > *s2)
			return 1;
		else if(*s1 < *s2)
			return -1;
		else if(!*s1 && !*s2)
			return 0;
		s1++;
		s2++;
	}
}

long strtol(char *str, char **end, int base)
{
	long tmp = 0;
	bool neg = false;
	if(*str == '-') {
		neg = true;
		str++;
	}
	if(*str == '+')
		str++;

	while(*str) {
		if(*str >= '0' && *str <= '0' + (base - 1)) {
			tmp *= base;
			tmp += *str - '0';
		} else if(*str >= 'a' && *str < 'a' + (base - 10)) {
			tmp *= base;
			tmp += *str - 'a';
		} else if(*str >= 'A' && *str < 'A' + (base - 10)) {
			tmp *= base;
			tmp += *str - 'A';
		} else {
			break;
		}

		str++;
	}

	if(end)
		*end = str;

	return neg ? -tmp : tmp;
}
