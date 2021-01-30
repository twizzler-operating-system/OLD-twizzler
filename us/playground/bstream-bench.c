#include <twz/obj.h>
#include <twz/queue.h>

#include <stdio.h>
#include <stdlib.h>
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

int32_t log2_uint32(uint32_t u)
{
	if(u == 0) {
		return INT32_MIN;
	}
	return ((int32_t)31 - (int32_t)__builtin_clz(u));
}

int main(int argc, char **argv)
{
	twzobj obj;
	twz_object_new(
	  &obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_VOLATILE | TWZ_OC_TIED_NONE);

	size_t stride = 8;
	if(argv[1]) {
		stride = atol(argv[1]);
	}

	uint32_t qlen = log2_uint32((1 << 16) / stride);
	printf("::: qlen = %d\n", qlen);
	queue_init_hdr(&obj, qlen, stride, qlen, stride);

	size_t total = 100000;
	if(!fork()) {
		size_t *ls = malloc(sizeof(size_t) * 64);
		double *tm = malloc(sizeof(double) * 64);
		size_t i = 0;
		for(; i < 64; i++) {
			size_t count = 0;

			struct timespec start, end, diff;
			char buf[stride];
			clock_gettime(CLOCK_MONOTONIC, &start);
			while(count < total) {
				queue_receive(&obj, (struct queue_entry *)buf, 0);
				count += 1;
			}
			clock_gettime(CLOCK_MONOTONIC, &end);
			timespec_diff(&start, &end, &diff);
			ls[i] = total * stride;
			tm[i] = get_seconds(diff);
			// fprintf(stderr, "got %ld bytes in %lf seconds\n", ls[i], tm[i]);
		}
		for(unsigned int j = 0; j < i; j++)
			fprintf(stderr, "got %ld bytes in %lf seconds\n", ls[j], tm[j]);
		exit(1);
	}

	for(size_t i = 0; i < 64; i++) {
		size_t count = 0;
		char buf[stride];

		while(count < total) {
			queue_submit(&obj, buf, 0);
			count += 1;
		}
	}
}
