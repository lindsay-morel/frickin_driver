/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#include "memx_ringbuffer.h"
#include "memx_common.h"

#include <stdlib.h>



/***************************************************************************//**
 * implementation
 ******************************************************************************/
MemxRingBuffer* memx_ringbuffer_create(uint32_t buffer_size)
{
    MemxRingBuffer *ring_buffer = NULL;
    uint8_t pos = 32;

    // find leading one position in range limited to bit 0 ~ 30
    for (uint8_t i = 0; i < 31; i++) { pos = (buffer_size & (0x1 << i)) ? i : pos; }

    // skip if size zero, otherwise align size to 2^N
    if (pos == 32) { return NULL; }
    buffer_size = 1u << pos;

    // first allocate new object
    ring_buffer = (MemxRingBuffer *)malloc(sizeof(MemxRingBuffer));
    if (!ring_buffer) { return NULL; }

    // allocate data buffer with the given size
    ring_buffer->data = (uint8_t *)malloc(buffer_size);
    if (!ring_buffer->data) {
        free(ring_buffer);
        ring_buffer = NULL;
        return NULL;
    }

    // update mask and pointers
    ring_buffer->size = buffer_size;
    ring_buffer->raw_mask = (buffer_size << 1) - 1;
    ring_buffer->ptr_mask = (buffer_size) - 1;
    ring_buffer->ptr_write = 0;
    ring_buffer->ptr_read = 0;

    return ring_buffer;
}

void memx_ringbuffer_destroy(MemxRingBuffer *ring_buffer)
{
    // do nothing if is null object
    if (!ring_buffer) { return; }

    // release data buffer and object
    free(ring_buffer->data);
    ring_buffer->data = NULL;
    free(ring_buffer);
    ring_buffer = NULL;
}

uint32_t memx_ringbuffer_put(MemxRingBuffer* ring_buffer, uint8_t* source, uint32_t length)
{
    uint32_t ptr_write = 0, ptr_read = 0, ptr_write_masked = 0, ptr_read_masked = 0;
    uint32_t size_empty = 0, size_to_copy = 0;

    // skip if either ring buffer or user buffer is null pointer
    if (!ring_buffer || !source || length == 0) { return 0; }

    // latch pointer from ring buffer
    ptr_write = ring_buffer->ptr_write;
    ptr_read = ring_buffer->ptr_read;
    ptr_write_masked = ptr_write & ring_buffer->ptr_mask;
    ptr_read_masked = ptr_read & ring_buffer->ptr_mask;

    // case ring-buffer full and cannot put data anymore
    if ((ptr_read_masked == ptr_write_masked) && (ptr_read != ptr_write)) { return 0; }

    // clip copy length to space available and check if ring buffer is ring-overed
    size_empty = (ptr_write_masked >= ptr_read_masked) ?
        ring_buffer->size - (ptr_write_masked - ptr_read_masked) :
                    ptr_read_masked - ptr_write_masked;
    size_to_copy = (length > size_empty) ? size_empty : length;

    if (ptr_write_masked >= ptr_read_masked) {
        // case ring-over, separate it into 2 memcpys, includes 'empty' case
        //  +-------+-----------------------+-------+
        //  | avail |         used          | avail |
        //  +-------+-----------------------+-------+
        //  0       r                       w     size-1=mask
        uint32_t size_chunk = ring_buffer->size - ptr_write_masked;
        if (size_to_copy > size_chunk) {
            memcpy(ring_buffer->data + ptr_write_masked, source, size_chunk);
            memcpy(ring_buffer->data, source + size_chunk, size_to_copy - size_chunk);
        } else {
            memcpy(ring_buffer->data + ptr_write_masked, source, size_to_copy);
        }
    } else {
        // case normal, one memcpy can do the work
        //  +------+-------------------------+------+
        //  | used |        available        | used |
        //  +------+-------------------------+------+
        //  0      w                         r    size-1=mask
        memcpy(ring_buffer->data + ptr_write_masked, source, size_to_copy);
    }

    // update write pointer and return size copied
    ring_buffer->ptr_write = (ptr_write + size_to_copy) & ring_buffer->raw_mask;
    return size_to_copy;
}

uint32_t memx_ringbuffer_get(MemxRingBuffer* ring_buffer, uint8_t* target, uint32_t length)
{
    uint32_t ptr_write = 0, ptr_read = 0, ptr_write_masked = 0, ptr_read_masked = 0;
    uint32_t size_used = 0, size_to_copy = 0;


    // skip if either ring buffer or user buffer is null pointer
    if (!ring_buffer || !target || length == 0) { return 0; }

    // latch pointer from ring buffer
    ptr_write = ring_buffer->ptr_write;
    ptr_read = ring_buffer->ptr_read;
    ptr_write_masked = ptr_write & ring_buffer->ptr_mask;
    ptr_read_masked = ptr_read & ring_buffer->ptr_mask;

    // case ring-buffer is empty and there is nothing to copy
    if ((ptr_read_masked == ptr_write_masked) && (ptr_read == ptr_write)) { return 0; }

    // clip copy length to space used and check if ring buffer is ring-overed
    size_used = (ptr_write_masked > ptr_read_masked) ?
                ptr_write_masked - ptr_read_masked :
                ring_buffer->size - (ptr_read_masked - ptr_write_masked);

    size_to_copy = (length > size_used) ? size_used : length;

    if (ptr_write_masked > ptr_read_masked) {
        // case normal, one memcpy can do the work
        //  +-------+-----------------------+-------+
        //  | avail |         used          | avail |
        //  +-------+-----------------------+-------+
        //  0       r                       w     size-1=mask
        memcpy(target, ring_buffer->data+ptr_read_masked, size_to_copy);
    } else {
        // case ring-over, separate it into 2 memcpys, includes 'full' case
        //  +------+-------------------------+------+
        //  | used |        available        | used |
        //  +------+-------------------------+------+
        //  0      w                         r    size-1=mask
        uint32_t size_chunk = ring_buffer->size - ptr_read_masked;
        if (size_to_copy > size_chunk) {
            memcpy(target, ring_buffer->data + ptr_read_masked, size_chunk);
            memcpy(target + size_chunk, ring_buffer->data, size_to_copy - size_chunk);
        } else {
            memcpy(target, ring_buffer->data + ptr_read_masked, size_to_copy);
        }
    }

    // update read pointer and return size copied
    ring_buffer->ptr_read = (ptr_read + size_to_copy) & ring_buffer->raw_mask;
    return size_to_copy;
}

uint32_t memx_ringbuffer_drop(MemxRingBuffer* ring_buffer, uint32_t length)
{
    uint32_t ptr_write = 0, ptr_read = 0, ptr_write_masked = 0, ptr_read_masked = 0;
    uint32_t size_used = 0, size_to_copy = 0;

    // skip if ring buffer is null pointer
    if (!ring_buffer || length == 0) { return 0; }

    // latch pointer from ring buffer
    ptr_write = ring_buffer->ptr_write;
    ptr_read = ring_buffer->ptr_read;
    ptr_write_masked = ptr_write & ring_buffer->ptr_mask;
    ptr_read_masked = ptr_read & ring_buffer->ptr_mask;

    // case ring-buffer is empty and there is nothing to drop
    if ((ptr_read_masked == ptr_write_masked) && (ptr_read == ptr_write)) { return 0; }

    // clip copy length to space used
    size_used = (ptr_write_masked > ptr_read_masked) ?
        ptr_write_masked - ptr_read_masked :
        ring_buffer->size - (ptr_read_masked - ptr_write_masked);
    size_to_copy = (length > size_used) ? size_used : length;

    // update read pointer directly and return size dropped
    ring_buffer->ptr_read = (ptr_read + size_to_copy) & ring_buffer->raw_mask;
    return size_to_copy;
}

uint32_t memx_ringbuffer_peek_usage(MemxRingBuffer* ring_buffer)
{
    uint32_t ptr_write = 0, ptr_read = 0, ptr_write_masked = 0, ptr_read_masked = 0;
    // skip if ring buffer is null pointer
    if (!ring_buffer) { return 0; }

    // latch pointer from ring buffer
    ptr_write = ring_buffer->ptr_write;
    ptr_read = ring_buffer->ptr_read;
    ptr_write_masked = ptr_write & ring_buffer->ptr_mask;
    ptr_read_masked = ptr_read & ring_buffer->ptr_mask;

    // return size in case full, otherwise return difference
    if ((ptr_read_masked == ptr_write_masked) && (ptr_read != ptr_write)) { return ring_buffer->size; }

    if (ptr_write_masked >= ptr_read_masked) {
        return (ptr_write_masked - ptr_read_masked);
    } else {
        return (ring_buffer->size - (ptr_read_masked - ptr_write_masked));
    }
}

uint32_t memx_ringbuffer_peek_space(MemxRingBuffer *ring_buffer)
{
    uint32_t ptr_write = 0, ptr_read = 0, ptr_write_masked = 0, ptr_read_masked = 0;
    // skip if ring buffer is null pointer
    if (!ring_buffer) { return 0; }

    // latch pointer from ring buffer
    ptr_write = ring_buffer->ptr_write;
    ptr_read = ring_buffer->ptr_read;
    ptr_write_masked = ptr_write & ring_buffer->ptr_mask;
    ptr_read_masked = ptr_read & ring_buffer->ptr_mask;

    // return 0 in case full, otherwise return difference
    if ((ptr_read_masked == ptr_write_masked) && (ptr_read != ptr_write)) { return 0; }

    if (ptr_write_masked >= ptr_read_masked) {
        return (ring_buffer->size - (ptr_write_masked - ptr_read_masked));
    } else {
        return (ptr_read_masked - ptr_write_masked);
    }
}

