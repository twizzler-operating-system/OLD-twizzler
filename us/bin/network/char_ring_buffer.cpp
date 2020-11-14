#include "char_ring_buffer.h"


char_ring_buffer_t* create_char_ring_buffer(uint32_t size)
{
    char_ring_buffer_t* buffer = (char_ring_buffer_t *)
        malloc(sizeof(char_ring_buffer_t));
    buffer->buffer = (char *)malloc(sizeof(char)*size);
    buffer->head = 1;
    buffer->tail = 1;
    buffer->capacity = size;
    buffer->time_of_head_change = 0;
    return buffer;
}


uint32_t char_ring_buffer_add(char_ring_buffer_t* buffer,
                              char* src_buff,
                              uint32_t bytes)
{
    if (buffer == NULL) {
        fprintf(stderr, "Error char_ring_buffer_add: buffer is NULL\n");
        exit(1);
    }

    uint32_t empty = empty_space(buffer);
    if (bytes == 0 || bytes > empty) {
        return 0;
    }

    uint32_t start_idx = buffer->tail % buffer->capacity;

    if (src_buff != NULL) {
        if (start_idx + bytes <= buffer->capacity) {
            memcpy((buffer->buffer + start_idx), src_buff, bytes);
        } else {
            uint32_t diff = bytes - (buffer->capacity - start_idx);
            memcpy((buffer->buffer + start_idx), src_buff,
                    (buffer->capacity - start_idx));
            memcpy(buffer->buffer, src_buff + (buffer->capacity - start_idx), diff);
        }

        buffer->tail += bytes;
        assert(buffer->head <= buffer->tail);
        assert(buffer->tail - buffer->head <= buffer->capacity);

        return bytes;

    } else {
        return 0;
    }
}


uint32_t char_ring_buffer_remove(char_ring_buffer_t* buffer,
                                 char* dst_buff,
                                 uint32_t bytes)
{
    if (buffer == NULL) {
        fprintf(stderr, "Error char_ring_buffer_add: buffer is NULL\n");
        exit(1);
    }

    uint32_t occupied = occupied_space(buffer, NULL);
    if (bytes > occupied) {
        bytes = occupied;
    }

    uint32_t start_idx = buffer->head % buffer->capacity;

    if (dst_buff != NULL && bytes > 0) {
        if (start_idx + bytes <= buffer->capacity) {
            memcpy(dst_buff, (buffer->buffer + start_idx), bytes);
        } else {
            uint32_t diff = bytes - (buffer->capacity - start_idx);
            memcpy(dst_buff, (buffer->buffer + start_idx),
                    (buffer->capacity - start_idx));
            memcpy(dst_buff + (buffer->capacity - start_idx), buffer->buffer, diff);
        }
    }

    buffer->head += bytes;
    assert(buffer->head <= buffer->tail);
    assert(buffer->tail - buffer->head <= buffer->capacity);

    return bytes;
}


uint32_t char_ring_buffer_get(char_ring_buffer_t* buffer,
                              char* dst_buff,
                              uint32_t* idx,
                              uint32_t bytes)
{
    if (buffer == NULL) {
        fprintf(stderr, "Error char_ring_buffer_add: buffer is NULL\n");
        exit(1);
    }

    uint32_t head = (idx == NULL) ? buffer->head : *idx;

    if (head < buffer->head || head >= buffer->tail) {
        return 0;
    }

    uint32_t occupied = buffer->tail - head;
    if (bytes > occupied) {
        bytes = occupied;
    }

    uint32_t start_idx = head % buffer->capacity;

    if (dst_buff != NULL && bytes > 0) {
        if (start_idx + bytes <= buffer->capacity) {
            memcpy(dst_buff, (buffer->buffer + start_idx), bytes);
        } else {
            uint32_t diff = bytes - (buffer->capacity - start_idx);
            memcpy(dst_buff, (buffer->buffer + start_idx),
                    (buffer->capacity - start_idx));
            memcpy(dst_buff + (buffer->capacity - start_idx), buffer->buffer, diff);
        }
    }

    return bytes;
}


uint32_t empty_space(char_ring_buffer_t* buffer)
{
    if (buffer == NULL) {
        fprintf(stderr, "Error char_ring_buffer_add: buffer is NULL\n");
        exit(1);
    }

    assert(buffer->head <= buffer->tail);
    uint32_t occupied = buffer->tail - buffer->head;
    assert(occupied <= buffer->capacity);

    return (buffer->capacity - occupied);
}


uint32_t occupied_space(char_ring_buffer_t* buffer,
                        uint32_t* idx)
{
    if (buffer == NULL) {
        fprintf(stderr, "Error char_ring_buffer_add: buffer is NULL\n");
        exit(1);
    }

    uint32_t head = (idx == NULL) ? buffer->head : *idx;

    assert(head <= buffer->tail);
    uint32_t occupied = buffer->tail - head;

    return occupied;
}


void free_char_ring_buffer(char_ring_buffer_t* buffer)
{
    if (buffer != NULL) {
        free(buffer->buffer);
        free(buffer);
    }
}

///////////////////////////////////////////////////////////////////////////////////
//                                  For Unit Testing                             //
///////////////////////////////////////////////////////////////////////////////////

#ifdef UNIT_TEST
void* producer(void* p)
{
    char_ring_buffer_t* buffer = (char_ring_buffer_t *)p;

    while (char_ring_buffer_add(buffer, "This is", 7) != 7) usleep(10);
    while (char_ring_buffer_add(buffer, " an a", 5) != 5) usleep(10);
    while (char_ring_buffer_add(buffer, "mazing pla", 10) != 10) usleep(10);
    while (char_ring_buffer_add(buffer, "ce", 2) != 2) usleep(10);
}

void* consumer(void* p)
{
    char_ring_buffer_t* buffer = (char_ring_buffer_t *)p;

    char dest[24];

    while (char_ring_buffer_remove(buffer, dest, 4) != 4) usleep(10);
    while (char_ring_buffer_remove(buffer, dest+4, 8) != 8) usleep(10);
    while (char_ring_buffer_remove(buffer, dest+12, 5) != 5) usleep(10);
    while (char_ring_buffer_remove(buffer, dest+17, 4) != 4) usleep(10);
    while (char_ring_buffer_remove(buffer, dest+21, 3) != 3) usleep(10);

    printf("%s\n", dest);
}

int main()
{
    char_ring_buffer_t* buffer = create_char_ring_buffer(10);

    pthread_t t1, t2;
    pthread_create(&t1, NULL, &producer, buffer);
    pthread_create(&t2, NULL, &consumer, buffer);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    return 0;
}
#endif
