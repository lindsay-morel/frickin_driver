/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2023 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#ifndef MEMX_LOG_H_
#define MEMX_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * general include
 ******************************************************************************/
#include "memx_common.h"
#include <inttypes.h>
/**
 * @brief Log level enviroment variable name, export MEMX_LOG_LEVEL=0xff to enable all debug log
 *
 */
#define MEMX_LOG_LEVEL  "MEMX_LOG_LEVEL"
#define MEMX_LOG_DUMP_DATA(data) printf("%s: %d\n", #data, (uint32_t)data)

/**
 * @brief Driver internal log level, support enable different log level simutaneously
 */
typedef enum MEMX_LOG_FLAG {
    MEMX_LOG_DISABLE = 0x0,
    MEMX_LOG_MPU_IFMAP_FLOW = 0x1,
    MEMX_LOG_MPU_OFMAP_FLOW = 0x2,
    MEMX_LOG_MPUIO_IFMAP_FLOW = 0x4,
    MEMX_LOG_MPUIO_OFMAP_FLOW = 0x8,
    MEMX_LOG_MPUIO_IFMAP_DUMP = 0x10,
    MEMX_LOG_MPUIO_OFMAP_DUMP = 0x20,
    MEMX_LOG_DOWNLOAD_DFP = 0x40,
    MEMX_LOG_GENERAL = 0x80
} MEMX_LOG_FLAG;

extern uint32_t g_memx_log_level;

/**
 * @brief Initialize memx log, read log level from enviroment varialbe MEMX_LOG_LEVEL
 *
 * @param none
 *
 * @return none
 */
void memx_log_init(void);

/***************************************************************************//**
 * macro
 ******************************************************************************/
#define MEMX_LOG(log_level, format, ...) do { \
    if (g_memx_log_level && (log_level & g_memx_log_level)){ \
        printf("%s:[%d] --- " format, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    }\
} while(0)

#ifdef __cplusplus
}
#endif

#endif /* MEMX_LOG_H_ */

