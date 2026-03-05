/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2023 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#include "memx_cascadeplus.h"
#include "memx_mpu_comm.h"
#include "memx_gbf.h"
#include "memx_log.h"
#include "memx_device_manager.h"

#include <stdlib.h>

#define _CASCADE_PLUS_MPU_IFMAP_FLOW_NUMBER (32) // cascade_plus input port number
#define _CASCADE_PLUS_MPU_DEFAULT_IFMAP_FRAME_BUFFER_SIZE (4) // ifmap frame buffer size, used in buffering data to interface driver
#define _CASCADE_PLUS_MPU_MAX_IFMAP_FRAME_BUFFER_SIZE     (1000)// maximum ifmap frame buffer size

#define _CASCADE_PLUS_MPU_OFMAP_FLOW_NUMBER (32) // cascade_plus output port number
#define _CASCADE_PLUS_MPU_DEFAULT_OFMAP_FRAME_BUFFER_SIZE (4) // ofmap frame buffer size, used in buffering data from interface driver
#define _CASCADE_PLUS_MPU_MAX_OFMAP_FRAME_BUFFER_SIZE     (1000)// maximum ifmap frame buffer size

#define _memx_mpu_worker_is_pending(_worker_info_)  ((_worker_info_->enable == 1) && (_worker_info_->state_next == _mpu_worker_state_pending))
#define _memx_mpu_worker_is_blocking(_worker_info_) ((_worker_info_->enable == 1) && (_worker_info_->state_next == _mpu_worker_state_running) && (_worker_info_->blocking == 1))

/**
 * @brief cascade_plus context which stores device dependent information.
 */
typedef struct _MemxCascadePlus
{
  MemxMpuIfmapInfo ifmap_infos[_CASCADE_PLUS_MPU_IFMAP_FLOW_NUMBER]; // input feature map configuration
  MemxMpuFmapRingBuffer ifmap_ringbuffers[_CASCADE_PLUS_MPU_IFMAP_FLOW_NUMBER]; // input feature map row-based ring-buffer descriptor
  MemxMpuWorkerInfo ifmap_worker_info; // input feature map background worker, push data from ring-buffer to interface driver
  int32_t ifmap_frame_buffer_size; // input feature map frame buffer size

  MemxMpuOfmapInfo ofmap_infos[_CASCADE_PLUS_MPU_OFMAP_FLOW_NUMBER]; // output feature map configuration
  MemxMpuFmapRingBuffer ofmap_ringbuffers[_CASCADE_PLUS_MPU_OFMAP_FLOW_NUMBER]; // output feature map row-based ring-buffer descriptor
  MemxMpuWorkerInfo ofmap_worker_info; // output feature map background worker, poll data from interface driver to ring-buffer
  int32_t ofmap_frame_buffer_size; // output feature map frame buffer size
} MemxCascadePlus;

/***************************************************************************//**
 * internal helper
 ******************************************************************************/
/**
 * @brief Calculates maximum number of entries available within ring-buffer
 * and send to interface driver. In order to utilize interface's bandwidth, try
 * to transfer as much data as possible at once.
 *
 * @param ifmap_worker_info   ifmap worker control information
 * @param flow_id             target flow ID
 *
 * @return byte size transferred
 */
size_t _memx_cascade_plus_ifmap_worker_transfer(MemxMpuWorkerInfo *ifmap_worker_info, uint8_t flow_id)
{
    if (!ifmap_worker_info) { printf("ifmap_worker_transfer: worker_info is NULL\n"); return 0; }
    if (flow_id >= _CASCADE_PLUS_MPU_IFMAP_FLOW_NUMBER) { printf("ifmap_worker_transfer: flow_id(%d) is invaild\n", flow_id); return 0; }
    if (!ifmap_worker_info->mpu || !ifmap_worker_info->mpu->context) { printf("ifmap_worker_transfer: mpu or context is NULL\n"); return 0; }
    memx_status status = MEMX_STATUS_OK;
    MemxMpu *mpu = ifmap_worker_info->mpu;
    MemxCascadePlus *cascade_plus = (MemxCascadePlus *)ifmap_worker_info->mpu->context;
    MemxMpuFmapRingBuffer *ifmap_ringbuffer = &cascade_plus->ifmap_ringbuffers[flow_id];
    size_t max_num_of_frame = ifmap_ringbuffer->CascadePlus.max_num_of_frame;
    size_t size_of_one_frame = ifmap_ringbuffer->CascadePlus.size_of_one_frame;
    uint8_t *ring_buffer_data_start = ifmap_ringbuffer->CascadePlus.data;

    size_t nume_of_entry_need_process = 0;
    size_t local_entry_w = 0;
    platform_mutex_lock(&ifmap_ringbuffer->CascadePlus.read_guard);
    local_entry_w = ifmap_ringbuffer->CascadePlus.entry_w;
    if (local_entry_w > ifmap_ringbuffer->CascadePlus.entry_r) {
        // legal normal case :
        nume_of_entry_need_process = local_entry_w - ifmap_ringbuffer->CascadePlus.entry_r;
    } else if (local_entry_w < ifmap_ringbuffer->CascadePlus.entry_r) {
        // legal wrapper case :
        nume_of_entry_need_process = max_num_of_frame - ifmap_ringbuffer->CascadePlus.entry_r;
    } else {
        // illegal case : skip this transfer process for this ring buffer
        printf("illegal expected case happen\n");
        return 0;
    }
    MEMX_LOG(MEMX_LOG_MPU_IFMAP_FLOW, "pop from flow[%d] queue\n", flow_id);

    // try to transfer as much data as possible at once
    size_t expected_total_transfer_size = nume_of_entry_need_process * size_of_one_frame;
    size_t data_buffer_offset = ifmap_ringbuffer->CascadePlus.entry_r * size_of_one_frame;

    size_t actually_total_transfer_size = 0;
    // once transfer has began, let it finish or be stopped by user
    while (actually_total_transfer_size < expected_total_transfer_size) {
        int32_t transferred_length = 0;
        uint8_t *data_buffer = ring_buffer_data_start + data_buffer_offset + actually_total_transfer_size;
        int32_t data_length = (int32_t)(expected_total_transfer_size - actually_total_transfer_size);
        // for windows driver, it may sleep for Platform_Write API
        status = memx_mpuio_stream_write(mpu->mpuio, mpu->input_chip_id, flow_id, data_buffer, data_length, &transferred_length, 0);
        if(memx_status_error(status)){
            printf("memx_mpuio_stream_write error %d\r\n", status);
        }
        actually_total_transfer_size += transferred_length;
        // TODO: delay a bit if interface driver did not take data
        if (transferred_length == 0) {
            platform_usleep(10);
        }
    }

    // update pointer only if data has been transferred successfully
    for (size_t i = 0; i < nume_of_entry_need_process; i++) {
        ifmap_ringbuffer->CascadePlus.entry_r = ((ifmap_ringbuffer->CascadePlus.entry_r + 1) == ifmap_ringbuffer->CascadePlus.max_num_of_frame) ?
                                                                                    0 : ifmap_ringbuffer->CascadePlus.entry_r + 1;
    }
    // signal ifmap ringbuffer poniter changed to ifmap encode job
    pthread_cond_signal_with_lock(&ifmap_ringbuffer->CascadePlus.pointer_changed, &ifmap_ringbuffer->CascadePlus.guard);
    platform_mutex_unlock(&ifmap_ringbuffer->CascadePlus.read_guard);
    return actually_total_transfer_size;
}

/**
 * @brief Non-blocking write ifmap to mpuio, try write 64k each time and update ring buffer after whole frame transfer done.
 *
 * @param ifmap_worker_info   ifmap worker control information
 * @param flow_id             target flow ID
 *
 * @return byte size transferred
 */
size_t _memx_cascade_plus_ifmap_worker_transfer_noblocking(MemxMpuWorkerInfo *ifmap_worker_info, uint8_t flow_id)
{
    if (!ifmap_worker_info) { printf("ifmap_worker_transfer: worker_info is NULL\n"); return 0; }
    if (flow_id >= _CASCADE_PLUS_MPU_IFMAP_FLOW_NUMBER) { printf("ifmap_worker_transfer: flow_id(%d) is invaild\n", flow_id); return 0; }
    if (!ifmap_worker_info->mpu || !ifmap_worker_info->mpu->context) { printf("ifmap_worker_transfer: mpu or context is NULL\n"); return 0; }

    memx_status status = MEMX_STATUS_OK;
    MemxMpu *mpu = ifmap_worker_info->mpu;
    MemxCascadePlus *cascade_plus = (MemxCascadePlus *)ifmap_worker_info->mpu->context;
    MemxMpuFmapRingBuffer *ifmap_ringbuffer = &cascade_plus->ifmap_ringbuffers[flow_id];
    MemxMpuIfmapInfo *ifmap_info = &cascade_plus->ifmap_infos[flow_id];
    size_t size_of_one_frame = ifmap_ringbuffer->CascadePlus.size_of_one_frame;
    int64_t acctually_total_transferred_size = 0;
    uint8_t *ring_buffer_data = ifmap_ringbuffer->CascadePlus.data + ifmap_ringbuffer->CascadePlus.entry_r * size_of_one_frame + ifmap_ringbuffer->CascadePlus.transferred_size;
    int32_t transferred = 0;
    int64_t write_len = (ifmap_info->fmap.fmap_row_size & ~0x3) ? (ifmap_info->fmap.fmap_row_size & ~0x3) : 4;
    // FIXME: Increasing write_len can improve some multi-input models, but it’s challenging to determine the ideal write_len that won’t cause a hang.

    if (mpu->mpuio->ifmap_control && (size_of_one_frame < 0x40000) && // SRAM 256KB
        ((int64_t)size_of_one_frame / write_len) > (int64_t)cascade_plus->ifmap_frame_buffer_size) {
        write_len = ((int64_t)size_of_one_frame / (int64_t)cascade_plus->ifmap_frame_buffer_size) & ~0x3;
    }
    platform_mutex_lock(&ifmap_ringbuffer->CascadePlus.read_guard);

    int64_t remain_write_len = size_of_one_frame - ifmap_ringbuffer->CascadePlus.transferred_size;
    if (remain_write_len < write_len)
        write_len = remain_write_len;

    status = memx_mpuio_stream_write(mpu->mpuio, mpu->input_chip_id, flow_id, ring_buffer_data, (int32_t)write_len, &transferred, 0);
    if (memx_status_error(status)) { // debug message on error
        printf("memx_mpuio_stream_write fail at line(%d), status(%d)\n", __LINE__, status);
    }
    acctually_total_transferred_size += transferred;

    if (acctually_total_transferred_size > 0) {
        MEMX_LOG(MEMX_LOG_MPU_IFMAP_FLOW, "flow[%d] write %" PRId64 "data\n", flow_id, acctually_total_transferred_size);
        ifmap_ringbuffer->CascadePlus.transferred_size += acctually_total_transferred_size;
        ifmap_ringbuffer->CascadePlus.transferred_row += 1;

        if (ifmap_ringbuffer->CascadePlus.transferred_size == size_of_one_frame) {
            ifmap_ringbuffer->CascadePlus.transferred_size = 0;
            ifmap_ringbuffer->CascadePlus.transferred_row = 0;
            ifmap_ringbuffer->CascadePlus.transferred_one_frame = 1;

            // update pointer only if data has been transferred successfully
            ifmap_ringbuffer->CascadePlus.entry_r = ((ifmap_ringbuffer->CascadePlus.entry_r + 1) == ifmap_ringbuffer->CascadePlus.max_num_of_frame) ?
                                                                                        0 : ifmap_ringbuffer->CascadePlus.entry_r + 1;
            // signal ifmap ringbuffer poniter changed to ifmap encode job
            pthread_cond_signal_with_lock(&ifmap_ringbuffer->CascadePlus.pointer_changed, &ifmap_ringbuffer->CascadePlus.guard);
        }
    }
    platform_mutex_unlock(&ifmap_ringbuffer->CascadePlus.read_guard);

    return acctually_total_transferred_size;
}

/**
 * @brief Background worker helps to poll data out from all flow's ring-buffer
 * and send to interface driver.
 *
 * @param user_data           ifmap worker control information
 *
 * @return none
 */
void *_memx_cascade_plus_ifmap_worker_dowork(void *user_data)
{
    if (!user_data) { return NULL; }
    MemxMpuWorkerInfo *ifmap_worker_info = (MemxMpuWorkerInfo *)user_data;
    if (!ifmap_worker_info->mpu || !ifmap_worker_info->mpu->context) { return NULL; }
    MemxCascadePlus *cascade_plus = (MemxCascadePlus *)ifmap_worker_info->mpu->context;

    size_t load_balance_threshold = 0;
    uint32_t transfered_flow_row_mask = 0;
    uint32_t transfered_flow_frm_mask = 0;
    bool all_ringbuffer_empty = true;

    // infinite loop, stops only when program is terminated
    while (1) {
        platform_mutex_lock(&ifmap_worker_info->guard);
        while ((_memx_mpu_worker_is_pending(ifmap_worker_info)) ||
               (_memx_mpu_worker_is_blocking(ifmap_worker_info) && all_ringbuffer_empty == true)) {
            ifmap_worker_info->state = _mpu_worker_state_pending;
            platform_cond_wait(&ifmap_worker_info->state_changed, &ifmap_worker_info->guard);
        }
        ifmap_worker_info->state = _mpu_worker_state_running;
        ifmap_worker_info->blocking = 1;
        platform_mutex_unlock(&ifmap_worker_info->guard);
        all_ringbuffer_empty = true;

        // stop worker thread when disable
        if (ifmap_worker_info->enable == 0) { break; }

        if (ifmap_worker_info->state == _mpu_worker_state_running) {
            // do round-robin polling from each ifmap ring-buffer
            for (uint8_t flow_id = 0; flow_id < _CASCADE_PLUS_MPU_IFMAP_FLOW_NUMBER; ++flow_id) {
                MemxMpuFmapRingBuffer *ifmap_ringbuffer = &cascade_plus->ifmap_ringbuffers[flow_id]; // internal ring-buffer stores ifmaps with alignement to be transferred

                if ((ifmap_ringbuffer->CascadePlus.valid == 1) &&
                    (ifmap_ringbuffer->CascadePlus.entry_w != ifmap_ringbuffer->CascadePlus.entry_r)) {
                    uint32_t curr_flow_mask = (0x1 << flow_id);
                    if ((ifmap_worker_info->mpu->input_mode_flag & curr_flow_mask) == MEMX_DFP_INPUT_MODE_BLOCKING) {
                        size_t transferred_length = _memx_cascade_plus_ifmap_worker_transfer(ifmap_worker_info, flow_id);

                        if (!transferred_length) {
                            printf("_memx_cascade_plus_ifmap_worker_transfer[flow_id(%d)]: fail\n", flow_id);
                        }
                    } else {
                        // multi-input models use non-blocking mode
                        // apply load balance for each flow to avoid deadlock
                        bool check_flow_row_load_balance = ((transfered_flow_frm_mask == 0) &&
                                                            (!(transfered_flow_row_mask & curr_flow_mask) &&
                                                            ifmap_ringbuffer->CascadePlus.transferred_row <= load_balance_threshold));
                        bool check_flow_frm_load_balance = (transfered_flow_frm_mask && !(transfered_flow_frm_mask & curr_flow_mask));

                        if (check_flow_row_load_balance || check_flow_frm_load_balance) {
                            size_t transferred_length = _memx_cascade_plus_ifmap_worker_transfer_noblocking(ifmap_worker_info, flow_id);
                            if (!transferred_length) {
                                printf("_memx_cascade_plus_ifmap_worker_transfer[flow_id(%d)]: fail\n", flow_id);
                            } else {
                                transfered_flow_row_mask |= curr_flow_mask;

                                if (ifmap_ringbuffer->CascadePlus.transferred_one_frame) {
                                    ifmap_ringbuffer->CascadePlus.transferred_one_frame = 0;
                                    transfered_flow_frm_mask |= curr_flow_mask;
                                }

                                // update load balance threshold if current frame still in transfer
                                if (ifmap_ringbuffer->CascadePlus.transferred_row) {
                                    load_balance_threshold = ifmap_ringbuffer->CascadePlus.transferred_row;
                                }

                                if (transfered_flow_row_mask == ifmap_worker_info->mpu->input_mode_flag) {
                                    transfered_flow_row_mask = 0;
                                }

                                if (transfered_flow_frm_mask == ifmap_worker_info->mpu->input_mode_flag) {
                                    transfered_flow_frm_mask = 0;
                                }
                            }
                        }
                    }

                    if (ifmap_ringbuffer->CascadePlus.entry_w != ifmap_ringbuffer->CascadePlus.entry_r) {
                        all_ringbuffer_empty = false;
                    }
                }
            }
        }
    }
    return NULL;
}

/**
 * @brief Read data from interface driver and push frame data to ofmap list after whole frame data transferred.
 *
 * @param ofmap_worker_info   ofmap worker control information
 * @param flow_id             target flow ID
 *
 * @return byte size transferred
 */
size_t _memx_cascade_plus_ofmap_worker_transfer(MemxMpuWorkerInfo *ofmap_worker_info, uint8_t flow_id)
{
    if (!ofmap_worker_info) { printf("ofmap_worker_transfer: ofmap_worker_info is NULL\n"); return 0; }
    if (flow_id >= _CASCADE_PLUS_MPU_OFMAP_FLOW_NUMBER) { printf("ofmap_worker_transfer: invild flow_id(%d)\n", flow_id); return 0; }
    if (!ofmap_worker_info->mpu || !ofmap_worker_info->mpu->context) {  printf("ofmap_worker_transfer: mpu or context is NULL\n"); return 0; }

    memx_status status = MEMX_STATUS_OK;
    MemxMpu *mpu = ofmap_worker_info->mpu;
    MemxCascadePlus *cascade_plus = (MemxCascadePlus *)ofmap_worker_info->mpu->context;
    MemxMpuFmapRingBuffer *ofmap_ringbuffer = &cascade_plus->ofmap_ringbuffers[flow_id];
    size_t size_of_one_frame = ofmap_ringbuffer->CascadePlus.size_of_one_frame;
    int64_t acctually_total_transferred_size = 0;
    uint8_t *ring_buffer_data = ofmap_ringbuffer->CascadePlus.data + ofmap_ringbuffer->CascadePlus.entry_w * size_of_one_frame + ofmap_ringbuffer->CascadePlus.transferred_size;
    int32_t transferred = 0;

    status = memx_mpuio_stream_read(mpu->mpuio, mpu->output_chip_id, flow_id, ring_buffer_data, (int32_t)size_of_one_frame, &transferred, 1);
    if (memx_status_error(status)) { // debug message on error
        printf("memx_mpuio_stream_read fail at line(%d), status(%d)\n", __LINE__, status);
    }
    acctually_total_transferred_size += transferred;

    if (acctually_total_transferred_size > 0) {
        MEMX_LOG(MEMX_LOG_MPU_OFMAP_FLOW, "flow[%d] read %" PRId64 "data\n", flow_id, acctually_total_transferred_size);

        ofmap_ringbuffer->CascadePlus.transferred_size += acctually_total_transferred_size;

        if (ofmap_ringbuffer->CascadePlus.transferred_size == size_of_one_frame) {
            ofmap_ringbuffer->CascadePlus.transferred_size = 0;
            // update pointer after one whole output frame has been transferred successfully
            platform_mutex_lock(&ofmap_ringbuffer->CascadePlus.write_guard);
            ofmap_ringbuffer->CascadePlus.remain_buf_cnt++;
            ofmap_ringbuffer->CascadePlus.entry_w = ((ofmap_ringbuffer->CascadePlus.entry_w + 1) == ofmap_ringbuffer->CascadePlus.max_num_of_frame) ? 0 : ofmap_ringbuffer->CascadePlus.entry_w + 1;
            platform_mutex_unlock(&ofmap_ringbuffer->CascadePlus.write_guard);
        }
    }
    return acctually_total_transferred_size;
}

/**
 * @brief Background worker helps to poll data out from interface driver and
 * send to corresponding flow's ring-buffer.
 *
 * @param user_data           ofmap worker control information
 *
 * @return none
 */
void *_memx_cascade_plus_ofmap_worker_dowork(void *user_data)
{
    if (!user_data) { printf("ofmap_worker_dowork: user_data is NULL\n"); return NULL; }
    MemxMpuWorkerInfo *ofmap_worker_info = (MemxMpuWorkerInfo *)user_data;
    if (!ofmap_worker_info->mpu || !ofmap_worker_info->mpu->context) { printf("ofmap_worker_dowork: mpu or context is NULL\n"); return NULL; }
    MemxCascadePlus *cascade_plus = (MemxCascadePlus *)ofmap_worker_info->mpu->context;

    // infinite loop, stops only when program is terminated
    while (ofmap_worker_info->enable == 1) {
        // go to pending on demand
        _memx_mpu_worker_go_to_sleep(ofmap_worker_info);

        // do round-robin polling to check if any ofmap ring-buffer has space
        size_t is_any_data_transferred = 0;

        for (uint8_t flow_id = 0; flow_id  < _CASCADE_PLUS_MPU_OFMAP_FLOW_NUMBER; ++flow_id) {
            MemxMpuFmapRingBuffer *ofmap_ringbuffer = &cascade_plus->ofmap_ringbuffers[flow_id];
            // polling for enabled output flow
            if (ofmap_ringbuffer->CascadePlus.valid == 1) {
                // wait for ofmap list has free space or timeout
                if ((ofmap_ringbuffer->CascadePlus.entry_w + 1) % ofmap_ringbuffer->CascadePlus.max_num_of_frame != ofmap_ringbuffer->CascadePlus.entry_r) { // not full
                    is_any_data_transferred += _memx_cascade_plus_ofmap_worker_transfer(ofmap_worker_info, flow_id)  == 0 ? 0 : 1;
                }
            }
        }/* end of loop all flow */

        // cannot go to sleep since we can obtain ofmap only by polling
        // simply add some delay to thread if no data transferred
        if (is_any_data_transferred == 0) {
            platform_usleep(10);
        } // TODO: 1ms or 0.1ms delay is better?
    }

    return NULL;
}

/**
 * @brief Runtime input feature map information update. Based on cascade_plus's
 * design, ifmap should always be row and frame with 4 bytes alignment.
 * Frame-alignment here is hardware limitation while row-alignment is software
 * limitation for we transfer data to device by row to reduce latency.
 *
 * @param mpu                 MPU context
 * @param flow_id             target flow ID
 * @param height              input feature map height
 * @param width               input feature map width
 * @param z                   input feature map z
 * @param channel_number      input feature map channel number
 * @param format              input feature map format
 *
 * @return 0 on success, otherwise error code
 */
memx_status _memx_cacasde_plus_ifmap_info_update(MemxMpu *mpu, uint8_t flow_id, int32_t height, int32_t width, int32_t z, int32_t channel_number, int32_t format)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->context) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (flow_id >= _CASCADE_PLUS_MPU_IFMAP_FLOW_NUMBER) { return MEMX_STATUS_MPU_INVALID_FLOW_ID; }
    if (height <= 0 || width <= 0 || z <=0 || channel_number <= 0 || format < 0) { return MEMX_STATUS_MPU_INVALID_PARAMETER; }

    MemxCascadePlus *cascade_plus = (MemxCascadePlus *)mpu->context;
    MemxMpuIfmapInfo *ifmap_info = &cascade_plus->ifmap_infos[flow_id];

    // cascade plus support direct float/rgb888/gbf input
    // using frame pad for better performance since we don't need encode by row for pipeline
    // update new input feature map size
    switch (format) {
    case MEMX_MPU_IFMAP_FORMAT_FLOAT32: {
        ifmap_info->fmap.format = format;
        ifmap_info->fmap.channel_number = channel_number;
        ifmap_info->fmap.z = z;
        ifmap_info->fmap.width = width;
        ifmap_info->fmap.height = height;
        ifmap_info->fmap.channel_size = sizeof(float);
        ifmap_info->fmap.row_size = width * z * channel_number;
        ifmap_info->fmap.user_frame_size = height * ifmap_info->fmap.row_size;
        ifmap_info->fmap.fmap_channel_number = channel_number;
        ifmap_info->fmap.fmap_pixel_size = ifmap_info->fmap.fmap_channel_number * sizeof(float);
        ifmap_info->fmap.fmap_row_size = ifmap_info->fmap.row_size * sizeof(float);
        ifmap_info->fmap.fmap_frame_size = (height * ifmap_info->fmap.fmap_row_size + 3) & ~0x3; // frame padding to 4 bytes
    } break;
    case MEMX_MPU_IFMAP_FORMAT_GBF80: {
        ifmap_info->fmap.format = format;
        ifmap_info->fmap.channel_number = channel_number;
        ifmap_info->fmap.z = z;
        ifmap_info->fmap.width = width;
        ifmap_info->fmap.height = height;
        ifmap_info->fmap.channel_size = sizeof(uint8_t);
        ifmap_info->fmap.row_size = width * z * ((channel_number+7) >> 3) * 10;
        ifmap_info->fmap.user_frame_size = (height * ifmap_info->fmap.row_size + 3) & ~0x3; // frame padding to 4 bytes
        memx_gbf_get_gbf80_channel_number_reshaped(&ifmap_info->fmap.fmap_channel_number, channel_number);
        ifmap_info->fmap.fmap_pixel_size = (ifmap_info->fmap.fmap_channel_number >> 3) * 10;
        ifmap_info->fmap.fmap_row_size = ifmap_info->fmap.row_size;
        ifmap_info->fmap.fmap_frame_size = (height * ifmap_info->fmap.fmap_row_size + 3) & ~0x3; // frame padding to 4 bytes
    } break;
    case MEMX_MPU_IFMAP_FORMAT_GBF80_ROW_PAD: {
        ifmap_info->fmap.format = format;
        ifmap_info->fmap.channel_number = channel_number;
        ifmap_info->fmap.z = z;
        ifmap_info->fmap.width = width;
        ifmap_info->fmap.height = height;
        ifmap_info->fmap.channel_size = sizeof(uint8_t);
        ifmap_info->fmap.row_size = width * z * ((channel_number+7) >> 3) * 10;
        ifmap_info->fmap.user_frame_size = height * ifmap_info->fmap.row_size;
        memx_gbf_get_gbf80_channel_number_reshaped(&ifmap_info->fmap.fmap_channel_number, channel_number);
        ifmap_info->fmap.fmap_pixel_size = (ifmap_info->fmap.fmap_channel_number >> 3) * 10;
        memx_gbf_get_gbf80_row_size_reshaped(&ifmap_info->fmap.fmap_row_size, width, z, channel_number);
        memx_gbf_get_gbf80_frame_size_reshaped(&ifmap_info->fmap.fmap_frame_size, height, width, z, channel_number);
        ifmap_info->fmap.user_frame_size = ifmap_info->fmap.fmap_frame_size; // with padding size
    } break;
    case MEMX_MPU_IFMAP_FORMAT_RGB565:
    case MEMX_MPU_IFMAP_FORMAT_YUV422: {
        ifmap_info->fmap.format = format;
        ifmap_info->fmap.channel_number = channel_number;
        ifmap_info->fmap.z = z;
        ifmap_info->fmap.width = width;
        ifmap_info->fmap.height = height;
        ifmap_info->fmap.channel_size = sizeof(uint8_t);
        ifmap_info->fmap.row_size = width * z * channel_number * 2 / 3;
        ifmap_info->fmap.user_frame_size = height * ifmap_info->fmap.row_size;
        ifmap_info->fmap.fmap_channel_number = channel_number;
        ifmap_info->fmap.fmap_pixel_size = ifmap_info->fmap.fmap_channel_number * 2 / 3;
        ifmap_info->fmap.fmap_row_size = ifmap_info->fmap.row_size;
        ifmap_info->fmap.fmap_frame_size = (height * ifmap_info->fmap.fmap_row_size + 3) & ~0x3; // frame padding to 4 bytes
    } break;
        case MEMX_MPU_IFMAP_FORMAT_FLOAT16: {
        ifmap_info->fmap.format = format;
        ifmap_info->fmap.channel_number = channel_number;
        ifmap_info->fmap.z = z;
        ifmap_info->fmap.width = width;
        ifmap_info->fmap.height = height;
        ifmap_info->fmap.channel_size = (sizeof(float) >> 1);
        ifmap_info->fmap.row_size = width * z * channel_number;
        ifmap_info->fmap.user_frame_size = height * ifmap_info->fmap.row_size;
        ifmap_info->fmap.fmap_channel_number = channel_number;
        ifmap_info->fmap.fmap_pixel_size = ifmap_info->fmap.fmap_channel_number * (sizeof(float) >> 1);
        ifmap_info->fmap.fmap_row_size = ifmap_info->fmap.row_size * (sizeof(float) >> 1);
        ifmap_info->fmap.fmap_frame_size = (height * ifmap_info->fmap.fmap_row_size + 3) & ~0x3; // frame padding to 4 bytes
    } break;
    default: { // MEMX_MPU_IFMAP_FORMAT_RAW
        ifmap_info->fmap.format = format;
        ifmap_info->fmap.channel_number = channel_number;
        ifmap_info->fmap.z = z;
        ifmap_info->fmap.width = width;
        ifmap_info->fmap.height = height;
        ifmap_info->fmap.channel_size = sizeof(uint8_t);
        ifmap_info->fmap.row_size = width * z * channel_number;
        ifmap_info->fmap.user_frame_size = height * ifmap_info->fmap.row_size;
        ifmap_info->fmap.fmap_channel_number = channel_number;
        ifmap_info->fmap.fmap_pixel_size = channel_number;
        ifmap_info->fmap.fmap_row_size = ifmap_info->fmap.row_size;
        ifmap_info->fmap.fmap_frame_size = (height * ifmap_info->fmap.fmap_row_size + 3) & ~0x3; // frame padding to 4 bytes
    }
    }
    // TODO: can be remove?
    // input floating-point to rgb888 range conversion
    ifmap_info->range_convert_shift = 0;
    ifmap_info->range_convert_scale = 0;

    MEMX_LOG(MEMX_LOG_MPU_IFMAP_FLOW, "flow[%d] ifmap info: \n", flow_id);
    MEMX_LOG(MEMX_LOG_MPU_IFMAP_FLOW, " - format: %d \n", ifmap_info->fmap.format);
    MEMX_LOG(MEMX_LOG_MPU_IFMAP_FLOW, " - channel_number: %d\n", ifmap_info->fmap.channel_number);
    MEMX_LOG(MEMX_LOG_MPU_IFMAP_FLOW, " - z: %d \n", ifmap_info->fmap.z);
    MEMX_LOG(MEMX_LOG_MPU_IFMAP_FLOW, " - width: %d\n", ifmap_info->fmap.width);
    MEMX_LOG(MEMX_LOG_MPU_IFMAP_FLOW, " - height: %d\n", ifmap_info->fmap.height);
    MEMX_LOG(MEMX_LOG_MPU_IFMAP_FLOW, " - fmap_frame_size: %d\n", ifmap_info->fmap.fmap_frame_size);
    MEMX_LOG(MEMX_LOG_MPU_IFMAP_FLOW, " - user_frame_size: %d\n", ifmap_info->fmap.fmap_frame_size);


    return MEMX_STATUS_OK;
}

/**
 * @brief Re-configure input feature map with floating-point to RGB888 data
 * range conversion enabled. Input feature map buffer size will be re-shaped,
 * in case internal ring-buffer is also required to be updated.
 *
 * @param mpu                 MPU context
 * @param flow_id             target flow ID
 * @param shift               amount to shift before scale
 * @param scale               amount to scale before integer cast
 *
 * @return 0 on success, otherwise error code
 */
memx_status _memx_cascade_plus_ifmap_info_update_range_convert(MemxMpu *mpu, uint8_t flow_id, float shift, float scale)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->context) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (flow_id >= _CASCADE_PLUS_MPU_IFMAP_FLOW_NUMBER) { return MEMX_STATUS_MPU_INVALID_FLOW_ID; }
    MemxCascadePlus *cascade_plus = (MemxCascadePlus *)mpu->context;
    MemxMpuIfmapInfo *ifmap_info = &cascade_plus->ifmap_infos[flow_id];

    // follows configuration in MEMX_MPU_IFMAP_FORMAT_RAW
    ifmap_info->fmap.format = MEMX_MPU_IFMAP_FORMAT_FLOAT32_RGB888;
    ifmap_info->fmap.channel_number = ifmap_info->fmap.channel_number;
    ifmap_info->fmap.width = ifmap_info->fmap.width;
    ifmap_info->fmap.height = ifmap_info->fmap.height;
    ifmap_info->fmap.channel_size = sizeof(float); // user input format will be float32
    ifmap_info->fmap.row_size = ifmap_info->fmap.width * ifmap_info->fmap.channel_number;
    ifmap_info->fmap.user_frame_size = ifmap_info->fmap.height * ifmap_info->fmap.row_size;
    ifmap_info->fmap.fmap_channel_number = ifmap_info->fmap.channel_number;
    ifmap_info->fmap.fmap_pixel_size = ifmap_info->fmap.channel_number;
    ifmap_info->fmap.fmap_row_size = (ifmap_info->fmap.width * ifmap_info->fmap.fmap_channel_number + 3) & ~0x3;
    ifmap_info->fmap.fmap_frame_size = (ifmap_info->fmap.height * ifmap_info->fmap.fmap_row_size + 3) & ~0x3;
    ifmap_info->range_convert_shift = shift;
    ifmap_info->range_convert_scale = scale;

    return MEMX_STATUS_OK;
}

/**
 * @brief Runtime input feature map ring-buffer update.
 *
 * @param mpu                 MPU context
 * @param flow_id             target flow ID
 *
 * @return 0 on success, otherwise error code
 */
memx_status _memx_cascade_plus_ifmap_ringbuffer_update(MemxMpu *mpu, uint8_t flow_id)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->context) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (flow_id >= _CASCADE_PLUS_MPU_IFMAP_FLOW_NUMBER) { return MEMX_STATUS_MPU_INVALID_FLOW_ID; }
    MemxCascadePlus *cascade_plus = (MemxCascadePlus *)mpu->context;
    MemxMpuIfmapInfo *ifmap_info = &cascade_plus->ifmap_infos[flow_id];
    if (ifmap_info->fmap.user_frame_size <= 0) { return MEMX_STATUS_MPU_INVALID_DATALEN; }

    int32_t frame_buffer_size = cascade_plus->ifmap_frame_buffer_size;
    uint8_t *new_data_ring_buffer = (uint8_t *)malloc(ifmap_info->fmap.fmap_frame_size * sizeof(uint8_t) * frame_buffer_size);
    if (!new_data_ring_buffer) { return MEMX_STATUS_MPU_ALLOCATE_IFMAP_BUFFER_FAIL; }
    memset(new_data_ring_buffer, 0, ifmap_info->fmap.fmap_frame_size * sizeof(uint8_t) * frame_buffer_size);

    uint8_t *new_buf_status = (uint8_t *)malloc(sizeof(uint8_t) * frame_buffer_size);
    if (!new_buf_status) { return MEMX_STATUS_MPU_ALLOCATE_IFMAP_BUFFER_FAIL; }
    memset(new_buf_status, 0, sizeof(uint8_t) * frame_buffer_size);

    MemxMpuFmapRingBuffer *ifmap_ringbuffer = &cascade_plus->ifmap_ringbuffers[flow_id];
    // update new ring-buffer size, but do not enable flow immediately
    ifmap_ringbuffer->CascadePlus.max_num_of_frame = frame_buffer_size;
    ifmap_ringbuffer->CascadePlus.size_of_one_frame = ifmap_info->fmap.fmap_frame_size;
    ifmap_ringbuffer->CascadePlus.entry_r = 0;
    ifmap_ringbuffer->CascadePlus.entry_w = 0;
    ifmap_ringbuffer->CascadePlus.remain_buf_cnt = frame_buffer_size;
    ifmap_ringbuffer->CascadePlus.buf_status = new_buf_status;
    ifmap_ringbuffer->CascadePlus.read_buf_idx = 0;

    // re-allocate buffer memory space
    free(ifmap_ringbuffer->CascadePlus.data);
    ifmap_ringbuffer->CascadePlus.data = NULL;
    ifmap_ringbuffer->CascadePlus.data = new_data_ring_buffer;

    return MEMX_STATUS_OK;
}

/**
 * @brief Runtime output feature map information update. Based on cascade_plus's
 * design, ofmap should always be row and frame with 4 bytes alignment. Both
 * frame-alignment and row-alignment are hardware limitations.
 *
 * @param mpu                 MPU context
 * @param flow_id             target flow ID
 * @param height              output feature map height
 * @param width               output feature map width
 * @param z                   output feature map z
 * @param channel_number      output feature map channel number
 * @param format              output feature map format
 *
 * @return 0 on success, otherwise error code
 */
memx_status _memx_cacasde_plus_ofmap_info_update(MemxMpu *mpu, uint8_t flow_id, int32_t height, int32_t width, int32_t z, int32_t channel_number, int32_t format)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->context) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (flow_id >= _CASCADE_PLUS_MPU_OFMAP_FLOW_NUMBER) { return MEMX_STATUS_MPU_INVALID_FLOW_ID; }
    if (height <= 0 || width <= 0 || z <= 0 || channel_number <= 0 || format < 0) { return MEMX_STATUS_MPU_INVALID_PARAMETER; }

    MemxCascadePlus *cascade_plus = (MemxCascadePlus *)mpu->context;
    MemxMpuOfmapInfo *ofmap_info = &cascade_plus->ofmap_infos[flow_id];
    // update new input feature map size
    switch (format) {
    case MEMX_MPU_OFMAP_FORMAT_FLOAT32 : {
        ofmap_info->CascadePlus.fmap.format = format;
        ofmap_info->CascadePlus.fmap.channel_number = channel_number;
        ofmap_info->CascadePlus.fmap.z = z;
        ofmap_info->CascadePlus.fmap.width = width;
        ofmap_info->CascadePlus.fmap.height = height;
        ofmap_info->CascadePlus.fmap.channel_size = sizeof(float);
        ofmap_info->CascadePlus.fmap.row_size = width * z * channel_number * sizeof(float);
        ofmap_info->CascadePlus.fmap.user_frame_size = (height * ofmap_info->CascadePlus.fmap.row_size+ 3) & ~0x3; // frame padding to 4 bytes
        ofmap_info->CascadePlus.fmap.fmap_channel_number = channel_number;
        ofmap_info->CascadePlus.fmap.fmap_pixel_size = ofmap_info->CascadePlus.fmap.fmap_channel_number * sizeof(float);
        ofmap_info->CascadePlus.fmap.fmap_row_size = ofmap_info->CascadePlus.fmap.row_size;
        ofmap_info->CascadePlus.fmap.fmap_frame_size = (height * ofmap_info->CascadePlus.fmap.fmap_row_size + 3) & ~0x3; // frame padding to 4 bytes
    } break;
    case MEMX_MPU_OFMAP_FORMAT_GBF80 : {
        ofmap_info->CascadePlus.fmap.format = format;
        ofmap_info->CascadePlus.fmap.channel_number = channel_number;
        ofmap_info->CascadePlus.fmap.z = z;
        ofmap_info->CascadePlus.fmap.width = width;
        ofmap_info->CascadePlus.fmap.height = height;
        ofmap_info->CascadePlus.fmap.channel_size = sizeof(uint8_t);
        ofmap_info->CascadePlus.fmap.row_size = width * z * ((channel_number+7) >> 3) * 10;
        ofmap_info->CascadePlus.fmap.user_frame_size = height * ofmap_info->CascadePlus.fmap.row_size;
        memx_gbf_get_gbf80_channel_number_reshaped(&ofmap_info->CascadePlus.fmap.fmap_channel_number, channel_number);
        ofmap_info->CascadePlus.fmap.fmap_pixel_size = (ofmap_info->CascadePlus.fmap.fmap_channel_number >> 3) * 10;
        ofmap_info->CascadePlus.fmap.fmap_row_size = ofmap_info->CascadePlus.fmap.row_size;
        ofmap_info->CascadePlus.fmap.fmap_frame_size = (height * ofmap_info->CascadePlus.fmap.fmap_row_size + 3) & ~0x3; // frame padding to 4 bytes
    } break;
    case MEMX_MPU_OFMAP_FORMAT_GBF80_ROW_PAD : {
        ofmap_info->CascadePlus.fmap.format = format;
        ofmap_info->CascadePlus.fmap.channel_number = channel_number;
        ofmap_info->CascadePlus.fmap.z = z;
        ofmap_info->CascadePlus.fmap.width = width;
        ofmap_info->CascadePlus.fmap.height = height;
        ofmap_info->CascadePlus.fmap.channel_size = sizeof(uint8_t);
        ofmap_info->CascadePlus.fmap.row_size = width * ((channel_number+7) >> 3) * 10;
        memx_gbf_get_gbf80_channel_number_reshaped(&ofmap_info->CascadePlus.fmap.fmap_channel_number, channel_number);
        ofmap_info->CascadePlus.fmap.fmap_pixel_size = (ofmap_info->CascadePlus.fmap.fmap_channel_number >> 3) * 10;
        memx_gbf_get_gbf80_row_size_reshaped(&ofmap_info->CascadePlus.fmap.fmap_row_size, width, z, channel_number);
        memx_gbf_get_gbf80_frame_size_reshaped(&ofmap_info->CascadePlus.fmap.fmap_frame_size, height, width, z, channel_number);
        ofmap_info->CascadePlus.fmap.user_frame_size = ofmap_info->CascadePlus.fmap.fmap_frame_size; // with padding size
    } break;
    case MEMX_MPU_OFMAP_FORMAT_FLOAT16 : {
        ofmap_info->CascadePlus.fmap.format = format;
        ofmap_info->CascadePlus.fmap.channel_number = channel_number;
        ofmap_info->CascadePlus.fmap.z = z;
        ofmap_info->CascadePlus.fmap.width = width;
        ofmap_info->CascadePlus.fmap.height = height;
        ofmap_info->CascadePlus.fmap.channel_size = (sizeof(float) >> 1);
        ofmap_info->CascadePlus.fmap.row_size = width * z * channel_number * (sizeof(float) >> 1);
        ofmap_info->CascadePlus.fmap.user_frame_size = (height * ofmap_info->CascadePlus.fmap.row_size+ 3) & ~0x3; // frame padding to 4 bytes
        ofmap_info->CascadePlus.fmap.fmap_channel_number = channel_number;
        ofmap_info->CascadePlus.fmap.fmap_pixel_size = ofmap_info->CascadePlus.fmap.fmap_channel_number * (sizeof(float) >> 1);
        ofmap_info->CascadePlus.fmap.fmap_row_size = ofmap_info->CascadePlus.fmap.row_size;
        ofmap_info->CascadePlus.fmap.fmap_frame_size = (height * ofmap_info->CascadePlus.fmap.fmap_row_size + 3) & ~0x3; // frame padding to 4 bytes
    } break;
    default: { // MEMX_MPU_OFMAP_FORMAT_RAW
        ofmap_info->CascadePlus.fmap.format = format;
        ofmap_info->CascadePlus.fmap.channel_number = channel_number;
        ofmap_info->CascadePlus.fmap.z = z;
        ofmap_info->CascadePlus.fmap.width = width;
        ofmap_info->CascadePlus.fmap.height = height;
        ofmap_info->CascadePlus.fmap.channel_size = sizeof(uint8_t);
        ofmap_info->CascadePlus.fmap.row_size = width * z * channel_number;
        ofmap_info->CascadePlus.fmap.user_frame_size = height * ofmap_info->CascadePlus.fmap.row_size;
        ofmap_info->CascadePlus.fmap.fmap_channel_number = channel_number;
        ofmap_info->CascadePlus.fmap.fmap_pixel_size = channel_number;
        ofmap_info->CascadePlus.fmap.fmap_row_size = ofmap_info->CascadePlus.fmap.row_size;
        ofmap_info->CascadePlus.fmap.fmap_frame_size = (height * ofmap_info->CascadePlus.fmap.fmap_row_size + 3) & ~0x3; // frame padding to 4 bytes
    }
    }

    // high-precision-output-channel
    if (ofmap_info->CascadePlus.hpoc_dummy_channel_indexes != NULL) {
        free(ofmap_info->CascadePlus.hpoc_dummy_channel_indexes);
        ofmap_info->CascadePlus.hpoc_enabled = 0;
        ofmap_info->CascadePlus.hpoc_dummy_channel_indexes = NULL;
        ofmap_info->CascadePlus.hpoc_dummy_channel_number = 0;
        ofmap_info->CascadePlus.hpoc_dummy_channel_array_size = 0;
    }


    MEMX_LOG(MEMX_LOG_MPU_OFMAP_FLOW, "flow[%d] ofmap info: \n", flow_id);
    MEMX_LOG(MEMX_LOG_MPU_OFMAP_FLOW, " - format: %d \n", ofmap_info->CascadePlus.fmap.format);
    MEMX_LOG(MEMX_LOG_MPU_OFMAP_FLOW, " - channel_number: %d\n", ofmap_info->CascadePlus.fmap.channel_number);
    MEMX_LOG(MEMX_LOG_MPU_OFMAP_FLOW, " - z: %d \n", ofmap_info->CascadePlus.fmap.z);
    MEMX_LOG(MEMX_LOG_MPU_OFMAP_FLOW, " - width: %d\n", ofmap_info->CascadePlus.fmap.width);
    MEMX_LOG(MEMX_LOG_MPU_OFMAP_FLOW, " - height: %d\n", ofmap_info->CascadePlus.fmap.height);
    MEMX_LOG(MEMX_LOG_MPU_OFMAP_FLOW, " - fmap_frame_size: %d\n", ofmap_info->CascadePlus.fmap.fmap_frame_size);

    return MEMX_STATUS_OK;
}

/**
 * @brief Re-configure output feature map with HPOC (High-Precision Output
 * Channel) setting. Each HPOC occupy one GBF80 entry and be placed at the 1st
 * channel within this entry, which means dummy channels are inserted before and
 * after to make previous GBF80 and this GBF80 complete with 8 channels.
 *
 * @param mpu                 MPU context
 * @param flow_id             target flow ID
 * @param hpoc_size           high-precision-output-channel number
 * @param hpoc_indexes        high-precision-output-channel indexes
 *
 * @return 0 on success, otherwise error code
 */
memx_status _memx_cascade_plus_ofmap_info_update_hpoc(MemxMpu *mpu, uint8_t flow_id, int32_t hpoc_size, int32_t *hpoc_indexes)
{
    memx_status status = MEMX_STATUS_OK;
    MemxCascadePlus* cascade_plus = (MemxCascadePlus*)mpu->context;;
    MemxMpuOfmapInfo* ofmap_info = &cascade_plus->ofmap_infos[flow_id];

    // update output feature map setting
    // internal_ofmap will include hpoc dummy channel
    ofmap_info->CascadePlus.hpoc_enabled = 1;
    ofmap_info->CascadePlus.hpoc_dummy_channel_number = ofmap_info->CascadePlus.fmap.channel_number + hpoc_size;

    // re-calculate internal frame size with hpoc dummy channel
    switch (ofmap_info->CascadePlus.fmap.format) {
        case MEMX_MPU_OFMAP_FORMAT_FLOAT32 : {
            ofmap_info->CascadePlus.fmap.row_size = ofmap_info->CascadePlus.fmap.width * ofmap_info->CascadePlus.fmap.z * ofmap_info->CascadePlus.hpoc_dummy_channel_number * sizeof(float);
            ofmap_info->CascadePlus.fmap.fmap_row_size = ofmap_info->CascadePlus.fmap.row_size;
            ofmap_info->CascadePlus.fmap.fmap_frame_size = (ofmap_info->CascadePlus.fmap.height * ofmap_info->CascadePlus.fmap.fmap_row_size + 3) & ~0x3; // frame padding to 4 bytes
        } break;
        case MEMX_MPU_OFMAP_FORMAT_GBF80 : {
            ofmap_info->CascadePlus.fmap.row_size = ofmap_info->CascadePlus.fmap.width * ofmap_info->CascadePlus.fmap.z * ((ofmap_info->CascadePlus.hpoc_dummy_channel_number+7) >> 3) * 10;
            memx_gbf_get_gbf80_channel_number_reshaped(&ofmap_info->CascadePlus.fmap.fmap_channel_number, ofmap_info->CascadePlus.hpoc_dummy_channel_number);
            ofmap_info->CascadePlus.fmap.fmap_row_size = ofmap_info->CascadePlus.fmap.row_size;
            ofmap_info->CascadePlus.fmap.fmap_frame_size = (ofmap_info->CascadePlus.fmap.height * ofmap_info->CascadePlus.fmap.fmap_row_size + 3) & ~0x3; // frame padding to 4 bytes
            ofmap_info->CascadePlus.fmap.user_frame_size = ofmap_info->CascadePlus.fmap.fmap_frame_size;
        } break;
        case MEMX_MPU_OFMAP_FORMAT_GBF80_ROW_PAD : {
            ofmap_info->CascadePlus.fmap.row_size = ((ofmap_info->CascadePlus.fmap.width * ofmap_info->CascadePlus.fmap.z * ((ofmap_info->CascadePlus.fmap.channel_number+7) >> 3) * 10 ) + 3) & ~0x3; // row padding to 4 bytes;
            memx_gbf_get_gbf80_channel_number_reshaped(&ofmap_info->CascadePlus.fmap.fmap_channel_number, ofmap_info->CascadePlus.hpoc_dummy_channel_number);
            memx_gbf_get_gbf80_row_size_reshaped(&ofmap_info->CascadePlus.fmap.fmap_row_size, ofmap_info->CascadePlus.fmap.width, ofmap_info->CascadePlus.fmap.z, ofmap_info->CascadePlus.fmap.fmap_channel_number);
            ofmap_info->CascadePlus.fmap.fmap_frame_size = ofmap_info->CascadePlus.fmap.height * ofmap_info->CascadePlus.fmap.fmap_row_size;
            ofmap_info->CascadePlus.fmap.user_frame_size = ofmap_info->CascadePlus.fmap.fmap_frame_size;
        } break;
        default: {
            printf("TODO: HPOC format:%d \n", ofmap_info->CascadePlus.fmap.format);
        } break;
    }

    ofmap_info->CascadePlus.hpoc_dummy_channel_indexes = (int32_t*)malloc(sizeof(int32_t) * hpoc_size);
    if (!ofmap_info->CascadePlus.hpoc_dummy_channel_indexes) { return MEMX_STATUS_MPU_HPOC_CONFIG_FAIL; }
    memcpy(ofmap_info->CascadePlus.hpoc_dummy_channel_indexes, hpoc_indexes, sizeof(int32_t) * hpoc_size);
    ofmap_info->CascadePlus.hpoc_dummy_channel_array_size = hpoc_size;

    MEMX_LOG(MEMX_LOG_MPU_OFMAP_FLOW, "flow[%d] ofmap hpoc info: \n", flow_id);
    MEMX_LOG(MEMX_LOG_MPU_OFMAP_FLOW, " - channel_number: %d\n", ofmap_info->CascadePlus.fmap.channel_number);
    MEMX_LOG(MEMX_LOG_MPU_OFMAP_FLOW, " - hpoc_dummy_channel_number: %d\n", ofmap_info->CascadePlus.hpoc_dummy_channel_number);
    MEMX_LOG(MEMX_LOG_MPU_OFMAP_FLOW, " - fmap_frame_size: %d\n", ofmap_info->CascadePlus.fmap.fmap_frame_size);
    MEMX_LOG(MEMX_LOG_MPU_OFMAP_FLOW, " - user_frame_size: %d\n", ofmap_info->CascadePlus.fmap.user_frame_size);
    return status;
}

/**
 * @brief Runtime output feature map ring-buffer update.
 *
 * @param mpu                 MPU context
 * @param flow_id             target flow ID
 *
 * @return 0 on success, otherwise error code
 */
memx_status _memx_cascade_plus_ofmap_ringbuffer_update(MemxMpu *mpu, uint8_t flow_id)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->context) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (flow_id >= _CASCADE_PLUS_MPU_OFMAP_FLOW_NUMBER) { return MEMX_STATUS_MPU_INVALID_FLOW_ID; }
    MemxCascadePlus *cascade_plus = (MemxCascadePlus *)mpu->context;
    MemxMpuOfmapInfo *ofmap_info = &cascade_plus->ofmap_infos[flow_id];
    if (ofmap_info->CascadePlus.fmap.user_frame_size <= 0) { return MEMX_STATUS_MPU_INVALID_DATALEN; }

    int32_t frame_buffer_size = cascade_plus->ofmap_frame_buffer_size;
    uint8_t *new_data_ring_buffer = (uint8_t *)malloc(ofmap_info->CascadePlus.fmap.fmap_frame_size * sizeof(uint8_t) * frame_buffer_size);
    if (!new_data_ring_buffer) { return MEMX_STATUS_MPU_ALLOCATE_IFMAP_BUFFER_FAIL; }
    memset(new_data_ring_buffer, 0, ofmap_info->CascadePlus.fmap.fmap_frame_size * sizeof(uint8_t) * frame_buffer_size);

    MemxMpuFmapRingBuffer *ofmap_ringbuffer = &cascade_plus->ofmap_ringbuffers[flow_id];

    // update new ring-buffer size, but do not enable flow immediately
    ofmap_ringbuffer->CascadePlus.max_num_of_frame = frame_buffer_size;
    ofmap_ringbuffer->CascadePlus.size_of_one_frame = ofmap_info->CascadePlus.fmap.fmap_frame_size;
    ofmap_ringbuffer->CascadePlus.entry_r = 0;
    ofmap_ringbuffer->CascadePlus.entry_w = 0;
    ofmap_ringbuffer->CascadePlus.transferred_size = 0;
    ofmap_ringbuffer->CascadePlus.remain_buf_cnt = 0;
    ofmap_ringbuffer->CascadePlus.read_buf_idx = 0;

    // re-allocate buffer memory space
    free(ofmap_ringbuffer->CascadePlus.data);
    ofmap_ringbuffer->CascadePlus.data = NULL;
    ofmap_ringbuffer->CascadePlus.data = new_data_ring_buffer;
    return MEMX_STATUS_OK;
}

/***************************************************************************//**
 * implementation
 ******************************************************************************/
MemxMpu* memx_cascade_plus_create(MemxMpu* mpu, uint8_t mpu_group_id)
{
    MemxCascadePlus* cascade_plus = NULL;
    if (mpu == NULL) {
        goto end;
    }

    cascade_plus = (MemxCascadePlus *)malloc(sizeof(MemxCascadePlus));
    if (cascade_plus == NULL) {
        printf("%s: create cascade_plus fail\n", __FUNCTION__);
        goto fail;
    }

    memset(cascade_plus, 0, sizeof(MemxCascadePlus));

    // create input feature map ring-buffer
    for (int32_t flow_id = 0; flow_id <_CASCADE_PLUS_MPU_IFMAP_FLOW_NUMBER; flow_id++) {
        cascade_plus->ifmap_infos[flow_id].valid = 0;
        cascade_plus->ifmap_infos[flow_id].range_convert_scale = 0;
        cascade_plus->ifmap_infos[flow_id].range_convert_shift = 0;

        cascade_plus->ifmap_ringbuffers[flow_id].CascadePlus.valid = 0;
        cascade_plus->ifmap_ringbuffers[flow_id].CascadePlus.data = NULL;
        cascade_plus->ifmap_ringbuffers[flow_id].CascadePlus.remain_buf_cnt = 0;
        cascade_plus->ifmap_ringbuffers[flow_id].CascadePlus.buf_status = NULL;
        cascade_plus->ifmap_ringbuffers[flow_id].CascadePlus.read_buf_idx = 0;
        if (platform_cond_init(&cascade_plus->ifmap_ringbuffers[flow_id].CascadePlus.pointer_changed, NULL)) {
            printf("%s: init ifmap_ringbuffers[%d].pointer_changed condition variable fail\n", __FUNCTION__, flow_id);
            goto fail;
        }
        if (platform_mutex_create(&cascade_plus->ifmap_ringbuffers[flow_id].CascadePlus.guard, NULL)) {
            printf("%s: create ifmap_ringbuffers[%d].guard mutex fail\n", __FUNCTION__, flow_id);
            goto fail;
        }
        if (platform_mutex_create(&cascade_plus->ifmap_ringbuffers[flow_id].CascadePlus.read_guard, NULL)) {
            printf("%s: create ifmap_ringbuffers[%d].read_guard mutex fail\n", __FUNCTION__, flow_id);
            goto fail;
        }
        if (platform_mutex_create(&cascade_plus->ifmap_ringbuffers[flow_id].CascadePlus.write_guard, NULL)) {
            printf("%s: create ifmap_ringbuffers[%d].write_guard mutex fail\n", __FUNCTION__, flow_id);
            goto fail;
        }
    }

    // create output feature map ring-buffer
    for (int32_t flow_id = 0; flow_id < _CASCADE_PLUS_MPU_OFMAP_FLOW_NUMBER; flow_id++) {
        cascade_plus->ofmap_infos[flow_id].CascadePlus.valid = 0;
        cascade_plus->ofmap_infos[flow_id].CascadePlus.hpoc_enabled = 0;
        cascade_plus->ofmap_infos[flow_id].CascadePlus.hpoc_dummy_channel_number = 0;
        cascade_plus->ofmap_infos[flow_id].CascadePlus.hpoc_dummy_channel_indexes = NULL;
        cascade_plus->ofmap_infos[flow_id].CascadePlus.hpoc_dummy_channel_array_size = 0;

        cascade_plus->ofmap_ringbuffers[flow_id].CascadePlus.valid = 0;
        cascade_plus->ofmap_ringbuffers[flow_id].CascadePlus.data = NULL;
        cascade_plus->ofmap_ringbuffers[flow_id].CascadePlus.remain_buf_cnt = 0;
        cascade_plus->ofmap_ringbuffers[flow_id].CascadePlus.buf_status = NULL;
        cascade_plus->ofmap_ringbuffers[flow_id].CascadePlus.read_buf_idx = 0;
        if (platform_cond_init(&cascade_plus->ofmap_ringbuffers[flow_id].CascadePlus.pointer_changed, NULL)) {
            printf("%s: init ofmap_ringbuffers[%d].pointer_changed condition variable fail\n", __FUNCTION__, flow_id);
            goto fail;
        }
        if (platform_mutex_create(&cascade_plus->ofmap_ringbuffers[flow_id].CascadePlus.guard, NULL)) {
            printf("%s: create ofmap_ringbuffers[%d].guard mutex fail\n", __FUNCTION__, flow_id);
            goto fail;
        }
        if (platform_mutex_create(&cascade_plus->ofmap_ringbuffers[flow_id].CascadePlus.read_guard, NULL)) {
            printf("%s: create ofmap_ringbuffers[%d].read_guard mutex fail\n", __FUNCTION__, flow_id);
            goto fail;
        }
        if (platform_mutex_create(&cascade_plus->ofmap_ringbuffers[flow_id].CascadePlus.write_guard, NULL)) {
            printf("%s: create ofmap_ringbuffers[%d].write_guard mutex fail\n", __FUNCTION__, flow_id);
            goto fail;
        }
    }
    cascade_plus->ifmap_frame_buffer_size = _CASCADE_PLUS_MPU_DEFAULT_IFMAP_FRAME_BUFFER_SIZE;
    cascade_plus->ofmap_frame_buffer_size = _CASCADE_PLUS_MPU_DEFAULT_OFMAP_FRAME_BUFFER_SIZE;

    // setup callback
    mpu->context = (_memx_mpu_context_t)cascade_plus;
    mpu->destroy = (_memx_mpu_destroy_cb)&memx_cascade_plus_destroy;
    mpu->set_read_abort = (_memx_mpu_set_read_abort_cb)&memx_mpuio_set_read_abort_start;
    mpu->reconfigure = (_memx_mpu_reconfigure_cb)&memx_cascade_plus_reconfigure;
    mpu->operation = (_memx_mpu_operation_cb)&memx_cascade_plus_operation;
    mpu->control_write = NULL;
    mpu->control_read = NULL;
    mpu->stream_write = NULL;
    mpu->stream_read = NULL;
    mpu->stream_ifmap = (_memx_mpu_stream_ifmap_cb)&memx_cascade_plus_stream_ifmap;
    mpu->stream_ofmap = (_memx_mpu_stream_ofmap_cb)&memx_cascade_plus_stream_ofmap;
    mpu->download_model = (_memx_mpu_download_model_cb)&memx_cascade_plus_download_model;
    mpu->download_firmware = (_memx_mpu_download_firmware_cb)&memx_cascade_plus_download_firmware;
    mpu->set_stream_enable = (_memx_mpu_set_stream_enable_cb)&memx_cascade_plus_set_stream_enable;
    mpu->set_stream_disable = (_memx_mpu_set_stream_disable_cb)&memx_cascade_plus_set_stream_disable;
    mpu->set_ifmap_queue_size = (_memx_mpu_set_ifmap_queue_size_cb)&memx_cascade_plus_set_ifmap_queue_size;
    mpu->set_ifmap_size = (_memx_mpu_set_ifmap_size_cb)&memx_cascade_plus_set_ifmap_size;
    mpu->set_ifmap_range_convert = (_memx_mpu_set_ifmap_range_convert_cb)&memx_cascade_plus_set_ifmap_range_convert;
    mpu->set_ofmap_queue_size = (_memx_mpu_set_ofmap_queue_size_cb)&memx_cascade_plus_set_ofmap_queue_size;
    mpu->set_ofmap_size = (_memx_mpu_set_ofmap_size_cb)&memx_cascade_plus_set_ofmap_size;
    mpu->set_ofmap_hpoc = (_memx_mpu_set_ofmap_hpoc_cb)&memx_cascade_plus_set_ofmap_hpoc;
    mpu->update_fmap_size = (_memx_mpu_update_fmap_size_cb)&memx_cascade_plus_update_fmap_size;
    mpu->enqueue_ifmap_buf = (_memx_mpu_enqueue_fmap_buf_cb)&memx_cascade_plus_enqueue_ifmap_buf;
    mpu->enqueue_ofmap_buf = (_memx_mpu_enqueue_fmap_buf_cb)&memx_cascade_plus_enqueue_ofmap_buf;
    mpu->dequeue_ifmap_buf = (_memx_mpu_dequeue_fmap_buf_cb)&memx_cascade_plus_dequeue_ifmap_buf;
    mpu->dequeue_ofmap_buf = (_memx_mpu_dequeue_fmap_buf_cb)&memx_cascade_plus_dequeue_ofmap_buf;
    mpu->max_input_flow_cnt = _CASCADE_PLUS_MPU_IFMAP_FLOW_NUMBER;
    mpu->max_output_flow_cnt = _CASCADE_PLUS_MPU_OFMAP_FLOW_NUMBER;
    mpu->chip_gen = MEMX_MPU_CHIP_GEN_CASCADE_PLUS;
    mpu->mpu_group_id = mpu_group_id;
    mpu->input_mode_flag = 0;

    // start ifmap worker, default on
    cascade_plus->ifmap_worker_info.enable = 1;
    cascade_plus->ifmap_worker_info.mpu = mpu;
    cascade_plus->ifmap_worker_info.state = _mpu_worker_state_pending;
    cascade_plus->ifmap_worker_info.state_next = _mpu_worker_state_pending;
    if (platform_cond_init(&cascade_plus->ifmap_worker_info.state_changed, NULL)) {
        printf("%s: init ifmap_worker_info.state_changed condition variable fail\n", __FUNCTION__);
        goto fail;
    }
    if (platform_mutex_create(&cascade_plus->ifmap_worker_info.guard, NULL)) {
        printf("%s: create ifmap_worker_info.guard mutex fail\n", __FUNCTION__);
        goto fail;
    }
    if (platform_thread_create(&cascade_plus->ifmap_worker_info.worker, NULL, &_memx_cascade_plus_ifmap_worker_dowork, &cascade_plus->ifmap_worker_info)) {
        printf("%s: create ifmap_worker_info.worker task fail\n", __FUNCTION__);
        goto fail;
    }
    // start ofmap worker, default on
    cascade_plus->ofmap_worker_info.enable = 1;
    cascade_plus->ofmap_worker_info.mpu = mpu;
    cascade_plus->ofmap_worker_info.state = _mpu_worker_state_pending;
    cascade_plus->ofmap_worker_info.state_next = _mpu_worker_state_pending;
    if (platform_cond_init(&cascade_plus->ofmap_worker_info.state_changed, NULL)) {
        printf("%s: init ofmap_worker_info.state_changed condition variable fail\n", __FUNCTION__);
        goto fail;
    }
    if (platform_mutex_create(&cascade_plus->ofmap_worker_info.guard, NULL)) {
        printf("%s: create ofmap_worker_info.guard mutex fail\n", __FUNCTION__);
        goto fail;
    }
    if (platform_thread_create(&cascade_plus->ofmap_worker_info.worker, NULL, &_memx_cascade_plus_ofmap_worker_dowork, &cascade_plus->ofmap_worker_info)) {
        printf("%s: create ofmap_worker_info.worker task fail\n", __FUNCTION__);
        goto fail;
    }
#if _WIN32
    SetThreadPriority(&cascade_plus->ofmap_worker_info.worker, THREAD_PRIORITY_ABOVE_NORMAL);
#endif

end:
    return mpu;

fail:
    if (cascade_plus != NULL) {
        free(cascade_plus);
        cascade_plus = NULL;
    }
    memx_mpu_destroy(mpu);
    return NULL;
}

void memx_cascade_plus_destroy(MemxMpu *mpu)
{
    if (!mpu || !mpu->context) { return; }
    MemxCascadePlus *cascade_plus = (MemxCascadePlus *)mpu->context;

    // stop all workers
    cascade_plus->ifmap_worker_info.enable = 0;
    cascade_plus->ofmap_worker_info.enable = 0;

    // signal to wake-up workers and go to exit
    platform_mutex_lock(&cascade_plus->ifmap_worker_info.guard);
    cascade_plus->ifmap_worker_info.blocking = 0;
    platform_mutex_unlock(&cascade_plus->ifmap_worker_info.guard);
    // no need to care about which state since we already set enable to '0'
    _memx_mpu_worker_go_to_state(&cascade_plus->ifmap_worker_info, _mpu_worker_state_pending);
    _memx_mpu_worker_go_to_state(&cascade_plus->ofmap_worker_info, _mpu_worker_state_pending);

    // join all workers
    platform_thread_join(&cascade_plus->ifmap_worker_info.worker, NULL);
    platform_thread_join(&cascade_plus->ofmap_worker_info.worker, NULL);

    // clean-up ifmap ring-buffer data
    for (int32_t flow_idx = 0; flow_idx < _CASCADE_PLUS_MPU_IFMAP_FLOW_NUMBER; ++flow_idx) {
        free(cascade_plus->ifmap_ringbuffers[flow_idx].CascadePlus.data);
        cascade_plus->ifmap_ringbuffers[flow_idx].CascadePlus.data = NULL;
        free(cascade_plus->ifmap_ringbuffers[flow_idx].CascadePlus.buf_status);
        cascade_plus->ifmap_ringbuffers[flow_idx].CascadePlus.buf_status = NULL;
        platform_mutex_destory(&cascade_plus->ifmap_ringbuffers[flow_idx].CascadePlus.guard);
        platform_mutex_destory(&cascade_plus->ifmap_ringbuffers[flow_idx].CascadePlus.read_guard);
        platform_mutex_destory(&cascade_plus->ifmap_ringbuffers[flow_idx].CascadePlus.write_guard);
    }

    // clean-up ofmap ring-buffer data
    for (int32_t flow_idx = 0; flow_idx < _CASCADE_PLUS_MPU_OFMAP_FLOW_NUMBER; ++flow_idx) {
        free(cascade_plus->ofmap_ringbuffers[flow_idx].CascadePlus.data);
        cascade_plus->ofmap_ringbuffers[flow_idx].CascadePlus.data = NULL;
        free(cascade_plus->ofmap_ringbuffers[flow_idx].CascadePlus.buf_status);
        cascade_plus->ofmap_ringbuffers[flow_idx].CascadePlus.buf_status = NULL;
        platform_mutex_destory(&cascade_plus->ofmap_ringbuffers[flow_idx].CascadePlus.guard);
        platform_mutex_destory(&cascade_plus->ofmap_ringbuffers[flow_idx].CascadePlus.read_guard);
        platform_mutex_destory(&cascade_plus->ofmap_ringbuffers[flow_idx].CascadePlus.write_guard);
    }

    // clean-up ofmap hpoc setting
    for (int32_t flow_idx = 0; flow_idx < _CASCADE_PLUS_MPU_OFMAP_FLOW_NUMBER; ++flow_idx) {
        if (cascade_plus->ofmap_infos[flow_idx].CascadePlus.hpoc_dummy_channel_indexes != NULL) {
            free(cascade_plus->ofmap_infos[flow_idx].CascadePlus.hpoc_dummy_channel_indexes);
            cascade_plus->ofmap_infos[flow_idx].CascadePlus.hpoc_dummy_channel_indexes = NULL;
        }
    }

    // clean-up self
    free(cascade_plus);
    cascade_plus = NULL;
}

memx_status memx_cascade_plus_reconfigure(MemxMpu *mpu, int32_t timeout)
{
    unused(mpu);
    unused(timeout);

    // do nothing for now
    // TODO: may need to stop and re-configure IO handle
    return MEMX_STATUS_OK;
}

memx_status memx_cascade_plus_operation(MemxMpu *mpu, int32_t cmd_id, void *data, uint32_t size, int32_t timeout)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }

    // use command ID to lookup operation, forward to MPUIO if command is unrecognized
    // forward all to mpuio for now
    return memx_mpuio_operation(mpu->mpuio, mpu->input_chip_id, cmd_id, data, size, timeout);
}

memx_status memx_cascade_plus_stream_ifmap(MemxMpu *mpu, uint8_t flow_id, void *from_user_ifmap_data_buffer, int32_t delay_time_in_ms)
{
    if (!mpu || !mpu->context) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    MemxCascadePlus *cascade_plus = (MemxCascadePlus *)mpu->context;

    if (flow_id >= _CASCADE_PLUS_MPU_IFMAP_FLOW_NUMBER) { return MEMX_STATUS_MPU_INVALID_FLOW_ID; }

    MemxMpuIfmapInfo *ifmap_info = &cascade_plus->ifmap_infos[flow_id];
    // encode data to be transferred only if background worker is enabled
    if (ifmap_info->valid == 0) { return MEMX_STATUS_MPU_IFMAP_NOT_CONFIG; }


    if (!from_user_ifmap_data_buffer) { return MEMX_STATUS_NULL_POINTER; }
    if (delay_time_in_ms < 0 || delay_time_in_ms >= (INT_MAX / 100)) { return MEMX_STATUS_INVALID_PARAMETER; }
    MemxMpuFmapRingBuffer *ifmap_ringbuffer = &cascade_plus->ifmap_ringbuffers[flow_id]; // internal ring-buffer stores ofmaps with alignement to be transferred

    platform_mutex_lock(&ifmap_ringbuffer->CascadePlus.write_guard);
    // wait until queue has space, use polling here to detect timeout
    // do not use gettimeofdate() to acquire time for it's penalty just too high in embedded system
    int32_t milliSeconds_count = 0;
    while ((((ifmap_ringbuffer->CascadePlus.entry_w + 1) % ifmap_ringbuffer->CascadePlus.max_num_of_frame) == ifmap_ringbuffer->CascadePlus.entry_r)) {  // full
        if ((delay_time_in_ms != 0) &&
            (milliSeconds_count++ > (delay_time_in_ms*100))) {
            platform_mutex_unlock(&ifmap_ringbuffer->CascadePlus.write_guard);
            return MEMX_STATUS_MPU_IFMAP_ENQUEUE_TIMEOUT;
        }
        platform_usleep(10);
    }

    // allocate input feature map buffer
    // special usage for gbf80 to use size after padding
    // using fmap_frame_size because to chip need to 4 bytes alignment
    size_t ring_buffer_data_offset = ifmap_ringbuffer->CascadePlus.entry_w * ifmap_ringbuffer->CascadePlus.size_of_one_frame; // to ring-buffer
    uint8_t *ring_buffer_data_start = ifmap_ringbuffer->CascadePlus.data;

    // copy user data to queue, although data copy has large penalty
    // we still need to copy from user to prevent pointer being released
    // using fmap_frame_size because to user data will not coneten dummy bytes for 4 bytes alignment
    memcpy(ring_buffer_data_start + ring_buffer_data_offset, from_user_ifmap_data_buffer, (int64_t)ifmap_info->fmap.user_frame_size * ifmap_info->fmap.channel_size);

    MEMX_LOG(MEMX_LOG_MPU_IFMAP_FLOW, "flow[%d] push %" PRId64 "data to ifmap queue\n", flow_id, (int64_t)ifmap_info->fmap.user_frame_size * ifmap_info->fmap.channel_size);
    // update ring-buffer pointer by row
    ifmap_ringbuffer->CascadePlus.entry_w = ((ifmap_ringbuffer->CascadePlus.entry_w + 1) == ifmap_ringbuffer->CascadePlus.max_num_of_frame) ? 0 : ifmap_ringbuffer->CascadePlus.entry_w + 1;
    platform_mutex_unlock(&ifmap_ringbuffer->CascadePlus.write_guard);

    // signal background transfer worker new data arrive
    platform_mutex_lock(&cascade_plus->ifmap_worker_info.guard);
    cascade_plus->ifmap_worker_info.blocking = 0;
    platform_cond_signal(&cascade_plus->ifmap_worker_info.state_changed);
    platform_mutex_unlock(&cascade_plus->ifmap_worker_info.guard);
    return MEMX_STATUS_OK;
}

memx_status memx_cascade_plus_stream_ofmap(MemxMpu *mpu, uint8_t flow_id, void *to_user_ofmap_data_buffer, int32_t delay_time_in_ms)
{
    if (!mpu || !mpu->context) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    MemxCascadePlus *cascade_plus = (MemxCascadePlus *)mpu->context;

    if (flow_id >= _CASCADE_PLUS_MPU_OFMAP_FLOW_NUMBER) { return MEMX_STATUS_MPU_INVALID_FLOW_ID; }

    MemxMpuOfmapInfo *ofmap_info = &cascade_plus->ofmap_infos[flow_id];
    // take data from internal buffer only if background worker is enabled
    if (ofmap_info->CascadePlus.valid == 0) { return MEMX_STATUS_MPU_OFMAP_NOT_CONFIG; }

    if (!to_user_ofmap_data_buffer) { return MEMX_STATUS_NULL_POINTER; }
    if (delay_time_in_ms < 0 || delay_time_in_ms >= (INT_MAX / 100)) { return MEMX_STATUS_INVALID_PARAMETER; }
    MemxMpuFmapRingBuffer *ofmap_ringbuffer = &cascade_plus->ofmap_ringbuffers[flow_id]; // internal ring-buffer stores ofmaps with alignement to be transferred

    platform_mutex_lock(&ofmap_ringbuffer->CascadePlus.read_guard);
    // wait until there is something in queue, use polling here to detect timeout
    // do not use gettimeofdate() to acquire time for it's penalty just too high in embedded system
    long long milliSeconds_count = 0;
    while((ofmap_ringbuffer->CascadePlus.entry_w == ofmap_ringbuffer->CascadePlus.entry_r)) {// empty
        if ((delay_time_in_ms != 0) &&
            (milliSeconds_count++ > (delay_time_in_ms*100))) {
            platform_mutex_unlock(&ofmap_ringbuffer->CascadePlus.read_guard);
            return MEMX_STATUS_MPU_OFMAP_DEQUEUE_TIMEOUT;
        }
        platform_usleep(10);
    }
    size_t ring_buffer_data_offset = ofmap_ringbuffer->CascadePlus.entry_r * ofmap_ringbuffer->CascadePlus.size_of_one_frame; // to ring-buffer
    uint8_t *ring_buffer_data_start = ofmap_ringbuffer->CascadePlus.data;
    void *from_chip_ofmap_data = (void*)(ring_buffer_data_start + ring_buffer_data_offset);

    MEMX_LOG(MEMX_LOG_MPU_OFMAP_FLOW, "flow[%d] pop from ofmap queue\n", flow_id);

    platform_mutex_lock(&ofmap_ringbuffer->CascadePlus.write_guard);
    ofmap_ringbuffer->CascadePlus.remain_buf_cnt--;
    platform_mutex_unlock(&ofmap_ringbuffer->CascadePlus.write_guard);

    // copy from queue to user buffer
    // cannot pop internal buffer address to user directly or is there any way to give rvalue in C language?

    // remove HPOC channel if hpoc enabled
    // FIXME: Only support remove hpoc for FP32 for now
    if (ofmap_info->CascadePlus.hpoc_enabled && ofmap_info->CascadePlus.fmap.format == MEMX_MPU_OFMAP_FORMAT_FLOAT32) {
        // record how much row has been process, user and chip fmap channel number is different for hpoc,
        // so we will calcuate real buffer offeset with different channel number for both buffer
        int32_t row_without_ch_offset = 0;
        for (int32_t h = 1; h <= ofmap_info->CascadePlus.fmap.height; ++h) {
            for (int32_t w = 1; w <= ofmap_info->CascadePlus.fmap.width; ++w) {
                for (int32_t z = 1; z <= ofmap_info->CascadePlus.fmap.z; ++z) {
                    int32_t check_dummy_channel_idx = 0;
                    int32_t user_ch_offset = 0;
                    for (int32_t chip_ch_offset = 0; chip_ch_offset < ofmap_info->CascadePlus.hpoc_dummy_channel_number; ++chip_ch_offset) {
                        // skip dummy channel
                        if ((check_dummy_channel_idx < ofmap_info->CascadePlus.hpoc_dummy_channel_array_size) &&
                            (chip_ch_offset == ofmap_info->CascadePlus.hpoc_dummy_channel_indexes[check_dummy_channel_idx])) {
                            check_dummy_channel_idx++;
                            continue;
                        } else {
                            *((uint32_t*)to_user_ofmap_data_buffer + row_without_ch_offset * ofmap_info->CascadePlus.fmap.channel_number + user_ch_offset++) =
                                *((uint32_t*)from_chip_ofmap_data + row_without_ch_offset * ofmap_info->CascadePlus.hpoc_dummy_channel_number + chip_ch_offset);
                        }
                    }/* chip_ch_offset */
                    row_without_ch_offset ++;
                }/* z */
            }/* width */
        }/* height */
    } else {
        // from_chip_ofmap_data might have extra padding for 4 bytes alignment, but it is ok because we hide from user
        memcpy(to_user_ofmap_data_buffer, from_chip_ofmap_data, (uint64_t)(ofmap_info->CascadePlus.fmap.user_frame_size));
    }
    ofmap_ringbuffer->CascadePlus.entry_r = ((ofmap_ringbuffer->CascadePlus.entry_r + 1) == ofmap_ringbuffer->CascadePlus.max_num_of_frame) ? 0 : ofmap_ringbuffer->CascadePlus.entry_r + 1;
    platform_mutex_unlock(&ofmap_ringbuffer->CascadePlus.read_guard);
    return MEMX_STATUS_OK;
}

memx_status memx_cascade_plus_download_firmware(MemxMpu *mpu, const char *file_path)
{
    if (!mpu || !mpu->context || !file_path) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }

    // download model to device first then configure Host driver secondly
    return memx_mpuio_download_firmware(mpu->mpuio, file_path);
}

memx_status memx_cascade_plus_download_model(MemxMpu* mpu, void * pDfpMeta, uint8_t model_idx, int32_t type, int32_t timeout)
{
    if (!mpu || !mpu->context) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    memx_status status = MEMX_STATUS_OK;
    // try to attach mpu group before download model if mpu group not attached
    if ((mpu->mpu_group_id == MEMX_MPU_GROUP_ID_UNATTTACHED) &&
        ((status = memx_mpuio_attach_idle_mpu_group(mpu->mpuio, &mpu->mpu_group_id)) != MEMX_STATUS_OK)) {
        return status;
    }
    mpu->input_chip_id = mpu->mpuio->hw_info.chip.groups[mpu->mpu_group_id].input_chip_id;
    mpu->output_chip_id = mpu->mpuio->hw_info.chip.groups[mpu->mpu_group_id].output_chip_id;

    // download model to device first then configure Host driver secondly
    return memx_mpuio_download_model(mpu->mpuio, mpu->input_chip_id, pDfpMeta, model_idx, type, timeout);
}

memx_status memx_cascade_plus_set_stream_enable(MemxMpu *mpu, int32_t wait, int32_t timeout)
{
    unused(timeout);

    if (!mpu || !mpu->context) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio || !mpu->mpuio->context) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    MemxCascadePlus *cascade_plus = (MemxCascadePlus *)mpu->context;

    // enable all workers
    _memx_mpu_worker_go_to_state(&cascade_plus->ifmap_worker_info, _mpu_worker_state_running);
    _memx_mpu_worker_go_to_state(&cascade_plus->ofmap_worker_info, _mpu_worker_state_running);

    // wait until all worker state to running
    if (wait) {
        _memx_mpu_worker_wait_state_entered(&cascade_plus->ifmap_worker_info, _mpu_worker_state_running);
        _memx_mpu_worker_wait_state_entered(&cascade_plus->ofmap_worker_info, _mpu_worker_state_running);
    }
    return MEMX_STATUS_OK;
}

memx_status memx_cascade_plus_set_stream_disable(MemxMpu *mpu, int32_t wait, int32_t timeout)
{
    unused(timeout);

    if (!mpu || !mpu->context) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    MemxCascadePlus *cascade_plus = (MemxCascadePlus *)mpu->context;

    // block all workers
    // signal to wake-up threads if they are waiting for specific condition
    _memx_mpu_worker_go_to_state(&cascade_plus->ifmap_worker_info, _mpu_worker_state_pending);
    _memx_mpu_worker_go_to_state(&cascade_plus->ofmap_worker_info, _mpu_worker_state_pending);

    // wait until all worker state to pending
    // no need to wait for job workers for they only transfer data between queue and buffer
    if (wait) {
        _memx_mpu_worker_wait_state_entered(&cascade_plus->ifmap_worker_info, _mpu_worker_state_pending);
        _memx_mpu_worker_wait_state_entered(&cascade_plus->ofmap_worker_info, _mpu_worker_state_pending);
    }

    memx_mpuio_detach_mpu_group(mpu->mpuio, mpu->mpu_group_id);
    mpu->mpu_group_id = MEMX_MPU_GROUP_ID_UNATTTACHED;
    return MEMX_STATUS_OK;
}

memx_status memx_cascade_plus_set_ifmap_queue_size(MemxMpu *mpu, int32_t size, int32_t timeout)
{
    unused(timeout);
    if (!mpu || !mpu->context) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (size <= 0 || size > _CASCADE_PLUS_MPU_MAX_IFMAP_FRAME_BUFFER_SIZE) { return MEMX_STATUS_MPU_INVALID_QUEUE_SIZE; }

    MemxCascadePlus *cascade_plus = (MemxCascadePlus*)mpu->context; // get context here
    cascade_plus->ifmap_frame_buffer_size = size;

    return MEMX_STATUS_OK;
}

memx_status memx_cascade_plus_set_ifmap_size(MemxMpu *mpu, uint8_t flow_id, int32_t height, int32_t width, int32_t z, int32_t channel_number, int32_t format, int32_t timeout)
{
    if (!mpu || !mpu->context) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    MemxCascadePlus *cascade_plus = (MemxCascadePlus*)mpu->context; // get context here
    if (flow_id >= _CASCADE_PLUS_MPU_IFMAP_FLOW_NUMBER) { return MEMX_STATUS_MPU_INVALID_FLOW_ID; }
    MemxMpuIfmapInfo *ifmap_info = &cascade_plus->ifmap_infos[flow_id]; // pick the input feature map info structure based on given flow id
    MemxMpuFmapRingBuffer *ifmap_ringbuffer = &cascade_plus->ifmap_ringbuffers[flow_id]; // internal ring-buffer stores ofmaps with alignement to be transferred
    if (height <= 0 || width <= 0  || z <= 0 || channel_number <= 0) { return MEMX_STATUS_MPU_INVALID_FMAP_SIZE; }
    if (format < MEMX_MPU_IFMAP_FORMAT_GBF80 || format >= MEMX_MPU_IFMAP_FORMAT_RESERVED) { return MEMX_STATUS_MPU_INVALID_FMAP_FORMAT; }

    // TODO: check there still data in queue and is not yet finished

    // compare to original stored information to see if configuration changed is required
    int32_t is_any_ifmap_info_changed = ((ifmap_info->fmap.height != height) ||
                                     (ifmap_info->fmap.width != width) ||
                                     (ifmap_info->fmap.z != z) ||
                                     (ifmap_info->fmap.channel_number != channel_number) ||
                                     (ifmap_info->fmap.format != format)) ? 1: 0;
    if (!is_any_ifmap_info_changed) { return MEMX_STATUS_OK; }

    // stop background worker first if configuration is to be changed
    ifmap_info->valid = 0;
    ifmap_ringbuffer->CascadePlus.valid = 0; // pause flow data
    memx_status status = _memx_cacasde_plus_ifmap_info_update(mpu, flow_id, height, width, z, channel_number, format);
    if (memx_status_error(status)) { return  status; }

    // -----------------------------------
    // forward setting to low level driver
    // -----------------------------------
    status = memx_mpuio_set_ifmap_size(mpu->mpuio, mpu->input_chip_id, flow_id, ifmap_info->fmap.height,
            ifmap_info->fmap.width, ifmap_info->fmap.z, ifmap_info->fmap.fmap_channel_number, ifmap_info->fmap.format, timeout);
    if (memx_status_error(status)) { return  status; }

    // store new input feature map size configuration
    // enable flow background worker flow if size is none zero
    status = _memx_cascade_plus_ifmap_ringbuffer_update(mpu, flow_id);
    if (memx_status_error(status)) { return  status; }

    ifmap_ringbuffer->CascadePlus.valid = 1; // resume flow data
    ifmap_info->valid = 1;

    return MEMX_STATUS_OK;
}

memx_status memx_cascade_plus_set_ifmap_range_convert(MemxMpu *mpu, uint8_t flow_id, float shift, float scale, int32_t timeout)
{
    unused(timeout);

    if (!mpu || !mpu->context) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    MemxCascadePlus *cascade_plus = (MemxCascadePlus*)mpu->context; // get context here
    if (flow_id >= _CASCADE_PLUS_MPU_IFMAP_FLOW_NUMBER) { return MEMX_STATUS_MPU_INVALID_FLOW_ID; }
    MemxMpuIfmapInfo *ifmap_info = &cascade_plus->ifmap_infos[flow_id]; // pick the input feature map info structure based on given flow id
    if (ifmap_info->valid == 0) { return MEMX_STATUS_MPU_IFMAP_NOT_CONFIG; }
    MemxMpuFmapRingBuffer *ifmap_ringbuffer = &cascade_plus->ifmap_ringbuffers[flow_id]; // internal ring-buffer stores ofmaps with alignement to be transferred

    // stop background worker before input feature map shape changed
    ifmap_info->valid = 0;
    ifmap_ringbuffer->CascadePlus.valid = 0;
    memx_status status = _memx_cascade_plus_ifmap_info_update_range_convert(mpu, flow_id, shift, scale);
    if (memx_status_error(status)) { return  status; }

    // -----------------------------------
    // forward setting to low level driver
    // -----------------------------------
    status = memx_mpuio_set_ifmap_size(mpu->mpuio, mpu->input_chip_id, flow_id, ifmap_info->fmap.height,
            ifmap_info->fmap.width, ifmap_info->fmap.z, ifmap_info->fmap.fmap_channel_number, ifmap_info->fmap.format, timeout);
    if (memx_status_error(status)) { return  status; }

    // start background worker with new setting
    status = _memx_cascade_plus_ifmap_ringbuffer_update(mpu, flow_id);
    if (memx_status_error(status)) { return  status; }

    ifmap_ringbuffer->CascadePlus.valid = 1;
    ifmap_info->valid = 1;

    return MEMX_STATUS_OK;
}

memx_status memx_cascade_plus_set_ofmap_queue_size(MemxMpu *mpu, int32_t size, int32_t timeout)
{
    unused(timeout);
    if (!mpu || !mpu->context) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (size <= 0 || size > _CASCADE_PLUS_MPU_MAX_OFMAP_FRAME_BUFFER_SIZE) { return MEMX_STATUS_MPU_INVALID_QUEUE_SIZE; }

    MemxCascadePlus *cascade_plus = (MemxCascadePlus*)mpu->context; // get context here
    cascade_plus->ofmap_frame_buffer_size = size;

    return MEMX_STATUS_OK;
}

memx_status memx_cascade_plus_set_ofmap_size(MemxMpu *mpu, uint8_t flow_id, int32_t height, int32_t width, int32_t z, int32_t channel_number, int32_t format, int32_t timeout)
{
    if (!mpu || !mpu->context) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    MemxCascadePlus *cascade_plus = (MemxCascadePlus*)mpu->context; // get context here
    if (flow_id >= _CASCADE_PLUS_MPU_OFMAP_FLOW_NUMBER) { return MEMX_STATUS_MPU_INVALID_FLOW_ID; }
    MemxMpuOfmapInfo *ofmap_info = &cascade_plus->ofmap_infos[flow_id]; // pick the output feature map info structure based on given flow id
    MemxMpuFmapRingBuffer *ofmap_ringbuffer = &cascade_plus->ofmap_ringbuffers[flow_id]; // internal ring-buffer stores ofmaps with alignement to be transferred
    if (height <= 0 || width <= 0 || z <= 0 || channel_number <= 0) { return MEMX_STATUS_MPU_INVALID_FMAP_SIZE; }
    if (format < MEMX_MPU_OFMAP_FORMAT_GBF80 || format >= MEMX_MPU_OFMAP_FORMAT_RESERVED) { return MEMX_STATUS_MPU_INVALID_FMAP_FORMAT; }


    // TODO: check there still data in queue and is not yet finished

    // compare to original stored information to see if configuration changed is required
    int32_t is_any_ofmap_info_changed = ((ofmap_info->CascadePlus.fmap.height != height) ||
                                     (ofmap_info->CascadePlus.fmap.width != width) ||
                                     (ofmap_info->CascadePlus.fmap.z != z) ||
                                     (ofmap_info->CascadePlus.fmap.channel_number != channel_number) ||
                                     (ofmap_info->CascadePlus.fmap.format != format)) ? 1: 0;

    // backend mpuio and frontend mpu model_id mismatch or fmap info change, do update fmap size and backend mpuio model_id
    if ((mpu->model_id != mpu->mpuio->model_id) ||
        (is_any_ofmap_info_changed == 1)) {
        mpu->mpuio->model_id = mpu->model_id;
        // -----------------------------------
        // forward setting to low level driver
        // -----------------------------------
        memx_status status = memx_mpuio_set_ofmap_size(mpu->mpuio, mpu->output_chip_id, flow_id, height, width, z, channel_number, format, timeout);
        if (memx_status_error(status)) { return  status; }
    }

    // skip non-changed case
    if (!is_any_ofmap_info_changed) { return MEMX_STATUS_OK; }

    // stop background worker first if configuration is to be changed
    ofmap_info->CascadePlus.valid = 0;
    ofmap_ringbuffer->CascadePlus.valid = 0; // pause flow data
    memx_status status = _memx_cacasde_plus_ofmap_info_update(mpu, flow_id, height, width, z, channel_number, format);
    if (memx_status_error(status)) { return  status; }

    // store new output feature map size configuration
    // enable flow background worker flow if size is none zero
    status = _memx_cascade_plus_ofmap_ringbuffer_update(mpu, flow_id);
    if (memx_status_error(status)) { return  status; }

    ofmap_ringbuffer->CascadePlus.valid = 1; // resume flow data
    ofmap_info->CascadePlus.valid = 1;

    return MEMX_STATUS_OK;
}

memx_status memx_cascade_plus_set_ofmap_hpoc(MemxMpu *mpu, uint8_t flow_id, int32_t hpoc_size, int32_t *hpoc_indexes, int32_t timeout)
{
    unused(timeout);
    if (!mpu || !mpu->context) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    MemxCascadePlus *cascade_plus = (MemxCascadePlus *)mpu->context; // get context here
    if (flow_id >= _CASCADE_PLUS_MPU_OFMAP_FLOW_NUMBER) { return MEMX_STATUS_MPU_INVALID_FLOW_ID; }
    MemxMpuOfmapInfo *ofmap_info = &cascade_plus->ofmap_infos[flow_id]; // pick the output feature map info structure based on given flow id
    if (ofmap_info->CascadePlus.valid == 0) {return MEMX_STATUS_MPU_OFMAP_NOT_CONFIG; }
    MemxMpuFmapRingBuffer *ofmap_ringbuffer = &cascade_plus->ofmap_ringbuffers[flow_id]; // internal ring-buffer stores ofmaps with alignement to be transferred
    if (hpoc_size <= 0 || !hpoc_indexes) { return MEMX_STATUS_MPU_INVALID_PARAMETER; }

    // stop background worker before output feature map shape changed
    ofmap_info->CascadePlus.valid = 0;
    ofmap_ringbuffer->CascadePlus.valid = 0;
    memx_status status = _memx_cascade_plus_ofmap_info_update_hpoc(mpu, flow_id, hpoc_size, hpoc_indexes);
    if (memx_status_error(status)) { return  status; }

    status = memx_mpuio_set_ofmap_size(mpu->mpuio, mpu->output_chip_id, flow_id, ofmap_info->CascadePlus.fmap.height,
            ofmap_info->CascadePlus.fmap.width, ofmap_info->CascadePlus.fmap.z, ofmap_info->CascadePlus.fmap.fmap_channel_number, MEMX_MPU_OFMAP_FORMAT_GBF80, timeout);
    if (memx_status_error(status)) { return  status; }

    // start background worker with new setting
    status = _memx_cascade_plus_ofmap_ringbuffer_update(mpu, flow_id);
    if (memx_status_error(status)) { return  status; }

    ofmap_ringbuffer->CascadePlus.valid = 1;
    ofmap_info->CascadePlus.valid = 1;

    return MEMX_STATUS_OK;
}

memx_status memx_cascade_plus_update_fmap_size(MemxMpu* mpu, uint8_t in_flow_count, uint8_t out_flow_count, int32_t timeout)
{
    if (!mpu || !mpu->context) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (in_flow_count == 0 || out_flow_count == 0) { return MEMX_STATUS_MPU_INVALID_FLOW_ID; }
    // -----------------------------------
    // forward setting to low level driver
    // -----------------------------------
    return memx_mpuio_update_fmap_size(mpu->mpuio, mpu->output_chip_id, in_flow_count, out_flow_count, timeout);
}

memx_status memx_cascade_plus_enqueue_ifmap_buf(MemxMpu* mpu, uint8_t flow_id, memx_fmap_buf_t* fmap_buf, int32_t timeout)
{
    if (!mpu || !mpu->context) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    MemxCascadePlus *cascade_plus = (MemxCascadePlus *)mpu->context;
    if (flow_id >= _CASCADE_PLUS_MPU_IFMAP_FLOW_NUMBER) { return MEMX_STATUS_MPU_INVALID_FLOW_ID; }
    MemxMpuIfmapInfo *ifmap_info = &cascade_plus->ifmap_infos[flow_id];
    if (ifmap_info->valid == 0) { return MEMX_STATUS_MPU_IFMAP_NOT_CONFIG; }
    if (timeout < 0) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!fmap_buf || fmap_buf->size == 0 || !fmap_buf->data) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    MemxMpuFmapRingBuffer *ifmap_ringbuffer = &cascade_plus->ifmap_ringbuffers[flow_id]; // internal ring-buffer stores ofmaps with alignement to be transferred
    if (fmap_buf->idx >= ifmap_ringbuffer->CascadePlus.max_num_of_frame) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    memx_status status = MEMX_STATUS_OK;
    MEMX_LOG(MEMX_LOG_MPU_IFMAP_FLOW, "flow[%d] enqueue buf[%ld] to ifmap queue\n", flow_id, fmap_buf->idx);

    // try to transfer as much data as possible at once
    size_t expected_total_transfer_size = fmap_buf->size;
    size_t actually_total_transfer_size = 0;
    uint8_t *data_buffer_start = (uint8_t *)(fmap_buf->data);

    // once transfer has began, let it finish or be stopped by user
    while (actually_total_transfer_size < expected_total_transfer_size) {
        data_buffer_start += actually_total_transfer_size;
        int32_t transferred_length = 0;
        int32_t data_length = (int32_t)(expected_total_transfer_size - actually_total_transfer_size);
        // for windows driver, it may sleep for Platform_Write API
        status = memx_mpuio_stream_write(mpu->mpuio, mpu->input_chip_id, flow_id, data_buffer_start, data_length, &transferred_length, timeout);
        if(memx_status_error(status)){
            printf("memx_mpuio_stream_write error %d\r\n", status);
            break;
        }
        actually_total_transfer_size += transferred_length;
        // TODO: delay a bit if interface driver did not take data
        if (transferred_length == 0) {
            platform_usleep(10);
        }
    }

    platform_mutex_lock(&ifmap_ringbuffer->CascadePlus.write_guard);
    ifmap_ringbuffer->CascadePlus.buf_status[fmap_buf->idx] = _mpu_buffer_status_empty;
    ifmap_ringbuffer->CascadePlus.remain_buf_cnt++;
    platform_mutex_unlock(&ifmap_ringbuffer->CascadePlus.write_guard);
    return status;
}

memx_status memx_cascade_plus_enqueue_ofmap_buf(MemxMpu* mpu, uint8_t flow_id, memx_fmap_buf_t* fmap_buf, int32_t timeout)
{
    if (!mpu || !mpu->context) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    MemxCascadePlus *cascade_plus = (MemxCascadePlus *)mpu->context;
    if (flow_id >= _CASCADE_PLUS_MPU_OFMAP_FLOW_NUMBER) { return MEMX_STATUS_MPU_INVALID_FLOW_ID; }
    MemxMpuOfmapInfo *ofmap_info = &cascade_plus->ofmap_infos[flow_id];
    if (ofmap_info->CascadePlus.valid == 0) { return MEMX_STATUS_MPU_OFMAP_NOT_CONFIG; }
    if (timeout < 0) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!fmap_buf || fmap_buf->size == 0 || !fmap_buf->data) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    MemxMpuFmapRingBuffer *ofmap_ringbuffer = &cascade_plus->ofmap_ringbuffers[flow_id]; // internal ring-buffer stores ofmaps with alignement to be transferred
    if (fmap_buf->idx >= ofmap_ringbuffer->CascadePlus.max_num_of_frame) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }

    platform_mutex_lock(&ofmap_ringbuffer->CascadePlus.read_guard);
    ofmap_ringbuffer->CascadePlus.entry_r = ((ofmap_ringbuffer->CascadePlus.entry_r + 1) == ofmap_ringbuffer->CascadePlus.max_num_of_frame) ? 0 : ofmap_ringbuffer->CascadePlus.entry_r + 1;
    MEMX_LOG(MEMX_LOG_MPU_OFMAP_FLOW, "flow[%d] enqueue buf[%ld] to ofmap queue\n", flow_id, fmap_buf->idx);
    platform_mutex_unlock(&ofmap_ringbuffer->CascadePlus.read_guard);

    return MEMX_STATUS_OK;
}

memx_status memx_cascade_plus_dequeue_ifmap_buf(MemxMpu* mpu, uint8_t flow_id, memx_fmap_buf_t* fmap_buf, int32_t timeout)
{
    if (!mpu || !mpu->context) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    MemxCascadePlus *cascade_plus = (MemxCascadePlus *)mpu->context;
    if (flow_id >= _CASCADE_PLUS_MPU_IFMAP_FLOW_NUMBER) { return MEMX_STATUS_MPU_INVALID_FLOW_ID; }
    MemxMpuIfmapInfo *ifmap_info = &cascade_plus->ifmap_infos[flow_id];
    if (ifmap_info->valid == 0) { return MEMX_STATUS_MPU_IFMAP_NOT_CONFIG; }
    if (timeout < 0) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!fmap_buf) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    MemxMpuFmapRingBuffer *ifmap_ringbuffer = &cascade_plus->ifmap_ringbuffers[flow_id]; // internal ring-buffer stores ofmaps with alignement to be transferred

    int32_t milliSeconds_count = 0;
    platform_mutex_lock(&ifmap_ringbuffer->CascadePlus.write_guard);
    while (ifmap_ringbuffer->CascadePlus.remain_buf_cnt == 0) {
        if ((timeout != 0) &&
            (milliSeconds_count++ > (timeout*100))) {
            platform_mutex_unlock(&ifmap_ringbuffer->CascadePlus.write_guard);
            return MEMX_STATUS_MPU_IFMAP_BUF_DEQUEUE_TIMEOUT;
        }
        platform_usleep(10);
    }

    // find empty buffer
    size_t next_empty_buf_idx = SIZE_MAX;
    for (size_t i = 0; i < ifmap_ringbuffer->CascadePlus.max_num_of_frame; i++) {
        if (ifmap_ringbuffer->CascadePlus.buf_status[i] == _mpu_buffer_status_empty) {
            next_empty_buf_idx = i;
            break;
        }
    }

    if (next_empty_buf_idx == SIZE_MAX) {
        printf("memx_cascade_plus_dequeue_ifmap_buf: All buffers are full\n");
        platform_mutex_unlock(&ifmap_ringbuffer->CascadePlus.write_guard);
        return MEMX_STATUS_MPU_IFMAP_BUF_DEQUEUE_ERROR;
    }

    ifmap_ringbuffer->CascadePlus.buf_status[next_empty_buf_idx] = _mpu_buffer_status_full;
    ifmap_ringbuffer->CascadePlus.remain_buf_cnt--;

    size_t ring_buffer_data_offset = next_empty_buf_idx * ifmap_ringbuffer->CascadePlus.size_of_one_frame;
    fmap_buf->size = (uint32_t)ifmap_ringbuffer->CascadePlus.size_of_one_frame;
    fmap_buf->data = (void*)(ifmap_ringbuffer->CascadePlus.data + ring_buffer_data_offset);
    fmap_buf->idx = next_empty_buf_idx;
    platform_mutex_unlock(&ifmap_ringbuffer->CascadePlus.write_guard);

    MEMX_LOG(MEMX_LOG_MPU_IFMAP_FLOW, "flow[%d] dequeue buf[%ld] from ifmap queue\n", flow_id, next_empty_buf_idx);

    return MEMX_STATUS_OK;
}

memx_status memx_cascade_plus_dequeue_ofmap_buf(MemxMpu* mpu, uint8_t flow_id, memx_fmap_buf_t* fmap_buf, int32_t timeout)
{
    if (!mpu || !mpu->context) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    MemxCascadePlus *cascade_plus = (MemxCascadePlus *)mpu->context;
    if (flow_id >= _CASCADE_PLUS_MPU_OFMAP_FLOW_NUMBER) { return MEMX_STATUS_MPU_INVALID_FLOW_ID; }
    MemxMpuOfmapInfo *ofmap_info = &cascade_plus->ofmap_infos[flow_id];
    if (ofmap_info->CascadePlus.valid == 0) { return MEMX_STATUS_MPU_OFMAP_NOT_CONFIG; }
    if (timeout < 0) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!fmap_buf) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    MemxMpuFmapRingBuffer *ofmap_ringbuffer = &cascade_plus->ofmap_ringbuffers[flow_id]; // internal ring-buffer stores ofmaps with alignement to be transferred

    int32_t milliSeconds_count = 0;
    platform_mutex_lock(&ofmap_ringbuffer->CascadePlus.read_guard);
    while(ofmap_ringbuffer->CascadePlus.remain_buf_cnt == 0) {
        if ((timeout != 0) &&
            (milliSeconds_count++ > (timeout*100))) {
            platform_mutex_unlock(&ofmap_ringbuffer->CascadePlus.read_guard);
            return MEMX_STATUS_MPU_OFMAP_BUF_DEQUEUE_TIMEOUT;
        }
        platform_usleep(10);
    }

    platform_mutex_lock(&ofmap_ringbuffer->CascadePlus.write_guard);
    ofmap_ringbuffer->CascadePlus.remain_buf_cnt--;
    platform_mutex_unlock(&ofmap_ringbuffer->CascadePlus.write_guard);

    size_t next_read_buf_idx = ofmap_ringbuffer->CascadePlus.read_buf_idx;
    size_t ring_buffer_data_offset = next_read_buf_idx * ofmap_ringbuffer->CascadePlus.size_of_one_frame;
    fmap_buf->size = ofmap_ringbuffer->CascadePlus.size_of_one_frame;
    fmap_buf->data = (void*)(ofmap_ringbuffer->CascadePlus.data + ring_buffer_data_offset);
    fmap_buf->idx = next_read_buf_idx;
    ofmap_ringbuffer->CascadePlus.read_buf_idx = ((ofmap_ringbuffer->CascadePlus.read_buf_idx + 1) == ofmap_ringbuffer->CascadePlus.max_num_of_frame) ? 0 : ofmap_ringbuffer->CascadePlus.read_buf_idx + 1;

    platform_mutex_unlock(&ofmap_ringbuffer->CascadePlus.read_guard);
    MEMX_LOG(MEMX_LOG_MPU_OFMAP_FLOW, "flow[%d] dequeue buf[%ld] from ofmap queue\n", flow_id, next_read_buf_idx);

    return MEMX_STATUS_OK;
}
