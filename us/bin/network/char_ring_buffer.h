// This is an implementation of a char ring buffer with FIFO semantics
// It allows *one* producer thread and *one* consumer thread to operate in parallel

#ifndef __CHAR_RING_BUFFER_H__
#define __CHAR_RING_BUFFER_H__

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

typedef struct char_ring_buffer {
    char* buffer;
    uint32_t head;
    uint32_t tail;
    uint32_t capacity;
    clock_t time_of_head_change;
} char_ring_buffer_t;

/* creates a char buffer with capacity 'size' */
char_ring_buffer_t* create_char_ring_buffer(uint32_t size);

/* copies bytes amount of data from src_buf to the tail of buffer
 * returns bytes added -- no partial additions, i.e.,
 * either fully succeds (return bytes) or fully fails (returns 0) */
uint32_t char_ring_buffer_add(char_ring_buffer_t* buffer,
                              char* src_buff,
                              uint32_t bytes);

/* removes a max of bytes amount of data from head of buffer and adds it to dst_buff
 * if bytes > occupied data, removes ocuupied amount of data
 * returns bytes removed */
uint32_t char_ring_buffer_remove(char_ring_buffer_t* buffer,
                                 char* dst_buff,
                                 uint32_t bytes);

/* copies a max of bytes amount of data from index *idx of buffer to dst_buff
 * if bytes > occupied data, copies ocuupied amount of data
 * returns bytes copied */
uint32_t char_ring_buffer_get(char_ring_buffer_t* buffer,
                              char* dst_buff,
                              uint32_t* idx,
                              uint32_t bytes);

/* returns the amount of empty space (in bytes) in buffer */
uint32_t empty_space(char_ring_buffer_t* buffer);

/* returns the amount of occupied space between index *idx and tail of buffer
 * if idx == NULL, uses head of buffer instead */
uint32_t occupied_space(char_ring_buffer_t* buffer,
                        uint32_t* idx);

void free_char_ring_buffer(char_ring_buffer_t* buffer);

#endif
