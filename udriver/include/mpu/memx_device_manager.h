/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/

#ifndef MEMX_DEVICE_MANAGER_H_
#define MEMX_DEVICE_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif
#include "memx_global_config.h"
#include "memx_common.h"
#include "memx_mpu_comm.h"
#include "memx_mpuio.h"
#include "memx_platform.h"
#include "memx.h"
#include "memx_log.h"

/**
 * @brief This value should align with MEMX_DEVICE_GROUP_MAX_NUMBER
 */
#define MEMX_DEVICE_MAX_NUMBER (128)
#define MEMX_MAX_USB_MINORS    (256)

#define MEMX_DEVICE_FEATURE_ALL_CHIP (0xFF)
#define MEMX_INVALID_DATA            (0xFFFFFFFF)

#define MEMX_FW_IMGFMT_OFFSET   (0x6F08)
#define MEMX_FW_IMGSIZE_OFFSET  (0x7000)
#define MEMX_FW_IMG_OFFSET      (0x7004)
#define MEMX_FW_IMGSIZE_LEN     (4)
#define MEMX_FW_IMGCRC_LEN      (4)
#define MEMX_IMG_TOTAL_SIZE_LEN (4)
#define MEMX_FSBL_SECTION_SIZE  (DEF_KB(28))
#define MAX_FW_SIZE             (DEF_KB(128))

#define POWER_CORRECTION(P) ((P) * 105 / 100) // 5% correction for MXM2

#define MEMX_VENDOR_ID_FLASH_SIZE_MAX 32

#define MEMX_IS_GET_FEATURE_OPERATION_OPCODE(OP) (((OP) == OPCODE_GET_IFMAP_CONTROL) || ((OP) == OPCODE_GET_QSPI_RESET_RELEASE))
#define MEMX_IS_SET_FEATURE_OPERATION_OPCODE(OP) (((OP) == OPCODE_SET_IFMAP_CONTROL) || ((OP) == OPCODE_SET_QSPI_RESET_RELEASE))

typedef enum {
    MEMX_DEVICE_TYPE_CASCADE_PLUS = 0x0,
    MEMX_DEVICE_TYPE_CASCADE_PLUS_SPLIT = 0x1,
    MEMX_DEVICE_TYPE_FEATURE = 0x2,
    MEMX_DEVICE_TYPE_COUNT
} memx_device_type;

/**
 * @brief Internal MPU device group information structure. Each MPU group device
 * can be controled by only one IO driver (MPUIO) while IO driver itself can be
 * shared among multiple models (MPU). Here we use reference count to manage
 * driver instances. Only the first user will create corresponding driver
 * instance while other users will simply increase the reference count. After
 * last user who destroys the driver instance which results in reference
 * count down to zero will the instance finally be destroyed.
 */
typedef struct _MemxDeviceGroup {
    MemxMpuIo *mpuio; // driver instance for specific MPU device group
    unsigned int ref_count; // records how many users are currently using the driver instance
} MemxDeviceGroup, *pMemxDeviceGroup;

typedef struct _MemxDeviceMeta {
    uint8_t         chip_gen;       // chip gen of device
    uint8_t         hif_type;       // interface type of device
    uint8_t         role;           // device role (single or split)
    uint16_t        fw_rollback_cnt;
    uint32_t        fw_version;
    uint8_t         mpu_device_driver_version[16];
    uint8_t*        device_path[MEMX_DEVICE_TYPE_COUNT]; // 0/1 : cascade_plus  2 : feature
} MemxDeviceMeta, * pMemxDeviceMeta;

void                memx_device_group_init_all();
void                memx_device_group_del_all();
memx_status         memx_device_group_lock(uint8_t group_id);
memx_status         memx_device_group_trylock(uint8_t group_id);
memx_status         memx_device_group_unlock(uint8_t group_id);
pMemxDeviceGroup    memx_device_group_get_device(uint8_t group_id);
pMemxDeviceMeta     memx_device_group_get_devicemeta(uint8_t group_id);
memx_status         memx_device_group_create(uint8_t group_id);
memx_status         memx_device_group_destory(uint8_t group_id);
memx_status         memx_get_device_count(void *pData);
memx_status         memx_get_mpu_group_count(int8_t group_id, void *pData);
memx_status         memx_device_config_mpu_group(uint8_t group_id, uint8_t mpu_group_config);
memx_status         memx_device_get_total_chip_count(uint8_t group_id, uint8_t* chip_count);
memx_status         memx_device_submit_get_feature(uint8_t group_id, uint8_t feature_id, void* data);
memx_status         memx_device_submit_set_feature(uint8_t group_id, uint8_t feature_id, void* data);
memx_status         memx_device_antirollback_check(uint8_t group_id);
memx_status         memx_device_get_feature(uint8_t group_id, uint8_t chip_id, memx_get_feature_opcode opcode, void* buffer);
memx_status         memx_device_get_feature_opcode_to_feature_id(memx_get_feature_opcode opcode, uint8_t* feature_id);
void                memx_device_fillup_feature_data(struct transport_cmd cmd, memx_get_feature_opcode opcode, uint64_t* buffer, uint8_t gropu_id);
memx_status         memx_device_set_feature(uint8_t group_id, uint8_t chip_id, memx_set_feature_opcode opcode, uint16_t parameter);
memx_status         memx_device_set_feature_opcode_to_feature_id(memx_set_feature_opcode opcode, uint8_t* feature_id);
memx_status         memx_device_feature_error_code_transform(uint8_t chip_id, uint8_t feature_id, struct transport_cmd cmd);
memx_status         memx_device_set_thermal_threshold(uint8_t group_id, uint8_t thermal_threshold);
memx_status         memx_device_set_chip_frequency(uint8_t group_id, uint16_t frequency);
memx_status         memx_device_set_chip_voltage(uint8_t group_id, uint16_t voltage);
memx_status         memx_device_download_fw(uint8_t group_id, const char* fw_data, uint8_t type);
memx_status         memx_device_flash_access(uint8_t group_id, uint8_t chip_id, memx_flash_access_opcode flash_access_opcode, struct transport_cmd* cmd);
memx_status         memx_device_admincmd(uint8_t group_id, uint8_t chip_id, uint32_t opCode, uint32_t subOpCode, void* data);
#ifdef __cplusplus
}
#endif

#endif /* MEMX_DEVICE_MANAGER_H_ */
