/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2024 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#ifndef MEMX_COMMON_H_
#define MEMX_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * general include
 ******************************************************************************/
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
/***************************************************************************//**
 * API public
 ******************************************************************************/
#if defined(__MSC_VER) || defined(WIN_EXPORTS) || defined(_WIN32) || defined(_WIN64)
  #define MEMX_API_EXPORT __declspec(dllexport)
  #define MEMX_API_IMPORT __declspec(dllimport)
#elif defined(__GNUC__)
  #define MEMX_API_EXPORT __attribute__((visibility("default")))
  #define MEMX_API_IMPORT
#else
  #define MEMX_API_EXPORT
  #define MEMX_API_IMPORT
#endif

/***************************************************************************//**
 * misc.
 ******************************************************************************/
#ifndef null
#define null (NULL)
#endif

#ifndef nullptr
#define nullptr (NULL)
#endif

#ifndef unused
#define unused(x) (void)(x)
#endif

#ifndef DEF_BYTE
#define DEF_BYTE(x) (x)
#endif

#ifndef DEF_KB
#define DEF_KB(x) ((x) * DEF_BYTE(1024))
#endif

#ifndef BIT
#define BIT(x) ((0x00000001UL) << (x))
#endif

#ifndef SET_STATUS_AND_GOTO
#define SET_STATUS_AND_GOTO(_status_var, _error_code, _label) \
    do { (_status_var) = (_error_code); goto _label; } while (0)
#endif

#ifdef __cplusplus
}
#endif
//#define DEBUG_PERFORMANCE
#endif /* MEMX_COMMON_H_ */

