/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#include "memx_usb.h"
#include "memx_pcie.h"
#include "memx_mpu_comm.h"
#include "memx_mpuio_comm.h"
#include "memx_log.h"

#include <stdlib.h>

/***************************************************************************//**
 * implementation
 ******************************************************************************/
MemxMpuIo *memx_mpuio_create(void)
{
    // create new context
    MemxMpuIo *mpuio = (MemxMpuIo *)malloc(sizeof(MemxMpuIo));
    if (!mpuio) { return NULL; }
    memset(mpuio, 0, sizeof(MemxMpuIo));
    return mpuio;
}

MemxMpuIo *memx_mpuio_init(uint8_t group_id, uint8_t interface_type, uint8_t chip_gen)
{
    if (group_id >= MEMX_MPU_GROUP) { printf("Invaild group id(%u)\n", group_id); return NULL; }
    if (interface_type > MEMX_MPUIO_INTERFACE_PCIE) { printf("non-support bus type(%u)\n", interface_type); return NULL; }

    // create mpuio object
    MemxMpuIo *mpuio = memx_mpuio_create();
    if (!mpuio) { printf("mpuio create failed\n"); return NULL; }

    // FIXME: need to immplement related dispatch code, for now only fixed in usb interface
    switch (interface_type) {
    case MEMX_MPUIO_INTERFACE_PCIE: {
        // setup mpuio callback with pcie
        mpuio->destroy = (_memx_mpuio_destroy_cb)&memx_pcie_destroy;
        mpuio->set_read_abort_start = (_memx_mpuio_set_read_abort_start)&memx_pcie_set_read_abort_start;
        mpuio->operation = (_memx_mpuio_operation_cb)&memx_pcie_operation;
        mpuio->stream_write = (_memx_mpuio_stream_write_cb)&memx_pcie_stream_write;
        mpuio->stream_read = (_memx_mpuio_stream_read_cb)&memx_pcie_stream_read;
        mpuio->download_model = (_memx_mpuio_download_model_cb)&memx_pcie_download_model;
        mpuio->download_firmware = (_memx_mpuio_download_firmware_cb)&memx_pcie_download_firmware;
        mpuio->set_ifmap_size = (_memx_mpuio_set_ifmap_size_cb)&memx_pcie_set_ifmap_size;
        mpuio->set_ofmap_size = (_memx_mpuio_set_ofmap_size_cb)&memx_pcie_set_ofmap_size;
        mpuio->update_fmap_size = (_memx_mpuio_update_fmap_size_cb)&memx_pcie_update_fmap_size;
        mpuio->model_id = 0xff; // assign an invalid model ID for first initialize.
        mpuio->ifmap_control = 0;

        // create mpuio context and hook
        platform_device_t *memx_pcie_device = NULL;
        if (memx_pcie_create_context(&memx_pcie_device, group_id, chip_gen)) {
            memx_mpuio_destroy(mpuio); // remember to release mpuio in case fail
            return NULL;
        }
        mpuio->context = (_memx_mpuio_context_t)memx_pcie_device;
        memcpy((void*)&mpuio->hw_info, (void*)&memx_pcie_device->pdev.pcie->hw_info, sizeof(struct hw_info));
    }
    break;
    default:
        // default setup mpuio callback with usb
        mpuio->destroy = (_memx_mpuio_destroy_cb)&memx_usb_destroy;
        mpuio->set_read_abort_start = (_memx_mpuio_set_read_abort_start)&memx_usb_set_read_abort_start;
        mpuio->operation = (_memx_mpuio_operation_cb)&memx_usb_operation;
        mpuio->control_write = (_memx_mpuio_control_write_cb)&memx_usb_control_write;
        mpuio->control_read = (_memx_mpuio_control_read_cb)&memx_usb_control_read;
        mpuio->stream_write = (_memx_mpuio_stream_write_cb)&memx_usb_stream_write;
        mpuio->stream_read = (_memx_mpuio_stream_read_cb)&memx_usb_stream_read;
        mpuio->download_model = (_memx_mpuio_download_model_cb)&memx_usb_download_model;
        mpuio->download_firmware = (_memx_mpuio_download_firmware_cb)&memx_usb_download_firmware;
        mpuio->set_ifmap_size = (_memx_mpuio_set_ifmap_size_cb)&memx_usb_set_ifmap_size;
        mpuio->set_ofmap_size = (_memx_mpuio_set_ofmap_size_cb)&memx_usb_set_ofmap_size;
        mpuio->update_fmap_size = (_memx_mpuio_update_fmap_size_cb)&memx_usb_update_fmap_size;
        mpuio->model_id = 0xff; // assign an invalid model ID for first initialize.
        mpuio->ifmap_control = 0;

        // create mpuio context and hook
        platform_device_t *memx_usb_device = NULL;
        if (memx_usb_create_context(&memx_usb_device, group_id, chip_gen)) {
            memx_mpuio_destroy(mpuio);
            return NULL;
        }
        mpuio->context = (_memx_mpuio_context_t)memx_usb_device;
        memcpy((void*)&mpuio->hw_info, (void *)&memx_usb_device->pdev.usb->hw_info, sizeof(struct hw_info));
    }
    return mpuio;
}

void memx_mpuio_destroy(MemxMpuIo *mpuio)
{
    if (!mpuio) { return; }
    if (!mpuio->destroy) { printf("mpuio destroy callback is NULL\n"); free(mpuio); mpuio = NULL; return; }

    // clean-up child context
    mpuio->destroy(mpuio);
    // clean-up self context
    free(mpuio); // kill self context
    mpuio = NULL;
}

memx_status memx_mpuio_set_read_abort_start(MemxMpuIo *mpuio)
{
    if (!mpuio) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    if (!mpuio->set_read_abort_start) { return MEMX_STATUS_MPUIO_FUNCTION_NOT_IMPLEMENTED; }

    return mpuio->set_read_abort_start(mpuio);
}

memx_status memx_mpuio_operation(MemxMpuIo* mpuio, uint8_t chip_id, int32_t cmd_id, void* data, uint32_t size, int32_t timeout)
{
    if (!mpuio) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    if (!mpuio->operation) { return MEMX_STATUS_MPUIO_FUNCTION_NOT_IMPLEMENTED; }
    // child process
    return mpuio->operation(mpuio, chip_id, cmd_id, data, size, timeout);
}

memx_status memx_mpuio_download_firmware(MemxMpuIo *mpuio, const char *file_path)
{
    if (!mpuio || !file_path) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    if (!mpuio->download_firmware) { return MEMX_STATUS_MPUIO_FUNCTION_NOT_IMPLEMENTED; }
    // child process
    return mpuio->download_firmware(mpuio, file_path);
}

memx_status memx_mpuio_download_model(MemxMpuIo* mpuio, uint8_t chip_id, void * pDfpMeta, uint8_t model_idx, int32_t type, int32_t timeout)
{
    if (!mpuio) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    if (!mpuio->download_model) { return MEMX_STATUS_MPUIO_FUNCTION_NOT_IMPLEMENTED; }
    // child process
    return mpuio->download_model(mpuio, chip_id, pDfpMeta, model_idx, type, timeout);
}

memx_status memx_mpuio_set_ifmap_size(MemxMpuIo* mpuio, uint8_t chip_id, uint8_t flow_id, int32_t height, int32_t width,  int32_t z, int32_t channel_number, int32_t format, int32_t timeout)
{
    if (!mpuio) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    if (!mpuio->set_ifmap_size) { return MEMX_STATUS_MPUIO_FUNCTION_NOT_IMPLEMENTED; }
    // child process
    return mpuio->set_ifmap_size(mpuio, chip_id, flow_id, height, width, z, channel_number, format, timeout);
}

memx_status memx_mpuio_set_ofmap_size(MemxMpuIo* mpuio, uint8_t chip_id, uint8_t flow_id, int32_t height, int32_t width,  int32_t z, int32_t channel_number, int32_t format, int32_t timeout)
{
    if (!mpuio) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    if (!mpuio->set_ofmap_size) { return MEMX_STATUS_MPUIO_FUNCTION_NOT_IMPLEMENTED; }
    // child process
    return mpuio->set_ofmap_size(mpuio, chip_id, flow_id, height, width, z, channel_number, format, timeout);
}

memx_status memx_mpuio_update_fmap_size(MemxMpuIo* mpuio, uint8_t chip_id, uint8_t in_flow_count, uint8_t out_flow_count, int32_t timeout)
{
    if (!mpuio) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    if (!mpuio->update_fmap_size) { return MEMX_STATUS_MPUIO_FUNCTION_NOT_IMPLEMENTED; }
    // child process
    return mpuio->update_fmap_size(mpuio, chip_id, in_flow_count, out_flow_count, timeout);
}

memx_status memx_mpuio_control_write(MemxMpuIo* mpuio, uint8_t chip_id, uint32_t address, uint8_t* data, int32_t length, int32_t* transferred, int32_t increment, int32_t timeout)
{
    if (!mpuio) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    if (!mpuio->control_write) { return MEMX_STATUS_MPUIO_FUNCTION_NOT_IMPLEMENTED; }
    // child process
    return mpuio->control_write(mpuio, chip_id, address, data, length, transferred, increment, timeout);
}

memx_status memx_mpuio_control_read(MemxMpuIo* mpuio, uint8_t chip_id, uint32_t address, uint8_t* data, int32_t length, int32_t* transferred, int32_t increment, int32_t timeout)
{
    if (!mpuio) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    if (!mpuio->control_read) { return MEMX_STATUS_MPUIO_FUNCTION_NOT_IMPLEMENTED; }
    // child process
    return mpuio->control_read(mpuio, chip_id, address, data, length, transferred, increment, timeout);
}

memx_status memx_mpuio_stream_write(MemxMpuIo* mpuio, uint8_t chip_id, uint8_t flow_id, uint8_t* data, int32_t length, int32_t* transferred, int32_t timeout)
{
    if (!mpuio) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    if (!mpuio->stream_write) { return MEMX_STATUS_MPUIO_FUNCTION_NOT_IMPLEMENTED; }
    // child process
    return mpuio->stream_write(mpuio, chip_id, flow_id, data, length, transferred, timeout);
}

memx_status memx_mpuio_stream_read(MemxMpuIo* mpuio, uint8_t chip_id, uint8_t flow_id, uint8_t* data, int32_t length, int32_t* transferred, int32_t timeout)
{
    if (!mpuio) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    if (!mpuio->stream_read) { return MEMX_STATUS_MPUIO_FUNCTION_NOT_IMPLEMENTED; }
    // child process
    return mpuio->stream_read(mpuio, chip_id, flow_id, data, length, transferred, timeout);
}

memx_status memx_mpuio_attach_idle_mpu_group(MemxMpuIo* mpuio, uint8_t *attached_mpu_group_id)
{
    if (!mpuio || attached_mpu_group_id == NULL) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    *attached_mpu_group_id = 0xff;
    for (uint8_t mpu_group_id = 0; mpu_group_id <= mpuio->hw_info.chip.group_count; mpu_group_id++) {
        if (mpuio->mpu_group_status[mpu_group_id] == MEMX_MPUIO_MPU_GROUP_STATUS_IDLE) {
            mpuio->mpu_group_status[mpu_group_id] = MEMX_MPUIO_MPU_GROUP_STATUS_BUSY;
            *attached_mpu_group_id = mpu_group_id;
            break;
        }
    }

    MEMX_LOG(MEMX_LOG_DOWNLOAD_DFP, "Attach MPU group %d \n", *attached_mpu_group_id);
    return (*attached_mpu_group_id == 0xff) ? MEMX_STATUS_DEVICE_IN_USE: MEMX_STATUS_OK;
}

memx_status memx_mpuio_detach_mpu_group(MemxMpuIo* mpuio, uint8_t mpu_group_id)
{
    if (!mpuio || mpu_group_id > MEMX_MPUIO_MAX_HW_MPU_COUNT - 1) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    MEMX_LOG(MEMX_LOG_DOWNLOAD_DFP, "Detach MPU group %d \n", mpu_group_id);
    mpuio->mpu_group_status[mpu_group_id] = MEMX_MPUIO_MPU_GROUP_STATUS_IDLE;
    return MEMX_STATUS_OK;
}
