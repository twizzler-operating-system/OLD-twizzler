/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

void timespec_diff(struct timespec *start, struct timespec *stop, struct timespec *result)
{
	if((stop->tv_nsec - start->tv_nsec) < 0) {
		result->tv_sec = stop->tv_sec - start->tv_sec - 1;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
	} else {
		result->tv_sec = stop->tv_sec - start->tv_sec;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec;
	}
}

#define get_seconds(t) ({ ((double)(t).tv_nsec / 1000000000.0f) + (double)(t).tv_sec; })

#include <twz/obj.h>

int main()
{
	struct timespec start, end, result;

	twzobj obj;
	int r;
	if((r = twz_object_new(&obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE))) {
		abort();
	}

	void **hdr = (void **)twz_object_base(&obj);
	for(int t = 0; t < 100; t++) {
		clock_gettime(CLOCK_MONOTONIC, &start);
		long i = 0;
		for(i = 0; i < 100000; i++) {
			*hdr = twz_ptr_swizzle(&obj, &obj, FE_READ);
		}
		clock_gettime(CLOCK_MONOTONIC, &end);

		timespec_diff(&start, &end, &result);

		double seconds = get_seconds(result);
		printf(
		  "swz_ext: %lf => %lf ( %lf ns )\n", seconds, seconds / i, (seconds / i) * 1000000000ul);
	}

	for(int t = 0; t < 100; t++) {
		clock_gettime(CLOCK_MONOTONIC, &start);
		long i = 0;
		for(i = 0; i < 100000; i++) {
			*hdr = twz_ptr_swizzle(&obj, (void *)hdr, FE_READ);
		}
		clock_gettime(CLOCK_MONOTONIC, &end);

		timespec_diff(&start, &end, &result);

		double seconds = get_seconds(result);
		printf(
		  "swz_int: %lf => %lf ( %lf ns )\n", seconds, seconds / i, (seconds / i) * 1000000000ul);
	}

#if 0
	for(int t=0;t<100;t++) {
		clock_gettime(CLOCK_MONOTONIC, &start);
		long i = 0;
		for(i = 0; i < 100000; i++) {
			getpid();
		}
		clock_gettime(CLOCK_MONOTONIC, &end);

		timespec_diff(&start, &end, &result);

		double seconds = get_seconds(result);
		printf(
		  "getpid: %lf => %lf ( %lf ns )\n", seconds, seconds / i, (seconds / i) * 1000000000ul);
	}

	int fd = open("/usr/include/stdio.h", O_RDONLY);
	for(int t=0;t<100;t++) {
		clock_gettime(CLOCK_MONOTONIC, &start);
		long i = 0;
		char buf[1];
		for(i = 0; i < 100000; i++) {
			pread(fd, buf, 1, 0);
		}
		clock_gettime(CLOCK_MONOTONIC, &end);

		timespec_diff(&start, &end, &result);

		double seconds = get_seconds(result);
		printf(
		  "read: %lf => %lf ( %lf ns )\n", seconds, seconds / i, (seconds / i) * 1000000000ul);
	}
#endif
}
