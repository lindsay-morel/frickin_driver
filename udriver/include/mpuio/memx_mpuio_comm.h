/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#ifndef MEMX_MPUIO_COMM_H_
#define MEMX_MPUIO_COMM_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include "memx_ioctl.h"
#include "memx_status.h"

#define MEMX_TYPE_NOT_EXIST(M, T) ((M & T) == 0)

#define MEMX_MPUIO_INTERFACE_USB (1 << 0)
#define MEMX_MPUIO_INTERFACE_PCIE (1 << 1)

#define MEMX_MPUIO_DOWNLOAD_MODEL_TYPE_WEIGHT_MEMORY    (1 << 0)
#define MEMX_MPUIO_DOWNLOAD_MODEL_TYPE_MPU_CONFIG       (1 << 1)
#define MEMX_MPUIO_DOWNLOAD_MODEL_TYPE_LEGACY           (1 << 6)
#define MEMX_MPUIO_DOWNLOAD_MODEL_TYPE_BUFFER           (1 << 7)

#define MEMX_MPUIO_MAX_ASSIGN_BUFFER_SIZE_TRY_COUNT (256)
#define MEMX_MPUIO_OFMAP_HEADER_SIZE (64)
#define MEMX_MPUIO_IFMAP_HEADER_SIZE (64)

#define MEMX_MPUIO_FLOW_RING_BUFFER_NUMBER (32)
#define MEMX_MPUIO_FLOW_RING_BUFFER_SIZE (512 * 1024)
#define MEMX_MPUIO_MAX_PENDDING_LIST_SIZE (300)

#define MEMX_MPUIO_MAX_HW_MPU_COUNT (16)

typedef enum MEMX_MPUIO_IFMAP_FORMAT {
    MEMX_MPUIO_IFMAP_FORMAT_GBF80 = 0,
    MEMX_MPUIO_IFMAP_FORMAT_RGB565 = 1,
    MEMX_MPUIO_IFMAP_FORMAT_RGB888 = 2,
    MEMX_MPUIO_IFMAP_FORMAT_YUV422 = 3,
    MEMX_MPUIO_IFMAP_FORMAT_FLOAT16 = 4,
    MEMX_MPUIO_IFMAP_FORMAT_FLOAT32 = 5,
    MEMX_MPUIO_IFMAP_FORMAT_GBF80_ROW_PAD = 6,
    MEMX_MPUIO_IFMAP_FORMAT_RESERVED
} MEMX_MPUIO_IFMAP_FORMAT;

typedef enum MEMX_MPUIO_OFMAP_FORMAT {
    MEMX_MPUIO_OFMAP_FORMAT_GBF80 = 0,
    MEMX_MPUIO_OFMAP_FORMAT_FLOAT16 = 4,
    MEMX_MPUIO_OFMAP_FORMAT_FLOAT32 = 5,
    MEMX_MPUIO_OFMAP_FORMAT_GBF80_ROW_PAD = 6,
    MEMX_MPUIO_OFMAP_FORMAT_RESERVED
} MEMX_MPUIO_OFMAP_FORMAT;

typedef enum MEMX_MPUIO_MPU_GROUP_STATUS {
    MEMX_MPUIO_MPU_GROUP_STATUS_IDLE = 0,
    MEMX_MPUIO_MPU_GROUP_STATUS_BUSY = 1,
    MEMX_MPUIO_MPU_GROUP_STATUS_RESERVED
} MEMX_MPUIO_MPU_GROUP_STATUS;

typedef enum MEMX_MPUIO_MPU_GROUP_CONFIG {
    MEMX_MPUIO_MPU_GROUP_CONFIG_ONE_GROUP_FOUR_MPUS    = 0,
    MEMX_MPUIO_MPU_GROUP_CONFIG_TWO_GROUP_TWO_MPUS     = 1,
    MEMX_MPUIO_MPU_GROUP_CONFIG_ONE_GROUP_ONE_MPU      = 2,
    MEMX_MPUIO_MPU_GROUP_CONFIG_ONE_GROUP_THREE_MPUS   = 3,
    MEMX_MPUIO_MPU_GROUP_CONFIG_ONE_GROUP_TWO_MPUS     = 4,
    MEMX_MPUIO_MPU_GROUP_CONFIG_ONE_GROUP_EIGHT_MPUS   = 5,
    MEMX_MPUIO_MPU_GROUP_CONFIG_ONE_GROUP_TWELVE_MPUS  = 6,
    MEMX_MPUIO_MPU_GROUP_CONFIG_ONE_GROUP_SIXTEEN_MPUS = 7,
    MEMX_MPUIO_MPU_GROUP_CONFIG_MAX
} MEMX_MPUIO_MPU_GROUP_CONFIG;

char *mpuio_comm_find_file_name(const char *name);
uint32_t mpuio_comm_cal_output_flow_size(int32_t h, int32_t w, int32_t z, int32_t ch, int32_t format, uint8_t frm_pad);
uint32_t mpuio_comm_assign_suitable_buffer_size_for_flow(uint32_t h, uint32_t flow_size, uint32_t remaining_try_count);
memx_status mpuio_comm_config_chip_role(uint8_t group_cnt, uint8_t chip_cnt_per_group, struct hw_info *p_hw_info);
memx_status mpuio_comm_config_mpu_group(uint8_t mpu_group_config, struct hw_info *p_hw_info);
int32_t mpuio_comm_buffer_allocation_for_flow(uint32_t *buffer_size, uint32_t *in_flow_size, uint32_t flow_count, uint32_t *ram_size);

#ifdef __cplusplus
}
#endif

#endif /* MEMX_MPUIO_COMM_H_ */
