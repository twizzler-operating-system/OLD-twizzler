#include "generic_ring_buffer.h"


generic_ring_buffer_t* create_generic_ring_buffer(uint32_t size)
{
    generic_ring_buffer_t* buffer = (generic_ring_buffer_t *)
        malloc(sizeof(generic_ring_buffer_t));
    buffer->buffer = (void **)malloc(sizeof(void*)*size);
    buffer->head = 0;
    buffer->tail = 0;
    buffer->capacity = size;
    return buffer;
}


uint32_t generic_ring_buffer_add(generic_ring_buffer_t* buffer,
                                 void* element)
{
    if (buffer == NULL) {
        fprintf(stderr, "Error generic_ring_buffer_add: buffer is NULL\n");
        exit(1);
    }

    if (num_of_elements(buffer) < buffer->capacity) {
        buffer->buffer[buffer->tail % buffer->capacity] = element;
        buffer->tail += 1;
        return 0;
    }

    return 1;
}


void* generic_ring_buffer_remove(generic_ring_buffer_t* buffer)
{
    if (buffer == NULL) {
        fprintf(stderr, "Error generic_ring_buffer_remove: buffer is NULL\n");
        exit(1);
    }

    if (num_of_elements(buffer) > 0) {
        void* element = buffer->buffer[buffer->head % buffer->capacity];
        buffer->head += 1;
        return element;
    }

    return NULL;
}


uint32_t num_of_elements(generic_ring_buffer_t* buffer)
{
    if (buffer == NULL) {
        fprintf(stderr, "Error generic_ring_buffer num_of_elements: "
                "buffer is NULL\n");
        exit(1);
    }

    assert(buffer->head <= buffer->tail);
    uint32_t num = buffer->tail - buffer->head;
    assert(num <= buffer->capacity);
    return num;
}


void free_generic_ring_buffer(generic_ring_buffer_t* buffer)
{
    if (buffer != NULL) {
        free(buffer->buffer);
        free(buffer);
    }
}
