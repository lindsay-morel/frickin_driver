/**
 * @file public.h
 * @author Gary Chang (gary.chang@memryx.com)
 * @brief This module contains the common declarations shared by driver and user applications.
 * @version 0.1
 * @date 2023
 *
 * @copyright Copyright (c) 2023 MemryX. Inc. All Rights Reserved.
 *
 */
#pragma once
//
// Define an Interface Guid so that apps can find the device and talk to it.
//
#include "..\..\include\memx_ioctl.h"

#define DEF_1KB                     (1024UL)
#define DEF_1MB                     (1024UL * DEF_1KB)
#define DEF_BYTE(x)                 (x)
#define DEF_KB(x)                   ((x)  * DEF_1KB)
#define DEF_MB(x)                   ((x)  * DEF_1MB)
#define DEF_ADDRESS_ALIGEMENT(x)    ((x) - 1)

#define BIT(x)                      (1UL << (x))


#define MSIX_ERROR_INDICTOR_VAL		    (512)
#define MEMX_MPUIO_COMMON_HEADER_SIZE   (64)
#define XFLOW_MAX_BURST_WRITE_SIZE      (256 * 256 * 1024)
#define XFLOW_BURST_WRITE_BATCH_SIZE    (64)
#define GET_CHIPID_FROM_MSIX(x)         (((x) - 1) >> 1)

#define DIRECT_IO (1)