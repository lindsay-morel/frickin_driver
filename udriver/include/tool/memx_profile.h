/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#ifndef MEMX_PROFILE_H_
#define MEMX_PROFILE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Time profile output file. If driver time profile is enabled, all
 * function entering and leaving time stamps will be written to this file.
 */
#define MEMX_PROFILE_FILE ("memx.profile")

/**
 * @brief Uses compile option -D_MEMX_PROFILE_ENABLE to enable driver timing
 * profile. After timing profile is enabled, functions marked will log times
 * entering and leaving to specified output file for off-line analysis.
 */
#ifdef _MEMX_PROFILE_ENABLE

#include <sys/time.h>
#include <stdio.h>
#include <pthread.h>

/**
 * @brief Lock to prevent multi-threads write message to the same log file in
 * the same time.
 */
static pthread_mutex_t _memx_profile_guard = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief Puts this macro at the beginning of function to record entering time.
 * This macro will log function name and system time stamp to file.
 */
#define MEMX_MARK_START \
  do { \
    pthread_mutex_lock(&_memx_profile_guard); \
    { \
      FILE *_fp = fopen(MEMX_PROFILE_FILE, "a"); \
      if(_fp) { \
        struct timeval now; \
        gettimeofday(&now, NULL); \
        fprintf(_fp, "[%ld:%ld][<] %s\n", now.tv_sec, now.tv_usec, __func__); \
        fclose(_fp); \
      } \
    } \
    pthread_mutex_unlock(&_memx_profile_guard); \
  } while(0);

/**
 * @brief Puts this macro at the end of function to record leaving time. This
 * macro will log function name and system time stamp to file.
 */
#define MEMX_MARK_STOP \
  do { \
    pthread_mutex_lock(&_memx_profile_guard); \
    { \
      FILE *_fp = fopen(MEMX_PROFILE_FILE, "a"); \
      if(_fp) { \
        struct timeval now; \
        gettimeofday(&now, NULL); \
        fprintf(_fp, "[%ld:%ld][>] %s\n", now.tv_sec, now.tv_usec, __func__); \
        fclose(_fp); \
      } \
    } \
    pthread_mutex_unlock(&_memx_profile_guard); \
  } while(0);

/**
 * @brief Puts this macro at the end of function to record leaving time. This
 * macro will log function name and system time stamp to file. The only
 * difference between 'stop' and 'return' is this macro returns value.
 */
#define MEMX_MARK_RETURN(_status_) \
  do { \
    int _status = (_status_); \
    pthread_mutex_lock(&_memx_profile_guard); \
    { \
      FILE *_fp = fopen(MEMX_PROFILE_FILE, "a"); \
      if(_fp) { \
        struct timeval now; \
        gettimeofday(&now, NULL); \
        fprintf(_fp, "[%ld:%ld][>] %s = %d\n", now.tv_sec, now.tv_usec, __func__, _status); \
        fclose(_fp); \
      } \
    } \
    pthread_mutex_unlock(&_memx_profile_guard); \
    return _status; \
  } while(0)

/**
 * @brief Puts this macro in the middle of function to record conditional
 * leaving, which means only return given status code on condition assertion.
 */
#define MEMX_MARK_EQ_RETURN(_condition_, _status_) \
  do { \
    if (_condition_) { \
      MEMX_MARK_RETURN(_status_); \
    } \
  } while(0)

#else /* !_MEMX_TOOL_PROFILE_TIME */

/**
 * @brief Do nothing in normal case.
 */
#define MEMX_MARK_START
#define MEMX_MARK_STOP
#define MEMX_MARK_RETURN(_status_) \
  do { \
    return (_status_); \
  } while(0)
#define MEMX_MARK_EQ_RETURN(_condition_, _status_) \
  do { \
    if (_condition) { \
      return (_status_); \
    } \
  } while(0)

#endif /* _MEMX_TOOL_PROFILE_TIME */


#ifdef __cplusplus
}
#endif

#endif /* MEMX_PROFILE_H_ */

