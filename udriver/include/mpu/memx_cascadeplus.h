/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#ifndef MEMX_CASCADE_PLUS_H_
#define MEMX_CASCADE_PLUS_H_

#ifdef __cplusplus
extern "C" {
#endif
#include "memx_mpu.h"

/**
 * @brief Create new MPU context which requires an IO handle and a given MPU group ID
 * To be noticed, this method will not configure MPU group ID to device but all
 * operations later will use the given IO handle and MPU group ID.
 *
 * @param mpu                 MPU context
 * @param mpu_group_id        MPU group ID used to access device
 *
 * @return new created MPU context on success, otherwise nullptr
 */
MemxMpu* memx_cascade_plus_create(MemxMpu* mpu, uint8_t mpu_group_id);

/**
 * @brief Destroy the given MPU context and clean-up all resources allocated
 * within original 'create' function.
 *
 * @param mpu                 MPU context
 *
 * @return none
 */
void memx_cascade_plus_destroy(MemxMpu* mpu);

/**
 * @brief Runtime re-configure binded IO handle. Will pause all background
 * workers and resume them after configuration change if it is running.
 *
 * @param mpu                 MPU context
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_cascade_plus_reconfigure(MemxMpu* mpu, int32_t timeout);

/**
 * @brief General purpose Cascade operation. Will forward the command given to
 * MPUIO binded if specific command is unrecognized.
 *
 * @param mpu                 MPU context
 * @param cmd_id              command ID
 * @param data                command data
 * @param size                command data size
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_cascade_plus_operation(MemxMpu* mpu, int32_t cmd_id, void* data, uint32_t size, int32_t timeout);

/**
 * @brief Set input feature map to device. The input feature map (frame) size
 * should be pre-configured in advance before using this function.
 *
 * @param mpu								MPU context
 * @param flow_id							target flow ID
 * @param from_user_ifmap_data_buffer       input feature map (frame)
 * @param delay_time_in_ms					milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_cascade_plus_stream_ifmap(MemxMpu *mpu, uint8_t flow_id, void *from_user_ifmap_data_buffer, int32_t delay_time_in_ms);
/**
 * @brief Get output feature map from device. The output feature map (frame)
 * size should be pre-configured in advance before using this function.
 *
 * @param mpu								MPU context
 * @param flow_id							target flow ID
 * @param to_user_ofmap_data_buffer         input feature map (frame)
 * @param delay_time_in_ms					milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_cascade_plus_stream_ofmap(MemxMpu *mpu, uint8_t flow_id, void *to_user_ofmap_data_buffer, int32_t delay_time_in_ms);
/**
 * @brief Download model data flow program to device. Data flow program should
 * be generated using MIX compiler. Will try to read input feature map and
 * output feature map configuration from DFP file.
 *
 * @param mpu                 MPU context
 * @param file_path           model dfp file, which by default should be named as '<device_name>.dfp'
 * @param model_idx           which rgcfg to use (used for model swapping). Set to 0 if only 1 model.
 * @param type                dfp type, 0: wtmem, 1: config
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_cascade_plus_download_model(MemxMpu* mpu, void * pDfpMeta, uint8_t model_idx, int32_t type, int32_t timeout);

/**
 * @brief Download firmware to device flash.
 *
 * @param mpu                 MPU context
 * @param file_path           firmware file, which by default should be named as 'cascade.bin'
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_cascade_plus_download_firmware(MemxMpu *mpu, const char *file_path);

/**
 * @brief Enable all input and output data flows of this MPU context to
 * interface driver. More detailly, this funciton will unblock all background
 * workers to move data from both user queue to interface and interface to
 * user queue.
 *
 * @param mpu                 MPU context
 * @param wait                wait until state changed
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_cascade_plus_set_stream_enable(MemxMpu* mpu, int32_t wait, int32_t timeout);

/**
 * @brief Disable all input and output data flows of this MPU context to
 * interface driver. More detailly, this function will block all background
 * workers to move data from both user queue to interface and interface to
 * user queue.
 *
 * @param mpu                 MPU context
 * @param wait                wait until state changed
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_cascade_plus_set_stream_disable(MemxMpu* mpu, int32_t wait, int32_t timeout);

/**
 * @brief Set internal input feature map queue size. All iput data flows of this
 * MPU context share the same setting value. By default, queue size '4' is
 * applied and can be adjusted dynamically.
 *
 * @param mpu                 MPU context
 * @param size                input feature map queue size
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_cascade_plus_set_ifmap_queue_size(MemxMpu* mpu, int32_t size, int32_t timeout);

/**
 * @brief Configure model input feature map size, which will adjust driver
 * internal buffer size and should be configured in the very beginning before
 * data stream starts.
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
memx_status memx_cascade_plus_set_ifmap_size(MemxMpu* mpu, uint8_t flow_id, int32_t height, int32_t width, int32_t z, int32_t channel_number, int32_t format, int32_t timeout);

/**
 * @brief Configure driver to do input feature map range conversion. This
 * function must be called after 'memx_cascade_plus_set_ifmap_size' since it will
 * reshape internal buffer size based on shape configuration. The conversion
 * should always be enabled when feature map format from user is float32.
 *
 * @param mpu                 MPU context
 * @param flow_id             target flow ID
 * @param shift               input feature map shift constant, which will be added to original value
 * @param scale               input feature map scale factor, which will multiply value after shifted
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_cascade_plus_set_ifmap_range_convert(MemxMpu* mpu, uint8_t flow_id, float shift, float scale, int32_t timeout);

/**
 * @brief Set internal output feature map queue size. All output data flows of
 * this MPU context share the same setting value. By default, queue size '4' is
 * applied and can be adjusted dynamically.
 *
 * @param mpu                 MPU context
 * @param size                output feature map queue size
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_cascade_plus_set_ofmap_queue_size(MemxMpu* mpu, int32_t size, int32_t timeout);

/**
 * @brief Configure model output feature map size, which will adjust driver
 * internal buffer size and should be configured in the very beginning before
 * data stream starts.
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
memx_status memx_cascade_plus_set_ofmap_size(MemxMpu* mpu, uint8_t flow_id, int32_t height, int32_t width, int32_t z, int32_t channel_number, int32_t format, int32_t timeout);

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
memx_status memx_cascade_plus_set_ofmap_hpoc(MemxMpu* mpu, uint8_t flow_id, int32_t hpoc_size, int32_t* hpoc_indexes, int32_t timeout);

/**
 * @brief Update model input/output feature map size, which will program driver
 * internal buffer size and should be configured in the very beginning before
 * data stream starts.
 *
 * @param mpu                 MPU context
 * @param in_flow_count       input flow count
 * @param out_flow_count      output flow count
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_cascade_plus_update_fmap_size(MemxMpu* mpu, uint8_t in_flow_count, uint8_t out_flow_count, int32_t timeout);

/**
 * @brief Enqueue input feature map buffer into queue. The input feature map (frame) size
 * should be pre-configured in advance before using this function.
 *
 * @param mpu								MPU context
 * @param flow_id							target flow ID
 * @param fmap_buf                          feature map buffer
 * @param timeout					        milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_cascade_plus_enqueue_ifmap_buf(MemxMpu* mpu, uint8_t flow_id,  memx_fmap_buf_t* fmap_buf, int32_t timeout);

/**
 * @brief Enqueue output feature map buffer into queue. The output feature map (frame)
 * size should be pre-configured in advance before using this function.
 *
 * @param mpu								MPU context
 * @param flow_id							target flow ID
 * @param fmap_buf                          feature map buffer
 * @param timeout					        milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_cascade_plus_enqueue_ofmap_buf(MemxMpu* mpu, uint8_t flow_id,  memx_fmap_buf_t* fmap_buf, int32_t timeout);

/**
 * @brief Dequeue input feature map buffer from queue. The input feature map (frame) size
 * should be pre-configured in advance before using this function.
 *
 * @param mpu								MPU context
 * @param flow_id							target flow ID
 * @param fmap_buf                          feature map buffer
 * @param timeout				         	milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_cascade_plus_dequeue_ifmap_buf(MemxMpu* mpu, uint8_t flow_id, memx_fmap_buf_t* fmap_buf, int32_t timeout);

/**
 * @brief Dequeue output feature map buffer from queue. The input feature map (frame) size
 * should be pre-configured in advance before using this function.
 *
 * @param mpu								MPU context
 * @param flow_id							target flow ID
 * @param fmap_buf                          feature map buffer
 * @param timeout					        milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_cascade_plus_dequeue_ofmap_buf(MemxMpu* mpu, uint8_t flow_id,  memx_fmap_buf_t* fmap_buf, int32_t timeout);

#ifdef __cplusplus
}
#endif

#endif /* MEMX_CASCADE_PLUS_H_ */
