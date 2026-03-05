/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#ifndef MEMX_MPUIO_H_
#define MEMX_MPUIO_H_

#ifdef __cplusplus
extern "C" {
#endif
#include "memx_common.h"
#include "memx_status.h"
#include "memx_mpuio_comm.h"
#include "memx_ioctl.h"
/***************************************************************************//**
 * prototype
 ******************************************************************************/
typedef void* _memx_mpuio_t;
typedef void* _memx_mpuio_context_t;
typedef void (*_memx_mpuio_destroy_cb)(_memx_mpuio_t mpuio);
typedef memx_status (*_memx_mpuio_set_read_abort_start)(_memx_mpuio_t mpuio);
typedef memx_status (*_memx_mpuio_operation_cb)(_memx_mpuio_t mpuio, uint8_t chip_id, int32_t cmd_id, void* data, uint32_t size, int32_t timeout);
typedef memx_status (*_memx_mpuio_control_write_cb)(_memx_mpuio_t mpuio, uint8_t chip_id, uint32_t address, uint8_t* data, int32_t length, int32_t* transferred, int32_t increment, int32_t timeout);
typedef memx_status (*_memx_mpuio_control_read_cb)(_memx_mpuio_t mpuio, uint8_t chip_id, uint32_t address, uint8_t* data, int32_t length, int32_t* transferred, int32_t increment, int32_t timeout);
typedef memx_status (*_memx_mpuio_stream_write_cb)(_memx_mpuio_t mpuio, uint8_t chip_id, uint8_t flow_id, uint8_t* data, int32_t length, int32_t* transferred, int32_t timeout);
typedef memx_status (*_memx_mpuio_stream_read_cb)(_memx_mpuio_t mpuio, uint8_t chip_id, uint8_t flow_id, uint8_t* data, int32_t length, int32_t* transferred, int32_t timeout);
typedef memx_status (*_memx_mpuio_download_model_cb)(_memx_mpuio_t mpuio, uint8_t chip_id, void * pDfpMeta, uint8_t model_idx, int32_t type, int32_t timeout);
typedef memx_status (*_memx_mpuio_download_firmware_cb)(_memx_mpuio_t mpuio, const char *file_path);
typedef memx_status (*_memx_mpuio_set_ifmap_size_cb)(_memx_mpuio_t mpuio, uint8_t chip_id, uint8_t flow_id, int32_t height, int32_t width, int32_t z, int32_t channel_number, int32_t format, int32_t timeout);
typedef memx_status (*_memx_mpuio_set_ofmap_size_cb)(_memx_mpuio_t mpuio, uint8_t chip_id, uint8_t flow_id, int32_t height, int32_t width, int32_t z, int32_t channel_number, int32_t format, int32_t timeout);
typedef memx_status (*_memx_mpuio_update_fmap_size_cb)(_memx_mpuio_t mpuio, uint8_t chip_id, uint8_t in_flow_count, uint8_t out_flow_count, int32_t timeout);

/***************************************************************************//**
 * interface
 ******************************************************************************/

/**
 * @brief Structure of all IO functions required to access MemryX MPU device.
 */
typedef struct _MemxMpuIo
{
  _memx_mpuio_context_t context; // context which stores IO interface-related information
  _memx_mpuio_destroy_cb destroy; // context clean-up method
  _memx_mpuio_set_read_abort_start set_read_abort_start; // setup read abort flag to make sure dummy read will be triggered
  _memx_mpuio_operation_cb operation; // general purpose operation
  _memx_mpuio_control_write_cb control_write; // basic bus write method
  _memx_mpuio_control_read_cb control_read; // basic bus read method
  _memx_mpuio_stream_write_cb stream_write; // data stream write method
  _memx_mpuio_stream_read_cb stream_read; // data stream read method
  _memx_mpuio_download_model_cb download_model; // download model to device
  _memx_mpuio_download_firmware_cb download_firmware;  // download firmware to device flash
  _memx_mpuio_set_ifmap_size_cb set_ifmap_size; // configure input feature map size
  _memx_mpuio_set_ofmap_size_cb set_ofmap_size; // configure output feature map size
  _memx_mpuio_update_fmap_size_cb update_fmap_size; // program in/out feature map size to driver
  uint8_t model_id; // model ID of currently running model.
  struct hw_info hw_info; // store fw and chip info
  uint8_t mpu_group_status[MEMX_MPUIO_MAX_HW_MPU_COUNT]; // store mpu group status
  uint32_t ifmap_control; // store input feature map control
} MemxMpuIo;

/**
 * @brief Create a new MPUIO context with default variables given. Used within
 * class who inherits this class and implement the interface.
 *
 * @return new created MPUIO context on success, otherwise nullptr
 */
MemxMpuIo *memx_mpuio_create(void);

/**
 * @brief Create new MPUIO context and allocate resources required to access
 * device.
 *
 * @param group_id            target group
 * @param interface_type      bus type
 * @param chip_gen            target MPU chip generation (MEMX_MPU_CHIP_GEN_CASCADE_PLUS)
 *
 * @return MPUIO context new created
 */
MemxMpuIo *memx_mpuio_init(uint8_t group_id, uint8_t interface_type, uint8_t chip_gen);

/**
 * @brief Destroy the given IO handle context and clean-up all resources
 * allocated within original 'create' function.
 *
 * @param mpuio               IO handle context
 *
 * @return none
 */
void memx_mpuio_destroy(MemxMpuIo *mpuio);

/**
 * @brief Set up read abort flag to the given MPUIO context.
 *
 * @param mpuio               MPUIO context
 *
 * @return none
 */
memx_status memx_mpuio_set_read_abort_start(MemxMpuIo *mpuio);

/**
 * @brief Gerneral purpose MPUIO operation. The command ID should be registered
 * to 'memx_command.h' in order to be global unique among all MPUs and MPUIOs.
 *
 * @param mpuio               IO handle context
 * @param chip_id             target chip ID
 * @param cmd_id              command ID
 * @param data                command data
 * @param size                command data size
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpuio_operation(MemxMpuIo *mpuio, uint8_t chip_id, int32_t cmd_id, void *data, uint32_t size, int32_t timeout);

/**
 * @brief Control channel write multiple bytes to specific bus address. The
 * address given can be either a register address or a SRAM address.
 *
 * @param mpuio               IO handle context
 * @param chip_id             target chip ID
 * @param address             bus address
 * @param data                byte buffer to write
 * @param length              byte length to write
 * @param transferred         actual transferred byte length
 * @param increment           post-write-address-increase enable
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpuio_control_write(MemxMpuIo *mpuio, uint8_t chip_id, uint32_t address, uint8_t *data, int32_t length, int32_t *transferred, int32_t increment, int32_t timeout);

/**
 * @brief Control channel read multiple bytes from specific bus address. The
 * address given can be either a register address or a SRAM address.
 *
 * @param mpuio               IO handle context
 * @param chip_id             target chip ID
 * @param address             bus address
 * @param data                byte buffer to store data read
 * @param length              byte length to read
 * @param transferred         actual transferred byte length
 * @param increment           post-read-address-increase enable
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpuio_control_read(MemxMpuIo *mpuio, uint8_t chip_id, uint32_t address, uint8_t *data, int32_t length, int32_t *transferred, int32_t increment, int32_t timeout);

/**
 * @brief Data channel write multiple bytes to specific data flow.
 *
 * @param mpuio               IO handle context
 * @param chip_id             target chip ID
 * @param flow_id             target flow ID
 * @param data                byte buffer to write
 * @param length              byte length to write
 * @param transferred         actual transferred byte length
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpuio_stream_write(MemxMpuIo *mpuio, uint8_t chip_id, uint8_t flow_id, uint8_t *data, int32_t length, int32_t *transferred, int32_t timeout);

/**
 * @brief Data channel read multiple bytes from specific data flow.
 *
 * @param mpuio               IO handle context
 * @param chip_id             target chip ID
 * @param flow_id             target flow ID
 * @param data                byte buffer to store data read
 * @param length              byte length to read
 * @param transferred         actual transferred byte length
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpuio_stream_read(MemxMpuIo *mpuio, uint8_t chip_id, uint8_t flow_id, uint8_t *data, int32_t length, int32_t *transferred, int32_t timeout);

/**
 * @brief Download firmware to device flash. Each MPUIO can have it's own implementation
 * for download path could be different through different interface.
 *
 * @param mpuio               IO handle context
 * @param file_path           firmware file, which by default should be named as 'cascade.bin'
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpuio_download_firmware(MemxMpuIo *mpuio, const char *file_path);
/**
 * @brief Download model data flow program to device. Data flow program should
 * be generated using MIX compiler. Each MPUIO can have it's own implementation
 * for download path could be different through different interface.
 *
 * @param mpuio               IO handle context
 * @param chip_id             target chip ID
 * @param file_path           model dfp file, which by default should be named as '<device_name>.dfp'
 * @param model_idx           which rgcfg to use (used for model swapping). Set to 0 if only 1 model.
 * @param type                dfp type, 0: wtmem, 1: config
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpuio_download_model(MemxMpuIo *mpuio, uint8_t chip_id, void * pDfpMeta, uint8_t model_idx, int32_t type, int32_t timeout);
/**
 * @brief Configure driver model input feature map size for specific data flow.
 *
 * @param mpuio               IO handle context
 * @param chip_id             target chip ID
 * @param flow_id             target flow ID
 * @param height              input feature map height
 * @param width               input feature map width
 * @param z                   input feature map z
 * @param channel_number      input feature map channel number
 * @param format              input feature map format
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpuio_set_ifmap_size(MemxMpuIo *mpuio, uint8_t chip_id, uint8_t flow_id, int32_t height, int32_t width, int32_t z, int32_t channel_number, int32_t format, int32_t timeout);
/**
 * @brief Configure driver model output feature map size for specific data flow.
 *
 * @param mpuio               IO handle context
 * @param chip_id             target chip ID
 * @param flow_id             target flow ID
 * @param height              output feature map height
 * @param width               output feature map width
 * @param z                   output feature map z
 * @param channel_number      output feature map channel number
 * @param format              output feature map format
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpuio_set_ofmap_size(MemxMpuIo *mpuio, uint8_t chip_id, uint8_t flow_id, int32_t height, int32_t width, int32_t z, int32_t channel_number, int32_t format, int32_t timeout);

/**
 * @brief Update driver model input/output feature map last layer size in order to
 * initialize driver internal buffers and variables.
 *
 * @param mpuio               IO handle context
 * @param chip_id             target chip ID
 * @param in_flow_count       total number of input feature map
 * @param out_flow_count      total number of output feature map
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpuio_update_fmap_size(MemxMpuIo *mpuio, uint8_t chip_id, uint8_t in_flow_count, uint8_t out_flow_count, int32_t timeout);

/**
 * @brief Attach to first found idle mpu group on device
 *
 * @param mpuio                   IO handle context
 * @param *attached_mpu_group_id  Attached mpu group id
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpuio_attach_idle_mpu_group(MemxMpuIo* mpuio, uint8_t *attached_mpu_group_id);

/**
 * @brief Detach mpu group on device
 *
 * @param mpuio               IO handle context
 * @param mpu_group_id        target MPU group ID
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpuio_detach_mpu_group(MemxMpuIo* mpuio, uint8_t mpu_group_id);

#ifdef __cplusplus
}
#endif

#endif /* MEMX_MPUIO_H_ */
