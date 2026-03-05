/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#ifndef MEMX_RINGBUFFER_H_
#define MEMX_RINGBUFFER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "memx_common.h"
/**
 * @brief Asynchronous circular buffer in order to transfer data between two
 * different layers without mutex locking. For example, transfering data from
 * low level interface buffer to user buffer.
 */
typedef struct _MemxRingBuffer {
  uint8_t *data;      /* pointer to byte buffer where data is actually stored */
  uint32_t size;      /* byte buffer size, should be aligned to 2^N in order to do easy write/read position masking */
  uint32_t raw_mask;  /* write mask with additional one bit to distinguish between buffer full and empty when 'ptr_write' equals to 'ptr_read' */
  uint32_t ptr_mask;  /* position mask to calculate relationship between 'ptr_write' and 'ptr_read' */
  uint32_t ptr_write; /* write pointer which is updated by producer */
  uint32_t ptr_read;  /* read pointer which is updated by consumer */
} MemxRingBuffer;

/**
 * @brief Create and allocate memory space for a new ring buffer. 'buffer_size'
 * should be aligned to 2^N and be limited to maximum size 2^31 for additional
 * one bit is required to do raw pointer masking.
 *
 * @param buffer_size         buffer byte size, which should be aligned to 2^N
 *
 * @return pointer of new-created MemxRingBuffer object on success, otherwise NULL
 */
MemxRingBuffer* memx_ringbuffer_create(uint32_t buffer_size);

/**
 * @brief Destroy the given MemxRingBuffer object and release the memory space
 * allocated. This function always works.
 *
 * @param ring_buffer         ring buffer object
 *
 * @return none
 */
void memx_ringbuffer_destroy(MemxRingBuffer *ring_buffer);

/**
 * @brief Put data into ring buffer. Returns the actual size which is copied
 * into buffer. This method will not block and wait, so User should always check
 * the actual size being written.
 *
 * @param ring_buffer         ring buffer object
 * @param source              pointer to buffer which stores data to be copied
 * @param length              byte length to be copied into ring buffer
 *
 * @return actual byte length copied into buffer
 */
uint32_t memx_ringbuffer_put(MemxRingBuffer *ring_buffer, uint8_t *source, uint32_t length);

/**
 * @brief Get data from ring buffer. Returns the actual size which is copied
 * into given buffer. This method will not block and wait, so User should always
 * check the actual size read.
 *
 * @param ring_buffer         ring buffer object
 * @param target              pointer to buffer which stores data being copied
 * @param length              byte length to be copied from ring buffer
 *
 * @return actual byte length copied from buffer
 */
uint32_t memx_ringbuffer_get(MemxRingBuffer *ring_buffer, uint8_t *target, uint32_t length);

/**
 * @brief Discard data from ring buffer. This method is a special version of
 * 'memx_ringbuffer_get' which directly moves pointer forward without copying
 * any data into buffer.
 *
 * @param ring_buffer         ring buffer object
 * @param length              byte length to be dropped
 *
 * @return actual byte length dropped from buffer
 */
uint32_t memx_ringbuffer_drop(MemxRingBuffer *ring_buffer, uint32_t length);

/**
 * @brief Peak current-used byte length within ring buffer. Calling this
 * function will not modify internal pointers.
 *
 * @param ring_buffer         ring buffer object
 *
 * @return byte length used within buffer
 */
uint32_t memx_ringbuffer_peek_usage(MemxRingBuffer *ring_buffer);

/**
 * @brief Peak current-empty byte length within ring buffer. Calling this
 * function will not modify internal pointers.
 *
 * @param ring_buffer         ring buffer object
 *
 * @return byte length not-used within buffer
 */
uint32_t memx_ringbuffer_peek_space(MemxRingBuffer *ring_buffer);


#ifdef __cplusplus
}
#endif

#endif /* MEMX_RINGBUFFER_H_ */

