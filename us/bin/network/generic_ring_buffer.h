// This is an implementation of a generic ring buffer with FIFO semantics
// It stores elements of type void*
// It allows *one* producer thread and *one* consumer thread to operate in parallel

#ifndef __GENERIC_RING_BUFFER_H__
#define __GENERIC_RING_BUFFER_H__

//#define UNIT_TEST

#ifdef UNIT_TEST
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#else
#include "common.h"
#endif

typedef struct generic_ring_buffer {
    void** buffer;
    uint32_t head;
    uint32_t tail;
    uint32_t capacity;
} generic_ring_buffer_t;

/* creates a generic ring buffer with capacity 'size' */
generic_ring_buffer_t* create_generic_ring_buffer(uint32_t size);

/* adds element 'element' to the tail of ring buffer */
uint32_t generic_ring_buffer_add(generic_ring_buffer_t* buffer,
                                 void* element);

/* removes and returns the element at the head of ring buffer */
void* generic_ring_buffer_remove(generic_ring_buffer_t* buffer);

/* returns the number of elements in ring buffer */
uint32_t num_of_elements(generic_ring_buffer_t* buffer);

/* destroys the ring buffer */
void free_generic_ring_buffer(generic_ring_buffer_t* buffer);

#endif
