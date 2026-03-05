/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2024 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#ifndef MEMX_PLATFORM_H_
#define MEMX_PLATFORM_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
struct _MemxUsb;
struct _MemxPcie;
typedef struct platform_device {
    uint32_t hif;
    union {
        struct _MemxUsb *usb;
        struct _MemxPcie *pcie;
    } pdev;
} platform_device_t;
typedef platform_device_t* platform_device_ptr_t;

// basic platform defination
#ifdef __linux__
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h> // needs to create mutex using shared memory, otherwise mutex won't work within multi-processes case
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>
#include <fcntl.h>
#include <stdio.h>
typedef int32_t             platform_handle_t,      *platform_handle_ptr_t;
typedef platform_handle_t   platform_fd_t,          *platform_fd_ptr_t;
typedef pthread_t           platform_thread_t,      *platform_thread_ptr_t;
typedef pthread_mutex_t     platform_mutex_t,       *platform_mutex_ptr_t;
typedef pthread_mutex_t     platform_share_mutex_t, *platform_share_mutex_ptr_t;
typedef pthread_cond_t      platform_thread_cond_t, *platform_thread_cond_ptr_t;
typedef FILE                platform_file_t,        *platform_file_ptr_t;

typedef int8_t              platform_int8_t;
typedef int16_t             platform_int16_t;
typedef int32_t             platform_int32_t;
typedef int64_t             platform_int64_t;

typedef uint8_t             platform_uint8_t;
typedef uint16_t            platform_uint16_t;
typedef uint32_t            platform_uint32_t;
typedef uint64_t            platform_uint64_t;

typedef size_t              platform_size_t;
typedef ssize_t             platform_ssize_t;

typedef void                platform_void_t;
#elif _WIN32
#include "pch.h"
#include <BaseTsd.h>
#include <windows.h>
#include "devioctl.h"
#include "strsafe.h"
#include <setupapi.h>
#include <basetyps.h>
#include "usbdi.h"
typedef HANDLE              platform_handle_t,      *platform_handle_ptr_t;
typedef platform_handle_t   platform_fd_t,          *platform_fd_ptr_t;
typedef platform_handle_t   platform_thread_t,      *platform_thread_ptr_t;
typedef CRITICAL_SECTION    platform_mutex_t,       *platform_mutex_ptr_t;
typedef platform_handle_t   platform_share_mutex_t, *platform_share_mutex_ptr_t;
typedef CONDITION_VARIABLE  platform_thread_cond_t, *platform_thread_cond_ptr_t;
typedef FILE                platform_file_t,        *platform_file_ptr_t;

typedef INT8                platform_int8_t;
typedef INT16               platform_int16_t;
typedef INT32               platform_int32_t;
typedef INT64               platform_int64_t;

typedef UINT8               platform_uint8_t;
typedef UINT16              platform_uint16_t;
typedef UINT32              platform_uint32_t;
typedef UINT64              platform_uint64_t;

typedef SIZE_T              platform_size_t;
typedef SSIZE_T             platform_ssize_t;

typedef VOID                platform_void_t;
#else
#error "Uon-support platform"
#endif


platform_handle_t platform_open(const void * device_path, platform_uint32_t flag);
platform_int32_t platform_close(platform_fd_ptr_t fd_ptr);
platform_int32_t platform_read(platform_handle_t tx_event, platform_fd_ptr_t fd_ptr, platform_void_t *buf, platform_uint32_t count);
platform_int32_t platform_write(platform_handle_t rx_event, platform_fd_ptr_t fd_ptr, platform_void_t *buf, platform_uint32_t count);
platform_int32_t platform_ioctl(platform_handle_t ctrl_event, platform_fd_ptr_t fd_ptr, platform_uint32_t ctrl_code, platform_void_t *data, platform_uint32_t size);
platform_int32_t platform_thread_create(platform_thread_ptr_t thread_ptr, platform_void_t *attr, platform_void_t *(*start_routine)(platform_void_t *), platform_void_t *arg);
platform_int32_t platform_thread_join(platform_thread_ptr_t thread_ptr, platform_void_t **retval);
platform_int32_t platform_mutex_create(platform_mutex_ptr_t mutex_ptr, platform_void_t *attr);
platform_int32_t platform_mutex_destory(platform_mutex_ptr_t mutex_ptr);
platform_int32_t platform_mutex_lock(platform_mutex_ptr_t mutex_ptr);
platform_int32_t platform_mutex_trylock(platform_mutex_ptr_t mutex_ptr);
platform_int32_t platform_mutex_unlock(platform_mutex_ptr_t mutex_ptr);
platform_int32_t platform_share_mutex_lock(platform_share_mutex_ptr_t mutex_ptr);
platform_int32_t platform_share_mutex_trylock(platform_share_mutex_ptr_t mutex_ptr);
platform_int32_t platform_share_mutex_unlock(platform_share_mutex_ptr_t mutex_ptr);
platform_int32_t platform_cond_init(platform_thread_cond_ptr_t cond_ptr, platform_void_t *attr);
platform_int32_t platform_cond_wait(platform_thread_cond_ptr_t cond_ptr, platform_mutex_ptr_t mutex_ptr);
platform_int32_t platform_cond_signal(platform_thread_cond_ptr_t cond_ptr);
platform_int32_t platform_cond_broadcast(platform_thread_cond_ptr_t cond_ptr);
platform_int32_t platform_usleep(platform_uint32_t usec);
platform_void_t platform_memcpy(platform_void_t *dest, const platform_void_t *src, platform_uint32_t size);

#define pthread_cond_wait_with_lock(_cond_, _mutex_) \
  do { \
    platform_mutex_lock((_mutex_)); \
    platform_cond_wait((_cond_), (_mutex_)); \
    platform_mutex_unlock((_mutex_)); \
  } while(0)

#define pthread_wait_until_condition_flase_with_lock(_condition_, _cond_, _mutex_) \
  do { \
    platform_mutex_lock((_mutex_)); \
    while((_condition_)) \
      platform_cond_wait((_cond_), (_mutex_)); \
    platform_mutex_unlock((_mutex_)); \
  } while(0)

#define pthread_cond_signal_with_lock(_cond_, _mutex_) \
  do { \
    platform_mutex_lock((_mutex_)); \
    platform_cond_signal((_cond_)); \
    platform_mutex_unlock((_mutex_)); \
  } while(0)

#define pthread_broadcast_signal_with_lock(_cond_, _mutex_) \
  do { \
    platform_mutex_lock((_mutex_)); \
    platform_cond_broadcast((_cond_)); \
    platform_mutex_unlock((_mutex_)); \
  } while(0)

#ifdef __cplusplus
}
#endif
#endif /* MEMX_PLATFORM_H_ */