/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2023 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#include "memx_log.h"
#include <stdlib.h>

// default disable all log
uint32_t g_memx_log_level = MEMX_LOG_DISABLE;
/***************************************************************************//**
 * implementation
 ******************************************************************************/

void memx_log_init(void) {
    const char* const valstr = getenv(MEMX_LOG_LEVEL);
    if (valstr) { g_memx_log_level = (uint32_t)strtol(valstr, NULL, 0); }
}