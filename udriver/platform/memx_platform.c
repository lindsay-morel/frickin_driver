/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#include <string.h>
#include "memx_platform.h"
#include "memx_mpuio_comm.h"
#include "memx_ioctl.h"

platform_void_t platform_memcpy(platform_void_t *dest, const platform_void_t *src, platform_uint32_t size)
{   // Note: __aarch64__ memcpy will assume destination is 8 bytes-alignment but our chip is 4 bytes-alignment, use cpu to write the last 4 bytes to prevent bus error issue.
#ifdef __aarch64__
    if (size % 8) {
        memcpy(dest, src, size - 4);
        memcpy(((platform_uint8_t *)dest + size - 4), ((platform_uint8_t *)src + size - 4), 4);
    } else {
        memcpy(dest, src, size);
    }
#else
    memcpy(dest, src, size);
#endif
}

#ifdef __linux__
platform_handle_t platform_open(const void * device_path, platform_uint32_t flag)
{
    flag =  (O_RDWR | O_SYNC);
    return open((const char *)device_path, flag);
}

platform_int32_t platform_close(platform_fd_ptr_t fd_ptr)
{
    return close(*fd_ptr);
}

platform_int32_t platform_read(platform_handle_t event, platform_fd_ptr_t fd_ptr, platform_void_t *buf, platform_uint32_t count)
{
    (platform_void_t)event;
    if (!fd_ptr) { printf("%s: fd ptr is NULL \n", __FUNCTION__); return -1; }
    if (!buf) { printf("%s: buf is NULL \n", __FUNCTION__); return -2; }
    return read(*fd_ptr, buf, count);
}

platform_int32_t platform_write(platform_handle_t event, platform_fd_ptr_t fd_ptr, platform_void_t *buf, platform_uint32_t count)
{
   (platform_void_t)event;
    if (!fd_ptr) { printf("%s: fd ptr is NULL \n", __FUNCTION__); return -1; }
    if (!buf) { printf("%s: buf is NULL \n", __FUNCTION__); return -2; }
    return write(*fd_ptr, buf, count);
}

platform_int32_t platform_ioctl(platform_handle_t ctrl_event, platform_fd_ptr_t fd_ptr, platform_uint32_t ctrl_code, platform_void_t *data, platform_uint32_t size)
{
    (platform_void_t)size;
    (platform_void_t)ctrl_event;
    return ioctl(*fd_ptr, ctrl_code, data);
}

platform_int32_t platform_thread_create(platform_thread_ptr_t thread_ptr, platform_void_t *attr, platform_void_t *(*start_routine)(platform_void_t *), platform_void_t *arg)
{
    (platform_void_t)attr;
    return pthread_create(thread_ptr, NULL, start_routine, arg);
}

platform_int32_t platform_thread_join(platform_thread_ptr_t thread_ptr, platform_void_t **retval)
{
    return pthread_join(*thread_ptr, retval);
}

platform_int32_t platform_mutex_create(platform_mutex_ptr_t mutex_ptr, platform_void_t *attr)
{
    (platform_void_t)attr;
    return pthread_mutex_init(mutex_ptr, NULL);
}

platform_int32_t platform_mutex_destory(platform_mutex_ptr_t mutex_ptr)
{
    return pthread_mutex_destroy(mutex_ptr);
}

platform_int32_t platform_mutex_lock(platform_mutex_ptr_t mutex_ptr)
{
    return pthread_mutex_lock(mutex_ptr);
}

platform_int32_t platform_mutex_trylock(platform_mutex_ptr_t mutex_ptr)
{
    return pthread_mutex_trylock(mutex_ptr);
}

platform_int32_t platform_mutex_unlock(platform_mutex_ptr_t mutex_ptr)
{
    return pthread_mutex_unlock(mutex_ptr);
}

platform_int32_t platform_share_mutex_lock(platform_share_mutex_ptr_t mutex_ptr)
{
    return pthread_mutex_lock(mutex_ptr);
}

platform_int32_t platform_share_mutex_trylock(platform_share_mutex_ptr_t mutex_ptr)
{
    return pthread_mutex_trylock(mutex_ptr);
}

platform_int32_t platform_share_mutex_unlock(platform_share_mutex_ptr_t mutex_ptr)
{
    return pthread_mutex_unlock(mutex_ptr);
}

platform_int32_t platform_cond_init(platform_thread_cond_ptr_t cond_ptr, platform_void_t *attr)
{
    (platform_void_t)attr;
    return pthread_cond_init(cond_ptr, NULL);
}

platform_int32_t platform_cond_wait(platform_thread_cond_ptr_t cond_ptr, platform_mutex_ptr_t mutex_ptr)
{
    return pthread_cond_wait(cond_ptr, mutex_ptr);
}

platform_int32_t platform_cond_signal(platform_thread_cond_ptr_t cond_ptr)
{
    return pthread_cond_signal(cond_ptr);
}

platform_int32_t platform_cond_broadcast(platform_thread_cond_ptr_t cond_ptr)
{
    return pthread_cond_broadcast(cond_ptr);
}

platform_int32_t platform_usleep(platform_uint32_t usec)
{
    return usleep(usec);
}

#elif _WIN32
platform_handle_t platform_open(void * device_path, platform_uint32_t flag)
{
    (platform_void_t)flag;
    return CreateFile((LPCWSTR) device_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
}

platform_int32_t platform_close(platform_fd_ptr_t fd_ptr)
{
    return CloseHandle(*fd_ptr);
}

platform_int32_t platform_read(platform_handle_t event, platform_fd_ptr_t fd_ptr, platform_void_t *buf, platform_uint32_t count)
{
    DWORD               lpBytesReturned    = 0;
    platform_int32_t    getLastError       = 0;
    OVERLAPPED          ov                 = {0};

    if (!fd_ptr) { printf("%s: fd ptr is NULL \n", __FUNCTION__); return -1; }
    if (!buf) { printf("%s: buf is NULL \n", __FUNCTION__); return -2; }
    if (!event) { printf("%s: event is NULL \n", __FUNCTION__); return -3; }

    ov.hEvent = event;
    if (!ReadFile(*fd_ptr, buf, count, (LPDWORD)&lpBytesReturned, &ov)) {
        if (GetLastError() != ERROR_IO_PENDING) {
            getLastError = GetLastError();
            printf("%s: file error %u\n", __FUNCTION__ , getLastError);
            return -getLastError;
        } else {
            WaitForSingleObject(ov.hEvent, INFINITE);
            // Wait for the operation to complete before continuing.
            // You could do some background work if you wanted to.
            if (!GetOverlappedResult(*fd_ptr, &ov, (LPDWORD)&lpBytesReturned, TRUE)) {
                getLastError = GetLastError();
                //printf("%s: GetOverlappedResult error %u\n", __FUNCTION__ , getLastError); 995 means ERROR_OPERATION_ABORTED
                return -getLastError;
            }
        }
    }

    return lpBytesReturned;
}

platform_int32_t platform_write(platform_handle_t event, platform_fd_ptr_t fd_ptr, platform_void_t *buf, platform_uint32_t count)
{
    DWORD               lpBytesReturned    = 0;
    platform_int32_t    getLastError       = 0;
    OVERLAPPED          ov                 = {0};

    if (!fd_ptr) { printf("%s: fd ptr is NULL \n", __FUNCTION__); return -1; }
    if (!buf) { printf("%s: buf is NULL \n", __FUNCTION__); return -2; }
    if (!event) { printf("%s: event is NULL \n", __FUNCTION__); return -3; }

    ov.hEvent = event;
    if (!WriteFile(*fd_ptr, buf, count, (LPDWORD)&lpBytesReturned, &ov)) {
        if (GetLastError() != ERROR_IO_PENDING) {
            getLastError = GetLastError();
            printf("%s: file error %u\n", __FUNCTION__ , getLastError);
            return -getLastError;
        } else {
            WaitForSingleObject(ov.hEvent, INFINITE);
            // Wait for the operation to complete before continuing.
            // You could do some background work if you wanted to.
            if (!GetOverlappedResult(*fd_ptr, &ov, (LPDWORD)&lpBytesReturned, TRUE)) {
                getLastError = GetLastError();
                //printf("%s: GetOverlappedResult error %u\n", __FUNCTION__ , getLastError); 995 means ERROR_OPERATION_ABORTED
                return -getLastError;
            }
        }
    }

    return lpBytesReturned;
}

platform_int32_t platform_ioctl(platform_handle_t event, platform_fd_ptr_t fd_ptr, platform_uint32_t ctrl_code, platform_void_t *data, platform_uint32_t size)
{
    DWORD               lpBytesReturned = 0;
    platform_int32_t    getLastError    = 0;
    OVERLAPPED          ov              = {0};
    UCHAR               temp            = 0;

    if (!fd_ptr) { printf("%s: fd ptr is NULL \n", __FUNCTION__); return -1; }
    if (ctrl_code == UNIMPLEMENT_METHOD) { printf("%s: UNIMPLEMENT_METHOD:%d\n", __FUNCTION__, ctrl_code); return -2; }
    if (!event) { printf("%s: event is NULL \n", __FUNCTION__); return -3; }

    if (data == NULL) {
        data = &temp;
        size = sizeof(temp);
    }

    ov.hEvent = event;
    if (!DeviceIoControl(*fd_ptr, ctrl_code, data, size, data, size, (LPDWORD)&lpBytesReturned, &ov)) {
        if (GetLastError() != ERROR_IO_PENDING) {
            getLastError = GetLastError();
            printf("%s: file error %u\n", __FUNCTION__ , getLastError);
            return -getLastError;
        } else {
            WaitForSingleObject(ov.hEvent, 0);
            // Wait for the operation to complete before continuing.
            // You could do some background work if you wanted to.
            if (!GetOverlappedResult(*fd_ptr, &ov, (LPDWORD)&lpBytesReturned, TRUE)) {
                getLastError = GetLastError();
                //printf("%s: GetOverlappedResult error %u\n", __FUNCTION__ , getLastError); 995 means ERROR_OPERATION_ABORTED
                return -getLastError;
            }
        }
    }

    return getLastError;
}

platform_int32_t platform_thread_create(platform_thread_ptr_t thread_ptr, platform_void_t *attr, platform_void_t *(*start_routine)(platform_void_t *), platform_void_t *arg)
{
    (platform_void_t)attr;
    *thread_ptr = CreateThread(
                NULL,       // default security attributes
                0,          // default stack size
                (LPTHREAD_START_ROUTINE)start_routine,
                arg,
                0,          // default creation flags
                NULL);
    if (*thread_ptr == NULL) {
        printf("THREAD_CREATE fail!%u\n", GetLastError());
        return -1;
    }
    return 0;
}

platform_int32_t platform_thread_join(platform_thread_ptr_t thread_ptr, platform_void_t **retval)
{
    (platform_void_t)retval;
    platform_uint32_t retvalue = WaitForSingleObject(*thread_ptr, INFINITE);
    if (retvalue != WAIT_OBJECT_0) { printf("Thread Join fail! %u thread_ptr %p\n", GetLastError(), *thread_ptr); return -1; }
    return 0;
}

platform_int32_t platform_mutex_create(platform_mutex_ptr_t mutex_ptr, platform_void_t *attr)
{
    (platform_void_t)attr;
    if (mutex_ptr == NULL) { printf("MUTEX_CREATE - mutex_ptr is NULL\n");  return -1; }
    InitializeCriticalSection(mutex_ptr);
    return 0;
}

platform_int32_t platform_mutex_destory(platform_mutex_ptr_t mutex_ptr)
{
    if (mutex_ptr == NULL) { printf("MUTEX_DESTORY - mutex_ptr is NULL\n");  return -1; }
    DeleteCriticalSection(mutex_ptr);
    return 0;
}

platform_int32_t platform_mutex_lock(platform_mutex_ptr_t mutex_ptr)
{
    if (mutex_ptr == NULL) { printf("LOCK fail - mutex_ptr is NULL\n"); return -1; }
    EnterCriticalSection(mutex_ptr);
    return 0;
}

platform_int32_t platform_mutex_trylock(platform_mutex_ptr_t mutex_ptr)
{
    if (mutex_ptr == NULL) { printf("TRYLOCK fail - mutex_ptr is NULL\n"); return -1; }
    if (!TryEnterCriticalSection(mutex_ptr)) {
        printf("TRYLOCK fail- Error code:%d\n", GetLastError());
        return -1;
    }
    return 0;
}

platform_int32_t platform_mutex_unlock(platform_mutex_ptr_t mutex_ptr)
{
    if (mutex_ptr == NULL) { printf("UNLOCK fail - mutex_ptr is NULL\n"); return -1; }
    LeaveCriticalSection(mutex_ptr);
    return 0;
}

platform_int32_t platform_share_mutex_lock(platform_share_mutex_ptr_t mutex_ptr)
{
    if (mutex_ptr == NULL) { printf("LOCK fail - mutex_ptr is NULL\n"); return -1; }
    platform_uint32_t retvalue = WaitForSingleObject(*mutex_ptr, INFINITE);
    if (retvalue != WAIT_OBJECT_0) {
        printf("LOCK fail- Error code:%d\n", retvalue);
        return -EINVAL;
    }
    return 0;
}

platform_int32_t platform_share_mutex_trylock(platform_share_mutex_ptr_t mutex_ptr)
{
    if (mutex_ptr == NULL) { printf("TRYLOCK fail - mutex_ptr is NULL\n"); return -1; }
    platform_uint32_t retvalue = WaitForSingleObject(*mutex_ptr, 0);
    if (retvalue != WAIT_OBJECT_0) {
        if (retvalue != WAIT_TIMEOUT) { printf("TRYLOCK fail- Error code:%d\n", retvalue); }
        return (retvalue == WAIT_TIMEOUT) ?	-EBUSY : -EINVAL;
    }
    return 0;
}

platform_int32_t platform_share_mutex_unlock(platform_share_mutex_ptr_t mutex_ptr)
{
    if (mutex_ptr == NULL) { printf("UNLOCK fail - mutex_ptr is NULL\n"); return -1; }
    if (!ReleaseSemaphore(*mutex_ptr, 1, NULL)) {
        printf("Unlock share mutex_ptr fail- Error code: %d\n", GetLastError());
        return -EINVAL;
    }
    return 0;
}

platform_int32_t platform_cond_init(platform_thread_cond_ptr_t cond_ptr, platform_void_t *attr)
{
    (platform_void_t)attr;
    if (cond_ptr == NULL) { printf("COND_INIT fail - cond_ptr is NULL\n"); return -1; }
    InitializeConditionVariable(cond_ptr);
    return 0;
}

platform_int32_t platform_cond_wait(platform_thread_cond_ptr_t cond_ptr, platform_mutex_ptr_t mutex_ptr)
{
    if (cond_ptr == NULL || mutex_ptr == NULL) { printf("COND_WAIT fail - cond_ptr or mutex_ptr is NULL\n"); return -1; }
    if (!SleepConditionVariableCS(cond_ptr, mutex_ptr, INFINITE)) { return -2; }
    return 0;
}

platform_int32_t platform_cond_signal(platform_thread_cond_ptr_t cond_ptr)
{
    if (cond_ptr == NULL) { printf("COND_SIGNAL fail - cond_ptr is NULL\n"); return -1; }
    WakeConditionVariable(cond_ptr);
    return 0;
}

platform_int32_t platform_cond_broadcast(platform_thread_cond_ptr_t cond_ptr)
{
    if (cond_ptr == NULL) { printf("COND_BROADCAST fail - cond_ptr is NULL\n"); return -1; }
    WakeAllConditionVariable(cond_ptr);
    return 0;
}

platform_int32_t platform_usleep(platform_uint32_t usec)
{
    platform_handle_t timer = INVALID_HANDLE_VALUE;
    LARGE_INTEGER ft = {0};
    ft.QuadPart = -(platform_int64_t)usec;
    timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!timer) { printf("CreateWaitableTimer fail\n"); return -1; }

    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
    return 0;
}
#else
    #error "Uon-support platform"
#endif