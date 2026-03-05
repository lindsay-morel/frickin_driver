/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#ifndef MEMX_MPU_H_
#define MEMX_MPU_H_

#ifdef __cplusplus
extern "C" {
#endif
#include "memx_global_config.h"
#include "memx_mpuio.h"
#include "memx_list.h"
#include "memx.h"

/***************************************************************************//**
 * prototype
 ******************************************************************************/
typedef void* _memx_mpu_t;
typedef void* _memx_mpu_context_t;
typedef void (*_memx_mpu_destroy_cb)(_memx_mpu_t mpu);
typedef memx_status (*_memx_mpu_set_read_abort_cb)(_memx_mpu_t mpu);
typedef memx_status (*_memx_mpu_reconfigure_cb)(_memx_mpu_t mpu, int32_t timeout);
typedef memx_status (*_memx_mpu_operation_cb)(_memx_mpu_t mpu, uint32_t cmd_id, void* data, uint32_t size, int32_t timeout);
typedef memx_status (*_memx_mpu_control_write_cb)(_memx_mpu_t mpu, uint32_t address, uint8_t* data, int32_t length, int32_t* transferred, int32_t increment, int32_t timeout);
typedef memx_status (*_memx_mpu_control_read_cb)(_memx_mpu_t mpu, uint32_t address, uint8_t* data, int32_t length, int32_t* transferred, int32_t increment, int32_t timeout);
typedef memx_status (*_memx_mpu_stream_write_cb)(_memx_mpu_t mpu, uint8_t flow_id, uint8_t* data, int32_t length, int32_t* transferred, int32_t timeout);
typedef memx_status (*_memx_mpu_stream_read_cb)(_memx_mpu_t mpu, uint8_t flow_id, uint8_t* data, int32_t length, int32_t* transferred, int32_t timeout);
typedef memx_status (*_memx_mpu_stream_ifmap_cb)(_memx_mpu_t mpu, uint8_t flow_id, void *to_user_ofmap_data_buffer, int32_t delay_time_in_ms);
typedef memx_status (*_memx_mpu_stream_ofmap_cb)(_memx_mpu_t mpu, uint8_t flow_id, void *from_user_ifmap_data_buffer, int32_t delay_time_in_ms);
typedef memx_status (*_memx_mpu_download_model_cb)(_memx_mpu_t mpu, void * pDfpMeta, uint8_t model_idx, int32_t type, int32_t timeout);
typedef memx_status (*_memx_mpu_download_firmware_cb)(_memx_mpu_t mpu, const char *file_path);
typedef memx_status (*_memx_mpu_set_stream_enable_cb)(_memx_mpu_t mpu, int32_t wait, int32_t timeout);
typedef memx_status (*_memx_mpu_set_stream_disable_cb)(_memx_mpu_t mpu, int32_t wait, int32_t timeout);
typedef memx_status (*_memx_mpu_set_ifmap_queue_size_cb)(_memx_mpu_t mpu, int32_t size, int32_t timeout);
typedef memx_status (*_memx_mpu_set_ifmap_size_cb)(_memx_mpu_t mpu, uint8_t flow_id, int32_t height, int32_t width, int32_t z, int32_t channel_number, int32_t format, int32_t timeout);
typedef memx_status (*_memx_mpu_set_ifmap_range_convert_cb)(_memx_mpu_t mpu, uint8_t flow_id, float shift, float scale, int32_t timeout);
typedef memx_status (*_memx_mpu_set_ofmap_queue_size_cb)(_memx_mpu_t mpu, int32_t size, int32_t timeout);
typedef memx_status (*_memx_mpu_set_ofmap_size_cb)(_memx_mpu_t mpu, uint8_t flow_id, int32_t height, int32_t width, int32_t z, int32_t channel_number, int32_t format, int32_t timeout);
typedef memx_status (*_memx_mpu_set_ofmap_hpoc_cb)(_memx_mpu_t mpu, uint8_t flow_id, int32_t hpoc_size, int32_t* hpoc_indexes, int32_t timeout);
typedef memx_status (*_memx_mpu_update_fmap_size_cb)(_memx_mpu_t mpu, uint8_t in_flow_count, uint8_t out_flow_count, int32_t timeout);
typedef memx_status (*_memx_mpu_enqueue_fmap_buf_cb)(_memx_mpu_t mpumpu, uint8_t flow_id, memx_fmap_buf_t* fmap_buf, int32_t timeout);
typedef memx_status (*_memx_mpu_dequeue_fmap_buf_cb)(_memx_mpu_t mpumpu, uint8_t flow_id, memx_fmap_buf_t* fmap_buf, int32_t timeout);

/***************************************************************************//**
 * interface
 ******************************************************************************/


/**
 * @brief MPU device structure which contains callbacks and common information
 * which should be independent from different hardware device. In general case,
 * this structure can also be viewed as 'model' structure which means if there
 * are multiple models running on the same device, there should be multiple of
 * this structure to handle model data separately.
 */
typedef struct _MemxMpu
{
  _memx_mpu_context_t context; // context which stores device related information
  _memx_mpu_destroy_cb destroy; // context clean-up method
  _memx_mpu_set_read_abort_cb set_read_abort; // setup read abort flag to make sure dummy read will be triggered
  _memx_mpu_reconfigure_cb reconfigure; // runtime reconfiguration
  _memx_mpu_operation_cb operation; // general purpose operation
  _memx_mpu_control_write_cb control_write; // basic bus write method
  _memx_mpu_control_read_cb control_read; // basic bus read method
  _memx_mpu_stream_write_cb stream_write; // data stream write method
  _memx_mpu_stream_read_cb stream_read; // data stream read method
  _memx_mpu_stream_ifmap_cb stream_ifmap; // data stream write input feature map
  _memx_mpu_stream_ofmap_cb stream_ofmap; // data stream read output feature map
  _memx_mpu_download_model_cb download_model; // download model to device
  _memx_mpu_download_firmware_cb download_firmware;  // download firmware to device
  _memx_mpu_set_stream_enable_cb set_stream_enable; // allow sending input feature map to and receiving output feature map from driver
  _memx_mpu_set_stream_disable_cb set_stream_disable; // block sending input feature map to and receiving output feature map from driver
  _memx_mpu_set_ifmap_queue_size_cb set_ifmap_queue_size; // configure input feature map internal queue size
  _memx_mpu_set_ifmap_size_cb set_ifmap_size; // configure input feature map size to driver
  _memx_mpu_set_ifmap_range_convert_cb set_ifmap_range_convert; // configure input feature floating-point to RGB888 range conversion
  _memx_mpu_set_ofmap_queue_size_cb set_ofmap_queue_size; // configure output feature map internal queue size
  _memx_mpu_set_ofmap_size_cb set_ofmap_size; // configure output feature map size to driver
  _memx_mpu_set_ofmap_hpoc_cb set_ofmap_hpoc; // configure output feature map high-precision-output-channel setting
  _memx_mpu_update_fmap_size_cb update_fmap_size; // program in/out feature map size to driver
  _memx_mpu_enqueue_fmap_buf_cb enqueue_ifmap_buf;
  _memx_mpu_enqueue_fmap_buf_cb enqueue_ofmap_buf;
  _memx_mpu_dequeue_fmap_buf_cb dequeue_ifmap_buf;
  _memx_mpu_dequeue_fmap_buf_cb dequeue_ofmap_buf;

  uint8_t chip_gen; // chip gen of device (MEMX_MPU_CHIP_GEN_CASCADE_PLUS)
  uint8_t model_id; // self's model ID
  uint8_t max_input_flow_cnt; // max input flow cnt for this model
  uint8_t max_output_flow_cnt; // max output flow cnt for this model
  MemxMpuIo* mpuio; // IO handle to access device
  MemxList* ifmaps; // input feature map information
  MemxList* ofmaps; // output feature map information
  uint32_t input_mode_flag; // record this DFP ("model")'s input mode flag
  uint8_t mpu_group_id; // hardware mpu group ID of device
  uint8_t input_chip_id; // input mpu chip id for write data/cmd/DFP
  uint8_t output_chip_id; // output mpu chip id for read data
} MemxMpu, *pMemxMpu;

/**
 * @brief Create a new MPU context based on parameter. Used within
 * class who inherits this class and implement the interface.
 *
 * @param mpuio               IO handle to access device
 * @param chip_gen            chip generation
 *
 * @return new created MPU context on success, otherwise nullptr
 */
MemxMpu* memx_mpu_create(MemxMpuIo* mpuio, uint8_t chip_gen);

/**
 * @brief Destroy the given MPU context and clean-up all resources allocated
 * within original 'create' function.
 *
 * @param mpu                 MPU context
 *
 * @return none
 */
void memx_mpu_destroy(MemxMpu* mpu);

/**
 * @brief Runtime re-configuration. Currently binded IO handle re-configuration
 * is required to achieve runtime multiple device group selection.
 *
 * @param mpu                 MPU context
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_reconfigure(MemxMpu* mpu, int32_t timeout);

/**
 * @brief Gerneral purpose operation.
 *
 * @param mpu                 MPU context
 * @param cmd_id              command ID
 * @param data                command data
 * @param size                command data size
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_operation(MemxMpu* mpu, uint32_t cmd_id, void* data, uint32_t size, int32_t timeout);

/**
 * @brief Control channel write multiple bytes to specific bus address. The
 * address given can be either a register address or a SRAM address.
 *
 * @param mpu                 MPU context
 * @param address             bus address
 * @param data                byte buffer to write
 * @param length              byte length to write
 * @param transferred         actual transferred byte length
 * @param increment           post-write-address-increase enable
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_control_write(MemxMpu* mpu, uint32_t address, uint8_t* data, int32_t length, int32_t* transferred, int32_t increment, int32_t timeout);

/**
 * @brief Control channel read multiple bytes from specific bus address. The
 * address given can be either a register address or a SRAM address.
 *
 * @param mpu                 MPU context
 * @param address             bus address
 * @param data                byte buffer to store data read
 * @param length              byte length to read
 * @param transferred         actual transferred byte length
 * @param increment           post-read-address-increase enable
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_control_read(MemxMpu* mpu, uint32_t address, uint8_t* data, int32_t length, int32_t* transferred, int32_t increment, int32_t timeout);

/**
 * @brief Data channel write multiple bytes to specific data flow.
 *
 * @param mpu                 MPU context
 * @param flow_id             target flow ID
 * @param data                byte buffer to write
 * @param length              byte length to write
 * @param transferred         actual transferred byte length
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_stream_write(MemxMpu* mpu, uint8_t flow_id, uint8_t* data, int32_t length, int32_t* transferred, int32_t timeout);

/**
 * @brief Data channel read multiple bytes from specific data flow.
 *
 * @param mpu                 MPU context
 * @param flow_id             target flow ID
 * @param data                byte buffer to store data read
 * @param length              byte length to read
 * @param transferred         actual transferred byte length
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_stream_read(MemxMpu* mpu, uint8_t flow_id, uint8_t* data, int32_t length, int32_t* transferred, int32_t timeout);

/**
 * @brief Set input feature map to MPU. The input feature map (frame) size
 * should be pre-configured through 'memx_mpu_set_ifmap_size' before calling
 * this function.
 *
 * @param mpu							MPU context
 * @param flow_id						target flow ID
 * @param from_user_ifmap_data_buffer   input feature map (frame)
 * @param timeout						milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_stream_ifmap(MemxMpu* mpu, uint8_t flow_id, void *from_user_ifmap_data_buffer, int32_t delay_time_in_ms);

/**
 * @brief Get output feature map from MPU. The output feature map (frame) size
 * should be pre-configured through 'memx_mpu_set_ofmap_size' before calling
 * this function.
 *
 * @param mpu							MPU context
 * @param flow_id						target flow ID
 * @param to_user_ofmap_data_buffer     input feature map (frame)
 * @param timeout						milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_stream_ofmap(MemxMpu* mpu, uint8_t flow_id, void *to_user_ofmap_data_buffer, int32_t delay_time_in_ms);


/**
 * @brief Download firmware to device flash.
 *
 * @param mpu                 MPU context
 * @param file_path           firmware file, which by default should be named as 'cascade.bin'

 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_download_firmware(MemxMpu* mpu, const char* file_path);

/**
 * @brief Download model data flow program to device. Data flow program should
 * be generated using MIX compiler. Both weight memory
 * and model download can be performed in order while update feature map shape
 * only be performed at last if model download is required.
 *
 * @param mpu                 MPU context
 * @param file_path           model dfp file, which by default should be named as '<device_name>.dfp'
 * @param model_idx           which rgcfg to use (used in model swapping). Set to 0 if only 1 model.
 * @param type                0: ignored, 1: weight memory only, 2: model only, 3: both weight memory and model
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_download_model(MemxMpu* mpu, void * pDfpMeta, uint8_t model_idx, int32_t type, int32_t timeout);

/**
 * @brief Enable all input and output data flows of this MPU context to
 * interface driver.
 *
 * @param mpu                 MPU context
 * @param wait                wait until state changed
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_set_stream_enable(MemxMpu* mpu, int32_t wait, int32_t timeout);

/**
 * @brief Disable all input and output data flows of this MPU context to
 * interface driver.
 *
 * @param mpu                 MPU context
 * @param wait                wait until state changed
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_set_stream_disable(MemxMpu* mpu, int32_t wait, int32_t timeout);

/**
 * @brief Set internal input feature map queue size. All iput data flows of this
 * MPU context share the same setting value.
 *
 * @param mpu                 MPU context
 * @param size                input feature map queue size
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_set_ifmap_queue_size(MemxMpu* mpu, int32_t size, int32_t timeout);

/**
 * @brief Configure driver model input feature map 1st layer size in order to
 * initialize driver internal buffers and variables.
 *
 * @param mpu                 MPU context
 * @param flow_id             target flow ID
 * @param height              input feature map height
 * @param width               input feature map width
 * @param z                   input feature map z
 * @param channel_number      input feature map channel number
 * @param format              input feature map format @ref MEMX_MPU_IFMAP_FORMAT
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_set_ifmap_size(MemxMpu* mpu, uint8_t flow_id, int32_t height, int32_t width, int32_t z, int32_t channel_number, int32_t format, int32_t timeout);

/**
 * @brief Configure driver to do input feature map range conversion. This option
 * can only be enabled when input feature map's format is given RGB888(float32)
 * and to be transferred as RGB888(uint8). The value conversion formula should
 * be performed as: '(uint8)y = ( (float)x + <shift> ) * <scale>'.
 *
 * @param mpu                 MPU context
 * @param flow_id             target flow ID
 * @param enable              input feature map data range conversion to be enabled
 * @param shift               input feature map shift constant, which will be added to original value
 * @param scale               input feature map scale factor, which will multiply value after shifted
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_set_ifmap_range_convert(MemxMpu* mpu, uint8_t flow_id, int32_t enable, float shift, float scale, int32_t timeout);

/**
 * @brief Set internal output feature map queue size. All output data flows of
 * this MPU context share the same setting value.
 *
 * @param mpu                 MPU context
 * @param size                output feature map queue size
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_set_ofmap_queue_size(MemxMpu* mpu, int32_t size, int32_t timeout);

/**
 * @brief Configure driver model output feature map last layer size in order to
 * initialize driver internal buffers and variables.
 *
 * @param mpu                 MPU context
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
memx_status memx_mpu_set_ofmap_size(MemxMpu* mpu, uint8_t flow_id, int32_t height, int32_t width, int32_t z, int32_t channel_number, int32_t format, int32_t timeout);

/**
 * @brief Configure model output feature map HPOC(High-Precision-Output-Channel)
 * setting. This setting must be used with MIX compiled model while each HPOC
 * occupies single GBF80 entry with dummy channels inserted.
 *
 * @param mpu                 MPU context
 * @param flow_id             target flow ID
 * @param hpoc_size           high-precision-output-channel number
 * @param hpoc_indexes        high-precision-output-channel index array
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_set_ofmap_hpoc(MemxMpu* mpu, uint8_t flow_id, int32_t hpoc_size, int32_t* hpoc_indexes, int32_t timeout);

/**
 * @brief Update driver model input/output feature map last layer size in order
 * to initialize driver internal buffers and variables.
 *
 * @param mpu                 MPU context
 * @param in_flow_count       total number of input feature map
 * @param out_flow_count      total number of output feature map
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_update_fmap_size(MemxMpu* mpu, uint8_t in_flow_count, uint8_t out_flow_count, int32_t timeout);

/**
 * @brief Read back input feature map shape configuration of specific flow.
 * Returns all zeros if given flow is not configured.
 *
 * @param mpu                 MPU context
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
memx_status memx_mpu_get_ifmap_size(MemxMpu* mpu, uint8_t flow_id, int32_t* height, int32_t* width, int32_t* z, int32_t* channel_number, int32_t* format, int32_t timeout);

/**
 * @brief Read back input feature map floating-point to RGB888 range conversion
 * configuration. Returns all zeros if given flow is not configured.
 *
 * @param mpu                 MPU context
 * @param flow_id             target flow ID
 * @param enable              input feature map data floating-point to RGB using ranges conversion
 * @param shift               amount to shift before scale
 * @param scale               amount ot scale before integer cast
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_get_ifmap_range_convert(MemxMpu* mpu, uint8_t flow_id, int32_t* enable, float* shift, float* scale, int32_t timeout);

/**
 * @brief Read back output feature map shape configuration of specific flow.
 * Returns all zeros if given flow is not configured.
 *
 * @param mpu                 MPU context
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
memx_status memx_mpu_get_ofmap_size(MemxMpu* mpu, uint8_t flow_id, int32_t* height, int32_t* width, int32_t* z, int32_t* channel_number, int32_t* format, int32_t timeout);

/**
 * @brief Read back output feature map HPOC(High-Precision-Output-Channel)
 * setting. The 'hpoc_indexes' returned is owned by driver in case 'hpoc_size'
 * larger than zero, and should not be modified by user.
 *
 * @param mpu                 MPU context
 * @param flow_id             target flow ID
 * @param hpoc_size           high-precision-output-channel number
 * @param hpoc_indexes        high-precision-output-channel index array (read-only, do not modify)
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_get_ofmap_hpoc(MemxMpu* mpu, uint8_t flow_id, int32_t* hpoc_size, int32_t** hpoc_indexes, int32_t timeout);

/**
 * @brief Read back the chip gen for the given MPU.
 *
 * @param mpu                 MPU context
 * @param chip_gen            chip gen of device (MEMX_MPU_CHIP_GEN_CASCADE_PLUS)
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_get_chip_gen(MemxMpu* mpu, uint8_t* chip_gen, int32_t timeout);

/**
 * @brief Read back the chip gen for the given MPU.
 *
 * @param mpu                 MPU context
 * @param grop_id             device index
 * @param state               This field indicates the new power state into which the controller is requested to transition.
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_set_powerstate(MemxMpu* mpu, uint8_t group_id, uint8_t state);

/**
 * @brief Enqueue input feature map buffer into queue. The input feature map (frame) size
 * should be pre-configured in advance before using this function.
 *
 * @param mpu               MPU context
 * @param flow_id           target flow ID
 * @param fmap_buf          feature map buffer
 * @param timeout           milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_enqueue_ifmap_buf(MemxMpu* mpu, uint8_t flow_id, memx_fmap_buf_t* fmap_buf, int32_t timeout);

/**
 * @brief Enqueue output feature map buffer into queue. The output feature map (frame)
 * size should be pre-configured in advance before using this function.
 *
 * @param mpu               MPU context
 * @param flow_id           target flow ID
 * @param fmap_buf          feature map buffer
 * @param timeout           milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_enqueue_ofmap_buf(MemxMpu* mpu, uint8_t flow_id, memx_fmap_buf_t* fmap_buf, int32_t timeout);

/**
 * @brief Dequeue input feature map buffer from queue. The input feature map (frame) size
 * should be pre-configured in advance before using this function.
 *
 * @param mpu               MPU context
 * @param flow_id           target flow ID
 * @param fmap_buf          feature map buffer
 * @param timeout           milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_dequeue_ifmap_buf(MemxMpu* mpu, uint8_t flow_id, memx_fmap_buf_t* fmap_buf, int32_t timeout);

/**
 * @brief Dequeue output feature map buffer from queue. The input feature map (frame) size
 * should be pre-configured in advance before using this function.
 *
 * @param mpu               MPU context
 * @param flow_id           target flow ID
 * @param fmap_buf          feature map buffer
 * @param timeout           milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_mpu_dequeue_ofmap_buf(MemxMpu* mpu, uint8_t flow_id, memx_fmap_buf_t* fmap_buf, int32_t timeout);

/**
 * @brief Set up read abort flag to the given MPU context.
 *
 * @param mpu               MPU context
 *
 * @return none
 */
memx_status memx_mpu_set_read_abort(MemxMpu* mpu);

#ifdef __cplusplus
}
#endif

#endif /* MEMX_MPU_H_ */
