/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#include "memx_mpu.h"
#include "memx_mpu_comm.h"
#include "memx_mpuio_comm.h"
#include "memx_cascadeplus.h"
#include "memx.h"
#include <stdlib.h>


/***************************************************************************//**
 * implementation
 ******************************************************************************/
MemxMpu *memx_mpu_create(MemxMpuIo* mpuio, uint8_t chip_gen)
{
    if (!mpuio) { printf("%s: mpuio is NULL\n", __FUNCTION__); return NULL; }
    // create new context
    MemxMpu *mpu = (MemxMpu *)malloc(sizeof(MemxMpu));
    if (!mpu) { printf("%s: mpu malloc fail\n", __FUNCTION__); return NULL; }
    memset(mpu, 0, sizeof(MemxMpu));

    // setup variable
    mpu->mpuio = mpuio;
    mpu->ifmaps = memx_list_create();
    if (!mpu->ifmaps) {
        free(mpu);
        mpu = NULL;
        printf("%s: create ifmaps list fail\n", __FUNCTION__);
        return NULL;
    }
    mpu->ofmaps = memx_list_create();
    if (!mpu->ofmaps) {
        memx_list_destroy(mpu->ifmaps);
        free(mpu);
        mpu = NULL;
        printf("%s: create ofmaps list fail\n", __FUNCTION__);
        return NULL;
    }

    // mpu will attach to idle mpu group when download dfp, create with unattached mpu group now
    switch(chip_gen){
        case MEMX_MPU_CHIP_GEN_CASCADE_PLUS:
            mpu = memx_cascade_plus_create(mpu, MEMX_MPU_GROUP_ID_UNATTTACHED);
            break;
        default:
            memx_mpu_destroy(mpu);
            break;
    }

    return mpu;
}

void memx_mpu_destroy(MemxMpu *mpu)
{
    if (!mpu) { return; }
    // clean-up child context
    if (mpu->destroy) { mpu->destroy(mpu); }

    // clean-up self context
    while (memx_list_count(mpu->ifmaps)) {
        MemxMpuCommIfmapInfo *ifmap = (MemxMpuCommIfmapInfo *)memx_list_pop(mpu->ifmaps);
        free(ifmap);
        ifmap = NULL;
    }
    memx_list_destroy(mpu->ifmaps);

    while (memx_list_count(mpu->ofmaps)) {
        MemxMpuCommOfmapInfo *ofmap = (MemxMpuCommOfmapInfo *)memx_list_pop(mpu->ofmaps);
        if (ofmap->hpoc_indexes) { free(ofmap->hpoc_indexes); ofmap->hpoc_indexes = NULL; }
        free(ofmap);
        ofmap = NULL;
    }
    memx_list_destroy(mpu->ofmaps);

    mpu->mpuio = NULL; // de-reference IO handle only, do not release it for other MPU might still need it
    free(mpu); // kill self context
    mpu = NULL;
}

memx_status memx_mpu_reconfigure(MemxMpu* mpu, int timeout)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!mpu->reconfigure) { return MEMX_STATUS_MPU_NOT_IMPLEMENTED; }
    // child process
    return mpu->reconfigure(mpu, timeout);
}

memx_status memx_mpu_operation(MemxMpu* mpu, uint32_t cmd_id, void* data, uint32_t size, int32_t timeout)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!mpu->operation) { return MEMX_STATUS_MPU_NOT_IMPLEMENTED; }

    if (cmd_id >= MEMX_CMD_MAX) { return MEMX_STATUS_MPU_INVALID_PARAMETER; }
    // child process
    return mpu->operation(mpu, cmd_id, data, size, timeout);
}

memx_status memx_mpu_control_write(MemxMpu* mpu, uint32_t address, uint8_t* data, int length, int* transferred, int increment, int timeout)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!mpu->control_write) { return MEMX_STATUS_MPU_NOT_IMPLEMENTED; }
    // child process
    return mpu->control_write(mpu, address, data, length, transferred, increment, timeout);
}

memx_status memx_mpu_control_read(MemxMpu* mpu, uint32_t address, uint8_t* data, int length, int* transferred, int increment, int timeout)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!mpu->control_read) { return MEMX_STATUS_MPU_NOT_IMPLEMENTED; }
    // child process
    return mpu->control_read(mpu, address, data, length, transferred, increment, timeout);
}

memx_status memx_mpu_stream_write(MemxMpu* mpu, uint8_t flow_id, uint8_t* data, int length, int* transferred, int timeout)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!mpu->stream_write) { return MEMX_STATUS_MPU_NOT_IMPLEMENTED; }
    // child process
    return mpu->stream_write(mpu, flow_id, data, length, transferred, timeout);
}

memx_status memx_mpu_stream_read(MemxMpu* mpu, uint8_t flow_id, uint8_t* data, int length, int* transferred, int timeout)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!mpu->stream_read) { return MEMX_STATUS_MPU_NOT_IMPLEMENTED; }
    // child process
    return mpu->stream_read(mpu, flow_id, data, length, transferred, timeout);
}

memx_status memx_mpu_stream_ifmap(MemxMpu* mpu, uint8_t flow_id, void* from_user_ifmap_data_buffer, int delay_time_in_ms)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!mpu->stream_ifmap) { return MEMX_STATUS_MPU_NOT_IMPLEMENTED; }

    // child process
    return mpu->stream_ifmap(mpu, flow_id, from_user_ifmap_data_buffer, delay_time_in_ms);
}

memx_status memx_mpu_stream_ofmap(MemxMpu* mpu, uint8_t flow_id, void* to_user_ofmap_data_buffer, int delay_time_in_ms)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!mpu->stream_ofmap) { return MEMX_STATUS_MPU_NOT_IMPLEMENTED; }

    // child process
    return mpu->stream_ofmap(mpu, flow_id, to_user_ofmap_data_buffer, delay_time_in_ms);
}

memx_status memx_mpu_download_firmware(MemxMpu *mpu, const char *file_path)
{
    if (!mpu || !file_path) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!mpu->download_firmware) { return MEMX_STATUS_MPU_NOT_IMPLEMENTED; }

    if (memx_status_error(mpu->download_firmware(mpu, file_path))) {
        printf("download firmware fail\n");
        return MEMX_STATUS_MPU_DOWNLOAD_FIRMWARE_FAIL;
    }
    uint8_t tmp = 0; // for NULL data checking in low-level API
    if (memx_status_error(mpu->operation(mpu, MEMX_CMD_GET_FW_DOWNLOAD_STATUS, &tmp, 0, 0))) {
        printf("get firmware update status fail\n");
        return MEMX_STATUS_MPU_DOWNLOAD_FIRMWARE_FAIL;
    }
    return MEMX_STATUS_OK;
}


memx_status memx_mpu_download_model(MemxMpu *mpu, void * pDfpMeta, uint8_t model_idx, int type, int timeout)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!mpu->download_model) { return MEMX_STATUS_MPU_NOT_IMPLEMENTED; }

    // try to download weight memory first if is required
    if (type & MEMX_MPU_DOWNLOAD_MODEL_TYPE_WEIGHT_MEMORY) {
        if (memx_status_error(mpu->download_model(mpu, pDfpMeta, model_idx, MEMX_MPU_DOWNLOAD_MODEL_TYPE_WEIGHT_MEMORY, timeout))) {
            printf("download weight memory fail\n");
            return MEMX_STATUS_MPU_DOWNLOAD_MODEL_FAIL;
        }
    }

    // try to download model after weight memory
    // since after download, model starts immediately with weight memory pre-fetched into registers
    if (type & MEMX_MPU_DOWNLOAD_MODEL_TYPE_MPU_CONFIG) {
        if (memx_status_error(mpu->download_model(mpu, pDfpMeta, model_idx, MEMX_MPU_DOWNLOAD_MODEL_TYPE_MPU_CONFIG, timeout))) {
            printf("download model config fail\n");
            return MEMX_STATUS_MPU_DOWNLOAD_MODEL_FAIL;
        }
    }

    // FIXME: support model_idx 0 only for now, is it ok ?
    // try to update input feature map shape from DFP file un-encryped region
    if (model_idx == 0) {
        uint8_t input_port_active_number = 0;
        uint8_t output_port_active_number = 0;
        pDfpContext pDfp = (pDfpContext) pDfpMeta;

        mpu->input_mode_flag = pDfp->input_mode_flag;

        for (uint32_t port_idx = 0; port_idx < pDfp->input_port_number; ++port_idx) {
            MemxDfpPortConfig *input_port_config = &pDfp->pInputConfigList[port_idx];
            if (input_port_config->active == 0) { continue; } // skip inactive ports

            input_port_active_number++;

            uint8_t ifmap_format = input_port_config->format;

            if (memx_status_error(memx_mpu_set_ifmap_size(mpu, input_port_config->port,
                                                          input_port_config->dim_x,
                                                          input_port_config->dim_y,
                                                          input_port_config->dim_z,
                                                          input_port_config->dim_c,
                                                          ifmap_format, timeout))) {
                printf("memx_mpu_set_ifmap_size fail\n");
                return MEMX_STATUS_MPU_SET_IFMAP_SIZE_FAIL;
            }

            if (input_port_config->range_convert_enabled == 1) {
                if (memx_status_error(memx_mpu_set_ifmap_range_convert(mpu, input_port_config->port, 1, input_port_config->range_convert_shift,
                                                                        input_port_config->range_convert_scale, timeout))) {
                    printf("memx_mpu_set_ifmap_range_convert fail\n");
                    return MEMX_STATUS_MPU_SET_IFMAP_RANGE_CONVERT_FAIL;
                }
            }
        }
        mpu->max_input_flow_cnt = input_port_active_number;

        for (uint32_t port_idx = 0; port_idx < pDfp->output_port_number; ++port_idx) {
            MemxDfpPortConfig *output_port_config = &pDfp->pOuputConfigList[port_idx];
            if (output_port_config->active == 0) { continue; } // skip inactive ports

            output_port_active_number++;

            uint8_t ofmap_format = output_port_config->format;

            if (memx_status_error(memx_mpu_set_ofmap_size(mpu, output_port_config->port,
                                                          output_port_config->dim_x,
                                                          output_port_config->dim_y,
                                                          output_port_config->dim_z,
                                                          output_port_config->dim_c,
                                                          ofmap_format, timeout))) {
                printf("memx_mpu_set_ofmap_size fail\n");
                return MEMX_STATUS_MPU_SET_OFMAP_SIZE_FAIL;
            }

            if (output_port_config->hpoc_en == 1) {
                int *hpoc_indexes = (int*) malloc(output_port_config->hpoc_list_length * sizeof(int));
                if(!hpoc_indexes) return MEMX_STATUS_MPU_SET_OFMAP_HPOC_FAIL;

                for(int hi=0; hi < output_port_config->hpoc_list_length; hi++){
                    hpoc_indexes[hi] = output_port_config->hpoc_dummy_channels[hi];
                }

                if (memx_status_error(memx_mpu_set_ofmap_hpoc(mpu, output_port_config->port, (int) output_port_config->hpoc_list_length, hpoc_indexes, timeout))) {
                    printf("memx_mpu_set_ofmap_hpoc fail\n");
                    free(hpoc_indexes); hpoc_indexes = NULL;
                    return MEMX_STATUS_MPU_SET_OFMAP_HPOC_FAIL;
                }
                free(hpoc_indexes); hpoc_indexes = NULL;
            }
        }
        mpu->max_output_flow_cnt = output_port_active_number;

        // for USB driver to calculate SRAM space used for each output port in Cascade
        if (input_port_active_number > 0 && output_port_active_number > 0) {
            if (memx_status_error(memx_mpu_update_fmap_size(mpu, input_port_active_number, output_port_active_number, timeout))) {
                printf("memx_mpu_update_fmap_size fail\n");
                return MEMX_STATUS_MPU_UPDATE_FMAP_FAIL;
            }
        }
    }

    return MEMX_STATUS_OK;
}

memx_status memx_mpu_set_stream_enable(MemxMpu *mpu, int wait, int timeout)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!mpu->set_stream_enable) { return MEMX_STATUS_MPU_NOT_IMPLEMENTED; }
    // child process
    return mpu->set_stream_enable(mpu, wait, timeout);
}

memx_status memx_mpu_set_stream_disable(MemxMpu* mpu, int wait, int timeout)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!mpu->set_stream_disable) { return MEMX_STATUS_MPU_NOT_IMPLEMENTED; }
    // child process
    return mpu->set_stream_disable(mpu, wait, timeout);
}

memx_status memx_mpu_set_ifmap_queue_size(MemxMpu* mpu, int size, int timeout)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!mpu->set_ifmap_queue_size) { return MEMX_STATUS_MPU_NOT_IMPLEMENTED; }
    // child process
    return mpu->set_ifmap_queue_size(mpu, size, timeout);
}

memx_status memx_mpu_set_ifmap_size(MemxMpu* mpu, uint8_t flow_id, int height, int width, int z, int channel_number, int format, int timeout)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!mpu->set_ifmap_size) { return MEMX_STATUS_MPU_NOT_IMPLEMENTED; }
    if (flow_id >= mpu->max_input_flow_cnt) {return MEMX_STATUS_MPU_INVALID_PARAMETER; }

    MemxMpuCommIfmapInfo *ifmap_info = NULL;
    // find corresponds flow description from list stored
    for (int idx = 0; idx < memx_list_count(mpu->ifmaps); ++idx) {
        ifmap_info = (MemxMpuCommIfmapInfo *)memx_list_peek(mpu->ifmaps, idx);
        if (ifmap_info && ifmap_info->fmap.flow_id == flow_id) { break; }
        // To avoid point to last ifmap_info which flow_id is not match case when loop run out exit
        ifmap_info = NULL;
    }
    // otherwise creates a new one
    if (!ifmap_info) {
        ifmap_info = (MemxMpuCommIfmapInfo *)malloc(sizeof(MemxMpuCommIfmapInfo));
        if (!ifmap_info) { return MEMX_STATUS_MPU_SET_IFMAP_SIZE_FAIL; }
        memx_list_push(mpu->ifmaps, ifmap_info);
    }

    // update feature map information
    ifmap_info->fmap.flow_id = flow_id;
    ifmap_info->fmap.format = format;
    ifmap_info->fmap.shape[MEMX_MPU_FMAP_SHAPE_INDEX_CHANNEL_NUMBER] = channel_number;
    ifmap_info->fmap.shape[MEMX_MPU_FMAP_SHAPE_INDEX_WIDTH] = width;
    ifmap_info->fmap.shape[MEMX_MPU_FMAP_SHAPE_INDEX_HEIGHT] = height;
    ifmap_info->fmap.shape[MEMX_MPU_FMAP_SHAPE_INDEX_Z] = z;
    ifmap_info->range_convert_enabled = 0;
    ifmap_info->range_convert_shift = 0;
    ifmap_info->range_convert_scale = 0;

    // child process
    return mpu->set_ifmap_size(mpu, flow_id, height, width, z, channel_number, format, timeout);
}

memx_status memx_mpu_set_ifmap_range_convert(MemxMpu *mpu, uint8_t flow_id, int enable, float shift, float scale, int timeout)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }

    MemxMpuCommIfmapInfo *ifmap_info = NULL;
    // find corresponds flow description from list stored
    for (int idx = 0; idx < memx_list_count(mpu->ifmaps); ++idx) {
        ifmap_info = (MemxMpuCommIfmapInfo *)memx_list_peek(mpu->ifmaps, idx);
        if (ifmap_info && ifmap_info->fmap.flow_id == flow_id) { break; }
    }
    if (!ifmap_info) { return MEMX_STATUS_MPU_IFMAP_NOT_CONFIG; }

    // update feature map range conversion information
    if (enable == 1) {
        ifmap_info->range_convert_enabled = 1;
        ifmap_info->range_convert_shift = shift;
        ifmap_info->range_convert_scale = scale;
    } else {
        ifmap_info->range_convert_enabled = 0;
        ifmap_info->range_convert_shift = 0;
        ifmap_info->range_convert_scale = 0;
    }

    // child process
    if ((enable == 1) && (mpu->set_ifmap_range_convert)) {
        return mpu->set_ifmap_range_convert(mpu, flow_id, shift, scale, timeout);
    } else if ((enable == 0) && (mpu->set_ifmap_size)) {
        // calling 'set_ifmap_size' to reset
        return mpu->set_ifmap_size(mpu, flow_id,
                                   ifmap_info->fmap.shape[MEMX_MPU_FMAP_SHAPE_INDEX_HEIGHT],
                                   ifmap_info->fmap.shape[MEMX_MPU_FMAP_SHAPE_INDEX_WIDTH],
                                   ifmap_info->fmap.shape[MEMX_MPU_FMAP_SHAPE_INDEX_Z],
                                   ifmap_info->fmap.shape[MEMX_MPU_FMAP_SHAPE_INDEX_CHANNEL_NUMBER],
                                   ifmap_info->fmap.format,
                                   timeout);
    }
    return MEMX_STATUS_MPU_NOT_IMPLEMENTED;
}

memx_status memx_mpu_set_ofmap_queue_size(MemxMpu* mpu, int size, int timeout)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!mpu->set_ofmap_queue_size) { return MEMX_STATUS_MPU_NOT_IMPLEMENTED; }
    // child process
    return mpu->set_ofmap_queue_size(mpu, size, timeout);
}

memx_status memx_mpu_set_ofmap_size(MemxMpu* mpu, uint8_t flow_id, int height, int width, int z, int channel_number, int format, int timeout)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!mpu->set_ofmap_size) { return MEMX_STATUS_MPU_NOT_IMPLEMENTED; }
    if (flow_id >= mpu->max_output_flow_cnt) {return MEMX_STATUS_MPU_INVALID_PARAMETER; }

    MemxMpuCommOfmapInfo *ofmap_info = NULL;
    // find corresponds flow description from list stored
    for (int idx = 0;idx < memx_list_count(mpu->ofmaps); ++idx) {
        ofmap_info = (MemxMpuCommOfmapInfo *)memx_list_peek(mpu->ofmaps, idx);
        if (ofmap_info && ofmap_info->fmap.flow_id == flow_id) { break; }
        // prevent not found after loop run out
        ofmap_info = NULL;
    }
    // otherwise creates a new one
    if (!ofmap_info) {
        ofmap_info = (MemxMpuCommOfmapInfo *)malloc(sizeof(MemxMpuCommOfmapInfo));
        if (!ofmap_info) { return MEMX_STATUS_MPU_SET_OFMAP_SIZE_FAIL; }
        ofmap_info->hpoc_indexes = NULL; // must init. to nullptr after creation
        memx_list_push(mpu->ofmaps, ofmap_info);
    }

    // update feature map information
    ofmap_info->fmap.flow_id = flow_id;
    ofmap_info->fmap.format = format;
    ofmap_info->fmap.shape[MEMX_MPU_FMAP_SHAPE_INDEX_CHANNEL_NUMBER] = channel_number;
    ofmap_info->fmap.shape[MEMX_MPU_FMAP_SHAPE_INDEX_WIDTH] = width;
    ofmap_info->fmap.shape[MEMX_MPU_FMAP_SHAPE_INDEX_HEIGHT] = height;
    ofmap_info->fmap.shape[MEMX_MPU_FMAP_SHAPE_INDEX_Z] = z;

    ofmap_info->hpoc_size = 0;
    if (ofmap_info->hpoc_indexes != NULL) {
        free(ofmap_info->hpoc_indexes);
    }
    ofmap_info->hpoc_indexes = NULL;

    // child process
    return mpu->set_ofmap_size(mpu, flow_id, height, width, z, channel_number, format, timeout);
}

memx_status memx_mpu_set_ofmap_hpoc(MemxMpu *mpu, uint8_t flow_id, int hpoc_size, int *hpoc_indexes, int timeout)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (hpoc_size < 0) { return MEMX_STATUS_MPU_INVALID_PARAMETER; }
    if (!hpoc_indexes) { return MEMX_STATUS_MPU_SET_OFMAP_HPOC_FAIL; }

    MemxMpuCommOfmapInfo *ofmap_info = NULL;
    // find corresponds flow description from list stored
    for (int idx = 0; idx < memx_list_count(mpu->ofmaps); ++idx) {
        ofmap_info = (MemxMpuCommOfmapInfo *)memx_list_peek(mpu->ofmaps, idx);
        if (ofmap_info && ofmap_info->fmap.flow_id == flow_id) { break; }
    }
    if (!ofmap_info) { return MEMX_STATUS_MPU_OFMAP_NOT_CONFIG; }

    // release original allocated resource
    ofmap_info->hpoc_size = 0;
    if (ofmap_info->hpoc_indexes != NULL) {
        free(ofmap_info->hpoc_indexes);
        ofmap_info->hpoc_indexes = NULL;
    }

    // allocate new hpoc array
    if (hpoc_size > 0) {
        ofmap_info->hpoc_indexes = (int *)malloc(sizeof(int) * hpoc_size);
        if (!ofmap_info->hpoc_indexes) { return MEMX_STATUS_MPU_SET_OFMAP_HPOC_FAIL; }
        memcpy(ofmap_info->hpoc_indexes, hpoc_indexes, sizeof(int) * hpoc_size);
        ofmap_info->hpoc_size = hpoc_size;
    }

    // child process
    if (!mpu->set_ofmap_hpoc) {
        ofmap_info->hpoc_size = 0;
        if (ofmap_info->hpoc_indexes != NULL) {
            free(ofmap_info->hpoc_indexes);
            ofmap_info->hpoc_indexes = NULL;
        }
        return MEMX_STATUS_MPU_NOT_IMPLEMENTED;
    }
    memx_status status = MEMX_STATUS_OK;
    if (hpoc_size > 0) {
        status = mpu->set_ofmap_hpoc(mpu, flow_id, hpoc_size, hpoc_indexes, timeout);
    } else {  // call 'set_ofmap_size' to reset
        status = mpu->set_ofmap_size(mpu, flow_id,
                                     ofmap_info->fmap.shape[MEMX_MPU_FMAP_SHAPE_INDEX_HEIGHT],
                                     ofmap_info->fmap.shape[MEMX_MPU_FMAP_SHAPE_INDEX_WIDTH],
                                     ofmap_info->fmap.shape[MEMX_MPU_FMAP_SHAPE_INDEX_Z],
                                     ofmap_info->fmap.shape[MEMX_MPU_FMAP_SHAPE_INDEX_CHANNEL_NUMBER],
                                     ofmap_info->fmap.format,
                                     timeout);
    }

    // clean-up on error
    if (memx_status_error(status)) {
        ofmap_info->hpoc_size = 0;
        if (ofmap_info->hpoc_indexes != NULL) {
            free(ofmap_info->hpoc_indexes);
            ofmap_info->hpoc_indexes = NULL;
        }
    }
    return status;
}

memx_status memx_mpu_update_fmap_size(MemxMpu* mpu, uint8_t in_flow_count, uint8_t out_flow_count, int timeout)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!mpu->update_fmap_size) { return MEMX_STATUS_MPU_NOT_IMPLEMENTED; }
    // child process
    return mpu->update_fmap_size(mpu, in_flow_count, out_flow_count, timeout);
}

memx_status memx_mpu_get_ifmap_size(MemxMpu* mpu, uint8_t flow_id, int *height, int *width, int *z, int *channel_number, int *format, int timeout)
{
    unused(timeout);

    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }

    if (!height || !width || !z || !channel_number || !format) { return MEMX_STATUS_MPU_INVALID_PARAMETER; }
    MemxMpuCommIfmapInfo *ifmap_info = NULL;

    // find corresponds flow description from list stored
    for (int idx = 0; idx < memx_list_count(mpu->ifmaps); ++idx) {
        ifmap_info = (MemxMpuCommIfmapInfo *)memx_list_peek(mpu->ifmaps, idx);
        if (ifmap_info && ifmap_info->fmap.flow_id == flow_id) { break; }
        // prevent not found after loop run out
        ifmap_info = NULL;
    }

    // not found
    if (!ifmap_info) { *format = 0; *channel_number = 0; *z = 0; *width = 0; *height = 0; return MEMX_STATUS_MPU_GET_IFMAP_SIZE_FAIL; }

    // update variables
    *format = ifmap_info->fmap.format;
    *channel_number = ifmap_info->fmap.shape[MEMX_MPU_FMAP_SHAPE_INDEX_CHANNEL_NUMBER];
    *z = ifmap_info->fmap.shape[MEMX_MPU_FMAP_SHAPE_INDEX_Z];
    *width = ifmap_info->fmap.shape[MEMX_MPU_FMAP_SHAPE_INDEX_WIDTH];
    *height = ifmap_info->fmap.shape[MEMX_MPU_FMAP_SHAPE_INDEX_HEIGHT];

    return MEMX_STATUS_OK;
}

memx_status memx_mpu_get_ifmap_range_convert(MemxMpu* mpu, uint8_t flow_id, int *enable, float *shift, float *scale, int timeout)
{
    unused(timeout);

    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!enable || !shift || !scale) { return MEMX_STATUS_MPU_INVALID_PARAMETER; }

    MemxMpuCommIfmapInfo *ifmap_info = NULL;
    // find corresponds flow description from list stored
    for (int idx = 0; idx < memx_list_count(mpu->ifmaps); ++idx) {
        ifmap_info = (MemxMpuCommIfmapInfo *)memx_list_peek(mpu->ifmaps, idx);
        if (ifmap_info && ifmap_info->fmap.flow_id == flow_id) { break; }
    }
    // not found
    if (!ifmap_info) { *enable = 0; *shift = 0; *scale = 0; return MEMX_STATUS_MPU_GET_IFMAP_RANGE_CONVERT_FAIL; }

    // update variables
    *enable = ifmap_info->range_convert_enabled;
    *shift = ifmap_info->range_convert_shift;
    *scale = ifmap_info->range_convert_scale;

    return MEMX_STATUS_OK;
}

memx_status memx_mpu_get_ofmap_size(MemxMpu* mpu, uint8_t flow_id, int *height, int *width, int32_t *z, int *channel_number, int *format, int timeout)
{
    unused(timeout);

    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!height || !width || !z || !channel_number || !format) { return MEMX_STATUS_MPU_INVALID_PARAMETER; }

    MemxMpuCommOfmapInfo *ofmap_info = NULL;
    // find corresponds flow description from list stored
    for (int idx = 0; idx < memx_list_count(mpu->ofmaps); ++idx) {
        ofmap_info = (MemxMpuCommOfmapInfo *)memx_list_peek(mpu->ofmaps, idx);
        if (ofmap_info && ofmap_info->fmap.flow_id == flow_id) { break; }
        // prevent not found after loop run out
        ofmap_info = NULL;
    }

    // not found
    if (!ofmap_info) { *format = 0; *channel_number = 0;  *z = 0; *width = 0; *height = 0; return MEMX_STATUS_MPU_GET_IFMAP_SIZE_FAIL; }

    // update variables
    *format = ofmap_info->fmap.format;
    *channel_number = ofmap_info->fmap.shape[MEMX_MPU_FMAP_SHAPE_INDEX_CHANNEL_NUMBER];
    *z = ofmap_info->fmap.shape[MEMX_MPU_FMAP_SHAPE_INDEX_Z];
    *width = ofmap_info->fmap.shape[MEMX_MPU_FMAP_SHAPE_INDEX_WIDTH];
    *height = ofmap_info->fmap.shape[MEMX_MPU_FMAP_SHAPE_INDEX_HEIGHT];
    return MEMX_STATUS_OK;
}

memx_status memx_mpu_get_ofmap_hpoc(MemxMpu* mpu, uint8_t flow_id, int* hpoc_size, int** hpoc_indexes, int timeout)
{
    unused(timeout);

    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!hpoc_size || !hpoc_indexes) { return MEMX_STATUS_MPU_INVALID_PARAMETER; }

    MemxMpuCommOfmapInfo *ofmap_info = NULL;
        // find corresponds flow description from list stored
    for (int idx = 0; idx < memx_list_count(mpu->ofmaps); ++idx) {
        ofmap_info = (MemxMpuCommOfmapInfo *)memx_list_peek(mpu->ofmaps, idx);
        if (ofmap_info && ofmap_info->fmap.flow_id == flow_id) { break; }
    }

    // not found
    if (!ofmap_info) { *hpoc_size = 0; *hpoc_indexes = NULL; return MEMX_STATUS_MPU_GET_OFMAP_HPOC_FAIL; }

    // 'lend' internal array to user
    *hpoc_size = ofmap_info->hpoc_size;
    *hpoc_indexes = ofmap_info->hpoc_indexes;
    return MEMX_STATUS_OK;
}

memx_status memx_mpu_get_chip_gen(MemxMpu* mpu, uint8_t* chip_gen, int timeout)
{
    unused(timeout);
    memx_status status = MEMX_STATUS_OK;
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!chip_gen) { return MEMX_STATUS_MPU_INVALID_PARAMETER; }

    switch(mpu->chip_gen){
        case MEMX_MPU_CHIP_GEN_CASCADE_PLUS:
            *chip_gen = mpu->chip_gen;
            break;
        default:
            *chip_gen = MEMX_MPU_CHIP_GEN_INVALID;
            status = MEMX_STATUS_MPU_INVALID_CHIP_GEN;
            break;
    }

    return status;
}

memx_status memx_mpu_set_powerstate(MemxMpu* mpu, uint8_t group_id, uint8_t state)
{
    memx_status status = MEMX_STATUS_OK;
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (state >= MEMX_PS4) { return MEMX_STATUS_MPU_INVALID_PARAMETER; }

    for(uint8_t chip_id = mpu->input_chip_id; chip_id <= mpu->output_chip_id; ++chip_id){
        status = memx_set_feature(group_id, chip_id, OPCODE_SET_POWERMANAGEMENT, state);
        if (!memx_status_no_error(status)) {
        break;
        }
    }

    return status;
}

memx_status memx_mpu_enqueue_ifmap_buf(MemxMpu* mpu, uint8_t flow_id, memx_fmap_buf_t* fmap_buf, int32_t timeout)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!mpu->enqueue_ifmap_buf) { return MEMX_STATUS_MPU_NOT_IMPLEMENTED; }

    // child process
    return mpu->enqueue_ifmap_buf(mpu, flow_id, fmap_buf, timeout);
}

memx_status memx_mpu_enqueue_ofmap_buf(MemxMpu* mpu, uint8_t flow_id, memx_fmap_buf_t* fmap_buf, int32_t timeout)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!mpu->enqueue_ofmap_buf) { return MEMX_STATUS_MPU_NOT_IMPLEMENTED; }

    // child process
    return mpu->enqueue_ofmap_buf(mpu, flow_id, fmap_buf, timeout);
}

memx_status memx_mpu_dequeue_ifmap_buf(MemxMpu* mpu, uint8_t flow_id, memx_fmap_buf_t* fmap_buf, int32_t timeout)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!mpu->dequeue_ifmap_buf) { return MEMX_STATUS_MPU_NOT_IMPLEMENTED; }

    // child process
    return mpu->dequeue_ifmap_buf(mpu, flow_id, fmap_buf, timeout);
}

memx_status memx_mpu_dequeue_ofmap_buf(MemxMpu* mpu, uint8_t flow_id, memx_fmap_buf_t* fmap_buf, int32_t timeout)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }
    if (!mpu->dequeue_ofmap_buf) { return MEMX_STATUS_MPU_NOT_IMPLEMENTED; }

    // child process
    return mpu->dequeue_ofmap_buf(mpu, flow_id, fmap_buf, timeout);
}

memx_status memx_mpu_set_read_abort(MemxMpu* mpu)
{
    if (!mpu) { return MEMX_STATUS_MPU_INVALID_CONTEXT; }
    if (!mpu->mpuio) { return MEMX_STATUS_MPU_INVALID_IO_CONTEXT; }

    return mpu->set_read_abort(mpu->mpuio);
}
