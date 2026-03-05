/**
 * @file queue.c
 *
 * @brief This file contains the queue entry points and callbacks.
 * @version 0.1
 * @date 2023
 *
 * @copyright Copyright (c) 2023 MemryX. Inc. All Rights Reserved.
 *
 */
#include "private.h"
#include "queue.tmh"
static NTSTATUS abort_all_request_on_pipe(WDFDEVICE Device);
static NTSTATUS FW_Recv(PDEVICE_CONTEXT DeviceContext, WDFREQUEST Request, PVOID buf, ULONG buf_len);
static NTSTATUS FW_Send(PDEVICE_CONTEXT DeviceContext, WDFREQUEST Request, PVOID buf, ULONG buf_len);
static NTSTATUS FW_SendCfgHeader(PDEVICE_CONTEXT DeviceContext, WDFREQUEST Request, ULONG FwCfgCmd, ULONG FwCfgCmdPayloadSize);
static NTSTATUS FW_ReadChip0Sram(PDEVICE_CONTEXT DeviceContext, WDFREQUEST Request, ULONG address, ULONG size, PVOID buf);
static void clear_fw_id(PDEVICE_CONTEXT DeviceContext, WDFREQUEST Request);
static void reset_mpu(PDEVICE_CONTEXT DeviceContext, WDFREQUEST Request, ULONG Id);
static void set_dfp_chip_id(PDEVICE_CONTEXT DeviceContext, WDFREQUEST Request, ULONG Id);
static void set_wtmem_addr(PDEVICE_CONTEXT DeviceContext, WDFREQUEST Request, ULONG Id, ULONG Addr);
static void set_wtmem_size(PDEVICE_CONTEXT DeviceContext, WDFREQUEST Request, ULONG Size);
static NTSTATUS memx_flash_download(PDEVICE_CONTEXT DeviceContext, WDFREQUEST Request, const PUCHAR FirmwareBuffer, ULONG FirmwareSize);
static NTSTATUS memx_seperate_dfp_download(PDEVICE_CONTEXT DeviceContext, WDFREQUEST Request, const PUCHAR buffer, UCHAR dfp_src);
static NTSTATUS _host_trigger_admcmd(WDFREQUEST Request, PDEVICE_CONTEXT pDevContext, PVOID data);
static NTSTATUS _get_device_feature(WDFREQUEST Request, PDEVICE_CONTEXT pDevContext, PVOID data, PBOOLEAN complete_request);
static NTSTATUS _admin_command_handler(WDFREQUEST Request, PDEVICE_CONTEXT devContext, PVOID data, PBOOLEAN complete_request);
static NTSTATUS _set_device_feature(WDFREQUEST Request, PDEVICE_CONTEXT pDevContext, PVOID data, PBOOLEAN complete_request);
static NTSTATUS memx_usb_read_hw_info(PDEVICE_CONTEXT DeviceContext, WDFREQUEST Request, struct fw_hw_info_pkt *hw_info);
static void memx_usb_update_mpu_groups_info(struct hw_info *hw_info);

#ifdef ALLOC_PRAGMA
    #pragma alloc_text(PAGE, abort_all_request_on_pipe)
    #pragma alloc_text(PAGE, FW_Recv)
    #pragma alloc_text(PAGE, FW_Send)
    #pragma alloc_text(PAGE, clear_fw_id)
    #pragma alloc_text(PAGE, reset_mpu)
    #pragma alloc_text(PAGE, set_dfp_chip_id)
    #pragma alloc_text(PAGE, set_wtmem_addr)
    #pragma alloc_text(PAGE, set_wtmem_size)
    #pragma alloc_text(PAGE, memx_flash_download)
    #pragma alloc_text(PAGE, memx_seperate_dfp_download)
    #pragma alloc_text(PAGE, MemxEvtDeviceFileCreate)
    #pragma alloc_text(PAGE, MemxEvtDeviceFileClose)
    #pragma alloc_text(PAGE, MemxEvtIoRead)
    #pragma alloc_text(PAGE, MemxEvtIoWrite)
    #pragma alloc_text(PAGE, MemxEvtIoDeviceControl)
    #pragma alloc_text(PAGE, memx_usb_init_hw_info)
#endif

NTSTATUS abort_all_request_on_pipe(WDFDEVICE Device)
/*++

Routine Description:

    This routine calls WdfUsbTargetPipeAbortSynchronously to all host request on pipe

Arguments:

    Device - Handle to a framework device

Return Value:

    NT status value

--*/
{
    NTSTATUS            status          = STATUS_SUCCESS;
    WDFUSBPIPE          pipe;
    PDEVICE_CONTEXT     pDeviceContext;
    UCHAR               idx;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");
    pDeviceContext = GetDeviceContext(Device);

    WdfWaitLockAcquire(pDeviceContext->read_lock, NULL);

    if (NT_SUCCESS(WdfUsbTargetDeviceIsConnectedSynchronous(pDeviceContext->WdfUsbTargetDevice))) {
        for (idx = 0; idx < pDeviceContext->NumberConfiguredPipes; idx++) {
            pipe = WdfUsbInterfaceGetConfiguredPipe(pDeviceContext->UsbInterface, idx, NULL);
            status = WdfUsbTargetPipeAbortSynchronously(pipe, WDF_NO_HANDLE, NULL);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "%!FUNC! - abort pipe fail #%d\n", idx);
            }
        }

    }

    pDeviceContext->is_read_abort = 1;

    WdfWaitLockRelease(pDeviceContext->read_lock);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");
    return status;
}

NTSTATUS FW_Send(PDEVICE_CONTEXT DeviceContext, WDFREQUEST Request, PVOID buf, ULONG buf_len)
{
    PAGED_CODE();
    NTSTATUS                    status          = STATUS_SUCCESS;
    WDF_REQUEST_REUSE_PARAMS    params;
    WDF_REQUEST_SEND_OPTIONS    Options;
    WDFUSBPIPE                  pipe            = DeviceContext->FwWritePipe;
    WDFMEMORY                   urbMemory       = DeviceContext->FwUrbMemory;
    PURB                        urb             = DeviceContext->FwUrb;
    USBD_PIPE_HANDLE            usbdPipeHandle  = WdfUsbTargetPipeWdmGetPipeHandle(pipe);

    KeWaitForSingleObject(&DeviceContext->ConfigMutex, Executive, KernelMode, FALSE, NULL);

    if (NULL == buf) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "%!FUNC! NULL buffer\n");
        status = STATUS_UNSUCCESSFUL;
        goto done;
    }

    WDF_REQUEST_REUSE_PARAMS_INIT(&params, WDF_REQUEST_REUSE_NO_FLAGS, STATUS_SUCCESS);
    status = WdfRequestReuse(Request, &params);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "WdfRequestReuse failed\n");
        goto done;
    }

    UsbBuildInterruptOrBulkTransferRequest(urb, sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER), usbdPipeHandle, buf, NULL, buf_len, USBD_TRANSFER_DIRECTION_OUT, NULL);
    status = WdfUsbTargetPipeFormatRequestForUrb(pipe, Request, urbMemory, NULL);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "WdfUsbTargetPipeFormatRequestForUrb failed\n");
        goto done;
    }

    WDF_REQUEST_SEND_OPTIONS_INIT(&Options, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&Options, WDF_REL_TIMEOUT_IN_SEC(10)); // 10 sec timeout
    if (WdfRequestSend(Request, WdfUsbTargetPipeGetIoTarget(pipe), &Options) == FALSE) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "WdfRequestSend failed\n");
        status = WdfRequestGetStatus(Request);
        goto done;
    }
done:
    KeReleaseMutex(&DeviceContext->ConfigMutex, FALSE);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE,"%!FUNC! status 0x%x len %6d \n", status, buf_len);
    return status;
}

NTSTATUS FW_Recv(PDEVICE_CONTEXT DeviceContext, WDFREQUEST Request, PVOID buf, ULONG buf_len)
{
    PAGED_CODE();
    NTSTATUS                    status          = STATUS_SUCCESS;
    WDF_REQUEST_REUSE_PARAMS    params;
    WDF_REQUEST_SEND_OPTIONS    Options;
    WDFUSBPIPE                  pipe            = DeviceContext->FwReadPipe;
    WDFMEMORY                   urbMemory       = DeviceContext->FwUrbMemory;
    PURB                        urb             = DeviceContext->FwUrb;
    USBD_PIPE_HANDLE            usbdPipeHandle  = WdfUsbTargetPipeWdmGetPipeHandle(pipe);

    KeWaitForSingleObject(&DeviceContext->ConfigMutex, Executive, KernelMode, FALSE, NULL);

    if (NULL == buf) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "%!FUNC! NULL buffer\n");
        status = STATUS_UNSUCCESSFUL;
        goto done;
    }

    WDF_REQUEST_REUSE_PARAMS_INIT(&params, WDF_REQUEST_REUSE_NO_FLAGS, STATUS_SUCCESS);
    status = WdfRequestReuse(Request, &params);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "WdfRequestReuse failed\n");
        goto done;
    }

    UsbBuildInterruptOrBulkTransferRequest(urb, sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER), usbdPipeHandle, buf, NULL, buf_len, USBD_TRANSFER_DIRECTION_IN, NULL);
    status = WdfUsbTargetPipeFormatRequestForUrb(pipe, Request, urbMemory, NULL);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "WdfUsbTargetPipeFormatRequestForUrb fail\n");
        goto done;
    }

    WDF_REQUEST_SEND_OPTIONS_INIT(&Options, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&Options, WDF_REL_TIMEOUT_IN_SEC(10)); // 10 sec timeout
    if (WdfRequestSend(Request, WdfUsbTargetPipeGetIoTarget(pipe), &Options) == FALSE) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "WdfRequestSend failed\n");
        status = WdfRequestGetStatus(Request);
        goto done;
    }
done:
    KeReleaseMutex(&DeviceContext->ConfigMutex, FALSE);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE,"%!FUNC! status 0x%x len %6d \n", status, buf_len);
    return status;
}

NTSTATUS FW_SendCfgHeader(PDEVICE_CONTEXT DeviceContext, WDFREQUEST Request, ULONG FwCfgCmd, ULONG FwCfgCmdPayloadSize)
{
    ULONG cfg_header[2] = { 0 };
    cfg_header[0] = FwCfgCmd;
    cfg_header[1] = FwCfgCmdPayloadSize;
    return FW_Send(DeviceContext, Request, cfg_header, FW_CFG_HEADER_SIZE);
}

NTSTATUS FW_ReadChip0Sram(PDEVICE_CONTEXT DeviceContext, WDFREQUEST Request, ULONG address, ULONG size, PVOID buf)
{
    ULONG cfg_header[4] = { 0 };
    cfg_header[0] = DBGFS_ID_RDADDR;
    cfg_header[1] = 0;
    cfg_header[2] = address;
    cfg_header[3] = size;
    NTSTATUS status = FW_Send(DeviceContext, Request, cfg_header, 16);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "FW_ReadChip0Sram fail\n");
        return status;
    }
    status = FW_Recv(DeviceContext, Request, buf, size);
    return status;
}

static NTSTATUS memx_usb_read_hw_info(PDEVICE_CONTEXT DeviceContext, WDFREQUEST Request, struct fw_hw_info_pkt *hw_info)
{
    ULONG reg_addr = MEMX_PCIE_CMD_BUF_STARTADDR;
    ULONG read_size = sizeof(*hw_info);
    NTSTATUS status;

    status = FW_SendCfgHeader(DeviceContext, Request, FWCFG_ID_RREG_ADR, 4);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "FW_SendCfgHeader RREG_ADR failed\n");
        return status;
    }
    status = FW_Send(DeviceContext, Request, &reg_addr, 4);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "FW_Send RREG_ADR failed\n");
        return status;
    }
    clear_fw_id(DeviceContext, Request);

    status = FW_SendCfgHeader(DeviceContext, Request, FWCFG_ID_RREG, 4);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "FW_SendCfgHeader RREG failed\n");
        return status;
    }
    status = FW_Send(DeviceContext, Request, &read_size, 4);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "FW_Send RREG size failed\n");
        return status;
    }
    clear_fw_id(DeviceContext, Request);

    status = FW_Recv(DeviceContext, Request, hw_info, read_size);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "FW_Recv hw_info failed\n");
        return status;
    }

    return STATUS_SUCCESS;
}

static void memx_usb_update_mpu_groups_info(struct hw_info *hw_info)
{
    UCHAR curr_mpu_group_id = 0;
    UCHAR curr_chip_count = 0;
    UCHAR chip_id;

    RtlZeroMemory(hw_info->chip.groups, sizeof(hw_info->chip.groups));

    for (chip_id = 0; chip_id < MAX_SUPPORT_CHIP_NUM; chip_id++) {
        switch (hw_info->chip.roles[chip_id]) {
        case ROLE_SINGLE:
            hw_info->chip.groups[curr_mpu_group_id].input_chip_id = chip_id;
            hw_info->chip.groups[curr_mpu_group_id].output_chip_id = chip_id;
            curr_mpu_group_id++;
            curr_chip_count++;
        break;
        case ROLE_MULTI_FIRST:
            hw_info->chip.groups[curr_mpu_group_id].input_chip_id = chip_id;
            curr_chip_count++;
        break;
        case ROLE_MULTI_LAST:
            hw_info->chip.groups[curr_mpu_group_id].output_chip_id = chip_id;
            curr_mpu_group_id++;
            curr_chip_count++;
        break;
        case ROLE_MULTI_MIDDLE:
            curr_chip_count++;
        break;
        default:
        break;
        }
    }

    hw_info->chip.group_count = curr_mpu_group_id;
    hw_info->chip.curr_config_chip_count = curr_chip_count;
}

NTSTATUS memx_usb_init_hw_info(PDEVICE_CONTEXT DeviceContext)
{
    NTSTATUS status = STATUS_SUCCESS;
    WDFREQUEST request = NULL;
    struct fw_hw_info_pkt fw_info;

    PAGED_CODE();

    status = WdfRequestCreate(WDF_NO_OBJECT_ATTRIBUTES,
        WdfUsbTargetPipeGetIoTarget(DeviceContext->FwWritePipe), &request);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfRequestCreate failed 0x%x\n", status);
        return status;
    }

    RtlZeroMemory(&fw_info, sizeof(fw_info));
    status = memx_usb_read_hw_info(DeviceContext, request, &fw_info);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "memx_usb_read_hw_info failed 0x%x\n", status);
        WdfObjectDelete(request);
        return status;
    }

    RtlZeroMemory(&DeviceContext->hw_info, sizeof(DeviceContext->hw_info));
    DeviceContext->hw_info.chip.total_chip_cnt = (UCHAR)(fw_info.total_chip_cnt & 0xff);
    DeviceContext->hw_info.chip.generation = (UCHAR)fw_info.chip_generation;
    for (int chip_id = 0; chip_id < MAX_SUPPORT_CHIP_NUM; chip_id++) {
        DeviceContext->hw_info.chip.roles[chip_id] = fw_info.chip_role[chip_id];
    }
    memx_usb_update_mpu_groups_info(&DeviceContext->hw_info);

    WdfObjectDelete(request);
    return STATUS_SUCCESS;
}

void clear_fw_id(PDEVICE_CONTEXT DeviceContext, WDFREQUEST Request)
{
    PAGED_CODE();
    ULONG temp;
    FW_SendCfgHeader(DeviceContext, Request, FWCFG_ID_CLR, 0);
    FW_Send(DeviceContext, Request, &temp, 0);
}

void reset_mpu(PDEVICE_CONTEXT DeviceContext, WDFREQUEST Request, ULONG Id)
{
    PAGED_CODE();
    FW_SendCfgHeader(DeviceContext, Request, FWCFG_ID_DFP_RESETMPU, 4);
    FW_Send(DeviceContext, Request, &Id, 4);
    clear_fw_id(DeviceContext, Request);
}

void set_dfp_chip_id(PDEVICE_CONTEXT DeviceContext, WDFREQUEST Request, ULONG Id)
{
    PAGED_CODE();
    FW_SendCfgHeader(DeviceContext, Request, FWCFG_ID_DFP_CHIPID, 4);
    FW_Send(DeviceContext, Request, &Id, 4);
    clear_fw_id(DeviceContext, Request);
}

void set_wtmem_addr(PDEVICE_CONTEXT DeviceContext, WDFREQUEST Request, ULONG Id, ULONG Addr)
{
    PAGED_CODE();
    FW_SendCfgHeader(DeviceContext, Request, Id, 4);
    FW_Send(DeviceContext, Request, &Addr, 4);
    clear_fw_id(DeviceContext, Request);
}

void set_wtmem_size(PDEVICE_CONTEXT DeviceContext, WDFREQUEST Request, ULONG Size)
{
    PAGED_CODE();
    FW_SendCfgHeader(DeviceContext, Request, FWCFG_ID_DFP_WTMEMSZ, Size);
}

NTSTATUS memx_flash_download(PDEVICE_CONTEXT DeviceContext, WDFREQUEST Request, const PUCHAR FirmwareBuffer, ULONG FirmwareSize)
{
    PAGED_CODE();
    NTSTATUS    status                      = STATUS_SUCCESS;
    PUCHAR      firmware_buffer_pos         = FirmwareBuffer;
    ULONG       remaining_size_need_to_send = FirmwareSize;

    status = FW_SendCfgHeader(DeviceContext, Request, FWCFG_ID_FW, FirmwareSize);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "FW_SendCfgHeader failed\n");
        goto done;
    }

    while (remaining_size_need_to_send > 0) {
        ULONG transferred_size = (remaining_size_need_to_send > MAX_OPS_SIZE) ? MAX_OPS_SIZE :remaining_size_need_to_send;
        status = FW_Send(DeviceContext, Request, firmware_buffer_pos, transferred_size);
        if (!NT_SUCCESS(status)) {
            goto done;
        }
        firmware_buffer_pos         += transferred_size;
        remaining_size_need_to_send -= transferred_size;
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "firmware download %ld/%ld status %x\n", FirmwareSize - remaining_size_need_to_send, FirmwareSize, status);
    }
    ULONG result = 0;
    status = FW_Recv(DeviceContext, Request, &result, 4);
    if (result != 0) {
        status = STATUS_UNSUCCESSFUL;
    }
done:
    clear_fw_id(DeviceContext, Request);

    return status;
}

NTSTATUS memx_seperate_dfp_download(PDEVICE_CONTEXT DeviceContext, WDFREQUEST Request, const PUCHAR buffer, UCHAR dfp_src)
{
    PAGED_CODE();
    NTSTATUS    status              = STATUS_SUCCESS;
    PUCHAR      dfp_len_buf         = NULL;
    PUCHAR      dfp_wtmem_count_buf = NULL;
    PUCHAR      dfp_wtmem_size_buf  = NULL;
    PUCHAR      dfp_cfg_addr        = NULL;
    PUCHAR      dfp_wtmem_addr      = NULL;
    ULONG       total_length        = 0;
    ULONG       reg_write_addr      = 0;
    ULONG       cfg_size            = 0, wtmem_count = 0, wtmem_sze = 0;
    ULONG       dfp_id              = 0;
    ULONG       dfp_count           = 0;
    ULONG       i                   = 0, j = 0;

    dfp_count = *(PULONG)(buffer + SEP_LEN_OFS);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "dfp_count %x \n", dfp_count);
    // Skip DFP length field
    dfp_len_buf = (PVOID)(buffer+ SEP_LEN_OFS);

    /*program each dfp data*/
    for (i = 0; i < dfp_count; i++) {

        dfp_len_buf += (total_length + SEP_NEXT_OFS);

        /*read dfp id value*/
        dfp_id = *(PULONG)dfp_len_buf;
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "DFP id %d \n", dfp_id);

        dfp_len_buf += SEP_NEXT_OFS;
        /*read total length value*/
        total_length = *(PULONG)dfp_len_buf;
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "total_length  %d \n", total_length);


        set_dfp_chip_id(DeviceContext, Request, dfp_id);

        if (dfp_src == DFP_FROM_SEPERATE_CONFIG) {
            /*Remove reset flow to reduce model swap time*/
            /*reset_mpu(data, i + MPU_CHIP_ID_BASE);*/
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "DFP_FROM_SEPERATE_CONFIG\n");

            cfg_size = total_length;
            dfp_cfg_addr = dfp_len_buf + 4;

            status = FW_SendCfgHeader(DeviceContext, Request, FWCFG_ID_DFP_CFGSIZE, cfg_size);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "FW_SendCfgHeader failed\n");
                return  status;
            }

            while (cfg_size > 0) {
                if (cfg_size > MAX_CFG_SZ) {
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "cfg_size %d \n", cfg_size);
                    status = FW_Send(DeviceContext, Request, dfp_cfg_addr, MAX_CFG_SZ);
                    if (!NT_SUCCESS(status))
                        return status;
                    dfp_cfg_addr += MAX_CFG_SZ;
                    cfg_size -= MAX_CFG_SZ;
                }
                else {
                    status = FW_Send(DeviceContext, Request, dfp_cfg_addr, cfg_size);
                    if(!NT_SUCCESS(status))
                        return  status;
                    cfg_size = 0;
                    // Add ZLP to avoid flow failed for extra USB header.
                    FW_Send(DeviceContext, Request, dfp_cfg_addr, 0);
                }
            }
            clear_fw_id(DeviceContext, Request);
        }
        else {
            ULONG wtmem_data[2] = {0};
            dfp_wtmem_count_buf = dfp_len_buf + 4;

            /*program wt_mem*/
            wtmem_count = *(PULONG) dfp_wtmem_count_buf;
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "wtmem_count %d \n", wtmem_count);

            if (wtmem_count == 0) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "invalid wtmem count %d\r\n", wtmem_count);
                return status;
            }

            dfp_wtmem_size_buf = dfp_wtmem_count_buf + 4;

            for (j = 0; j < wtmem_count; j++) {
                RtlCopyMemory(wtmem_data, dfp_wtmem_size_buf, 8);

                wtmem_sze = wtmem_data[0] - 8;
                reg_write_addr = wtmem_data[1];
                dfp_wtmem_addr = dfp_wtmem_size_buf + 8;

                set_wtmem_addr(DeviceContext, Request, FWCFG_ID_DFP_WMEMADR, reg_write_addr);
                set_wtmem_size(DeviceContext, Request, wtmem_sze);
                while (wtmem_sze > 0) {
                    if (wtmem_sze > MAX_CFG_SZ) {
                        status = FW_Send(DeviceContext, Request, dfp_wtmem_addr, MAX_CFG_SZ);
                        if (!NT_SUCCESS(status))
                            return status;

                        dfp_wtmem_addr += MAX_CFG_SZ;

                        wtmem_sze -= MAX_CFG_SZ;
                    }
                    else {
                        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "WTMEM sze:%ld , max cfg sz:%ld\r\n", wtmem_sze, MAX_CFG_SZ);
                        status = FW_Send(DeviceContext, Request, dfp_wtmem_addr, wtmem_sze);
                        if (!NT_SUCCESS(status))
                            return status;
                        wtmem_sze = 0;
                        // Add ZLP to avoid flow failed for extra USB header.
                        FW_Send(DeviceContext, Request, dfp_wtmem_addr, 0);
                    }
                }
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "WTMEM clear_fw_id\n");

                clear_fw_id(DeviceContext, Request);

                dfp_wtmem_size_buf += wtmem_data[0] + 4;
            }
        }
        dfp_len_buf += 4; /*0xFFFFFFFF for the end of one chip's config*/
        /*Remove recfg flow to reduce model swap time*/
    }
    return status;
}

NTSTATUS _host_trigger_admcmd(WDFREQUEST Request, PDEVICE_CONTEXT pDevContext, PVOID data){
    NTSTATUS                status      = STATUS_SUCCESS;
    struct transport_cmd    *pCmd       = (struct transport_cmd *) data;
    ULONG                   FwCfgCmd    = 0;

    if(pCmd->SQ.opCode == MEMX_ADMIN_CMD_SET_FEATURE){
        FwCfgCmd = FWCFG_ID_SET_FEATURE; //backward
    } else if (pCmd->SQ.opCode == MEMX_ADMIN_CMD_GET_FEATURE){
        FwCfgCmd = FWCFG_ID_GET_FEATURE; //backward
    } else {
        FwCfgCmd = FWCFG_ID_ADM_COMMAND;
    }
    status = FW_SendCfgHeader(pDevContext, Request, FwCfgCmd, sizeof(struct transport_cmd));
    status = FW_Send(pDevContext, Request, data, sizeof(struct transport_cmd));
    clear_fw_id(pDevContext, Request);
    if(!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "FW_SendCfgHeader failed\n");
    } else {
        status = FW_Recv(pDevContext, Request, data, sizeof(struct transport_cmd));
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "FW_Recv failed\n");
        } else {
            struct transport_cmd *cmd = (struct transport_cmd *) data;
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "OpCode 0x%x subOpCode 0x%x CQ Status 0x%x\r\n", cmd->SQ.opCode, cmd->SQ.subOpCode, cmd->CQ.status);
        }
    }

    return status;
}

NTSTATUS _get_device_feature(WDFREQUEST Request, PDEVICE_CONTEXT pDevContext, PVOID data, PBOOLEAN complete_request){
    NTSTATUS                status      = STATUS_SUCCESS;
    struct transport_cmd    *pCmd       = (struct transport_cmd *) data;

    switch(pCmd->SQ.subOpCode){
        case FID_DEVICE_THROUGHPUT:     //Driver Only Admin Command
        case FID_DEVICE_INTERFACE_INFO: //Driver Only Admin Command
            break;
        case FID_DEVICE_MPU_UTILIZATION:
            status = FW_ReadChip0Sram(pDevContext, Request, (DEV_CODE_SRAM_MPUUTIL_BASE), 64, &pCmd->CQ.data[0]);
            break;
        default:
            status = _host_trigger_admcmd(Request, pDevContext, data);
            break;
    }

    *complete_request = (BOOLEAN)TRUE;

    return status;
}

NTSTATUS _set_device_feature(WDFREQUEST Request, PDEVICE_CONTEXT pDevContext, PVOID data, PBOOLEAN complete_request){
    NTSTATUS                status  = STATUS_SUCCESS;
    struct transport_cmd    *pCmd   = (struct transport_cmd *) data;

    switch(pCmd->SQ.subOpCode){
        default:
            status = _host_trigger_admcmd(Request, pDevContext, data);
            break;
    }

    *complete_request = (BOOLEAN)TRUE;

    return status;
}

NTSTATUS _admin_command_handler(WDFREQUEST Request, PDEVICE_CONTEXT devContext, PVOID data, PBOOLEAN complete_request){
    NTSTATUS                status  = STATUS_SUCCESS;
    struct transport_cmd    *pCmd   = (struct transport_cmd *) data;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER," %!FUNC!: Admin Op: 0x%08x SubOp 0x%08x\n", pCmd->SQ.opCode, pCmd->SQ.subOpCode);

    switch(pCmd->SQ.opCode){
        case MEMX_ADMIN_CMD_SET_FEATURE:
            status = _set_device_feature(Request, devContext, data, complete_request);
            break;
        case MEMX_ADMIN_CMD_GET_FEATURE:
            status = _get_device_feature(Request, devContext, data, complete_request);
            break;
        case MEMX_ADMIN_CMD_DEVIOCTRL:
            if (pCmd->SQ.subOpCode == FID_DEVICE_I2C_TRANSCEIVE) {
                pCmd->SQ.opCode = MEMX_ADMIN_CMD_SET_FEATURE;
                status = _set_device_feature(Request, devContext, data, complete_request);
                if (!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, " %!FUNC!: MEMX_ADMIN_CMD_DEVIOCTRL _set_device_feature\n");
                    break;
                }
                status = FW_ReadChip0Sram(devContext, Request, USB_HANDSHAKE_ADDR, sizeof(struct transport_cmd), pCmd);
                if (NT_SUCCESS(status)) {
                    RtlCopyMemory(&pCmd->CQ.data[3], &pCmd->SQ.cdw3, 16);
                }
            } else {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, " %!FUNC!: Not Support subOp: 0x%08x \n", pCmd->SQ.subOpCode);
            }
            break;
        case MEMX_ADMIN_CMD_DOWNLOAD_DFP: //usb interface not support
        case MEMX_ADMIN_CMD_SELFTEST:     //usb interface not support
        default:
            pCmd->CQ.status = ERROR_STATUS_OPCODE_NOT_SUPPORT_FAIL;
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER," %!FUNC!: Not Support Op: 0x%08x \n", pCmd->SQ.opCode);
            break;
    }

    return status;
}

VOID MemxEvtIoStop(WDFQUEUE Queue, WDFREQUEST Request, ULONG ActionFlags)
{
    UNREFERENCED_PARAMETER(Queue);

    if (ActionFlags & WdfRequestStopActionSuspend ) {
        WdfRequestStopAcknowledge(Request, FALSE); // Don't requeue
    }
    else if (ActionFlags & WdfRequestStopActionPurge) {
        WdfRequestCancelSentRequest(Request);
    }

    return;
}

VOID MemxEvtDeviceFileCreate(WDFDEVICE Device, WDFREQUEST Request, WDFFILEOBJECT FileObject)
{
    NTSTATUS            status = STATUS_SUCCESS;
    PUNICODE_STRING     fileName;
    PDEVICE_CONTEXT     pDevContext;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    pDevContext     = GetDeviceContext(Device);
    fileName        = WdfFileObjectGetFileName(FileObject);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Device File Name %wZ\n", fileName);

    WdfSpinLockAcquire(pDevContext->device_lock);

    if (pDevContext->reference_count == 0) {
        pDevContext->is_read_abort = 0;
    }

    pDevContext->reference_count++;

    WdfSpinLockRelease(pDevContext->device_lock);

    WdfRequestComplete(Request, status);
}

VOID MemxEvtDeviceFileClose(WDFFILEOBJECT FileObject)
{
    PUNICODE_STRING fileName;
    WDFDEVICE       device;
    PDEVICE_CONTEXT pDevContext;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    fileName        = WdfFileObjectGetFileName(FileObject);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "Device File Name %wZ\n", fileName);

    device = WdfFileObjectGetDevice(FileObject);

    if (device) {
        pDevContext = GetDeviceContext(device);
        WdfSpinLockAcquire(pDevContext->device_lock);
        pDevContext->reference_count--;
        WdfSpinLockRelease(pDevContext->device_lock);
    }
}

VOID MemxEvtUsbReadCompletion(WDFREQUEST Request, WDFIOTARGET Target, PWDF_REQUEST_COMPLETION_PARAMS Params, WDFCONTEXT Context)
{
    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Context);

    NTSTATUS status = Params->IoStatus.Status;
    ULONG_PTR readLength = 0;

    if (NT_SUCCESS(status)) {
        readLength = Params->Parameters.Usb.Completion->Parameters.PipeRead.Length;
    }

    WdfRequestCompleteWithInformation(Request, status, readLength);
}

VOID MemxEvtIoRead(WDFQUEUE Queue, WDFREQUEST Request, size_t Length)
{
    PAGED_CODE();
    NTSTATUS                status = STATUS_SUCCESS;
    WDFMEMORY               output_memory;
    PDEVICE_CONTEXT         pDevContext = NULL;
    WDF_REQUEST_SEND_OPTIONS options;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "--> %!FUNC!: Queue 0x%p, Request 0x%p Length %ld", Queue, Request, (ULONG)Length);
    pDevContext = GetDeviceContext(WdfIoQueueGetDevice(Queue));

    WdfWaitLockAcquire(pDevContext->read_lock, NULL);

    if (pDevContext->is_read_abort == 0) {
        status = WdfRequestRetrieveOutputMemory(Request, &output_memory);
        if (!NT_SUCCESS(status)) {
            WdfRequestComplete(Request, status);
            goto done;
        }

        status = WdfUsbTargetPipeFormatRequestForRead(pDevContext->BulkReadPipe, Request, output_memory, NULL);
        if (!NT_SUCCESS(status)) {
            WdfRequestComplete(Request, status);
            goto done;
        }

        WdfRequestSetCompletionRoutine(Request, MemxEvtUsbReadCompletion, pDevContext);

        WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_TIMEOUT);
        WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options, WDF_REL_TIMEOUT_IN_MS(10000));

        if (!WdfRequestSend(Request, WdfUsbTargetPipeGetIoTarget(pDevContext->BulkReadPipe), &options)) {
            status = WdfRequestGetStatus(Request);
            WdfRequestComplete(Request, status);
            goto done;
        }

    } else {
        pDevContext->is_read_abort = 0;
        WdfRequestComplete(Request, STATUS_CANCELLED);
    }

done:
    WdfWaitLockRelease(pDevContext->read_lock);
    return;
}

VOID MemxEvtIoWrite(WDFQUEUE Queue, WDFREQUEST Request, size_t Length)
{
    PAGED_CODE();
    NTSTATUS                    status              = STATUS_SUCCESS;
    WDF_MEMORY_DESCRIPTOR       memoryDescriptor;
    PDEVICE_CONTEXT             pDevContext         = NULL;
    PVOID                       writeBuffer         = NULL;
    ULONG                       bytesToWrite        = 0;
    ULONG                       totalBytesWritten   = 0;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "--> %!FUNC!: Queue 0x%p, Request 0x%p Length %ld", Queue, Request, (ULONG)Length);
    pDevContext = GetDeviceContext(WdfIoQueueGetDevice(Queue));

    status = WdfRequestRetrieveInputBuffer(Request, Length, &writeBuffer, NULL);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,"<-- %!FUNC!: WdfRequestRetrieveInputBuffer fail status 0x%x\n", status);
    } else {
        WDF_REQUEST_SEND_OPTIONS options;
        WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_TIMEOUT);
        WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options, WDF_REL_TIMEOUT_IN_MS(30000));
        //The specified pipe must be an output pipe, and the pipe's type must be WdfUsbPipeTypeBulk
        PUCHAR currentBufferPointer = (PUCHAR)writeBuffer;
        while (totalBytesWritten < (ULONG)Length) {
            bytesToWrite = min(MAX_MPUOUT_SIZE, (ULONG)Length - totalBytesWritten);
            //Avoid the tansfer size is multiple of MaximumPackSize, which will let kernel think transfer completed, however the windows internal driver didn't
            if(bytesToWrite && ((bytesToWrite & (pDevContext->BulkWriteMaximumPacketSize - 1)) == 0)){
                bytesToWrite -= DEF_BYTE(128);
            }
            WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memoryDescriptor, currentBufferPointer, bytesToWrite);
            //If you supply a NULL request handle, the framework uses an internal request object.
            //This technique is simple to use, but the driver cannot cancel the request.
            status = WdfUsbTargetPipeWriteSynchronously(pDevContext->BulkWritePipe, NULL, &options, &memoryDescriptor, &bytesToWrite);
            if (!NT_SUCCESS(status)) {
                break;
            } else {
                currentBufferPointer += bytesToWrite;
                totalBytesWritten += bytesToWrite;
            }
        }
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE,"<-- %!FUNC!: Write Data %d/%ld status 0x%x\n", totalBytesWritten, (ULONG)Length, status);
    }

    WdfRequestCompleteWithInformation(Request, status, totalBytesWritten);
}

VOID MemxEvtIoDeviceControl(WDFQUEUE Queue, WDFREQUEST Request, size_t OutputBufferLength, size_t InputBufferLength, ULONG IoControlCode)
{
    NTSTATUS        status          = STATUS_SUCCESS;
    WDFDEVICE       device;
    PVOID           ioBuffer;
    size_t          bufLength;
    PDEVICE_CONTEXT pDevContext;
    PFILE_CONTEXT   pFileContext;
    size_t          length          = 0;
    BOOLEAN         CompleteRequest = TRUE;
    UCHAR           *in_buf         = NULL;
    ULONG           temp;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "--> %!FUNC!");

    PAGED_CODE();

    device = WdfIoQueueGetDevice(Queue);
    pDevContext = GetDeviceContext(device);

    switch (IoControlCode) {
        case MEMX_USB_RESET_PIPE:
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "MEMX_USB_RESET_PIPE\n");
            pFileContext = GetFileContext(WdfRequestGetFileObject(Request));
            if (pFileContext->Pipe == NULL) {
                status = STATUS_INVALID_PARAMETER;
            }
            else {
                status = WdfUsbTargetPipeResetSynchronously(pFileContext->Pipe, WDF_NO_HANDLE, NULL);
            }
            break;
        case MEMX_USB_GET_CONFIG_DESCRIPTOR:
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "MEMX_USB_GET_CONFIG_DESCRIPTOR\n");
            if (pDevContext->UsbConfigurationDescriptor) {
                length = pDevContext->UsbConfigurationDescriptor->wTotalLength;
                status = WdfRequestRetrieveOutputBuffer(Request, length, &ioBuffer, &bufLength);
                if (!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "WdfRequestRetrieveInputBuffer failed\n");
                    break;
                }

                RtlCopyMemory(ioBuffer, pDevContext->UsbConfigurationDescriptor, length);
            }
            else {
                status = STATUS_INVALID_DEVICE_STATE;
            }
            break;
        case MEMX_ABORT_TRANSFER:
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "MEMX_ABORT_TRANSFER\n");
            status = abort_all_request_on_pipe(device);
            break;
        case MEMX_DOWNLOAD_FIRMWARE:
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "MEMX_DOWNLOAD_FIRMWARE\n");
            status = WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &in_buf, &bufLength);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfRequestRetrieveInputBuffer %u", status);
            }
            else {
                PULONG  plBuf = (PULONG)in_buf;
                ULONG  img_size = plBuf[0];
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Get Firmware Total Size %u Image Size %u CRC 0x%x MID 0x%x\r\n", (ULONG)InputBufferLength, img_size, plBuf[(InputBufferLength - 8) >> 2], plBuf[(InputBufferLength - 4) >> 2]);
                status = memx_flash_download(pDevContext, Request, in_buf, (ULONG)InputBufferLength);
            }
            break;
        case MEMX_RUNTIMEDWN_DFP:
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "MEMX_RUNTIMEDWN_DFP\n");
            if (!InputBufferLength) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "InputBufferLength invalid\n");
                break;
            }
            status = WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &in_buf, &bufLength);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "WdfRequestRetrieveInputBuffer failed\n");
                break;
            }
            struct memx_bin *memx_bin = (struct memx_bin *)in_buf;
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "IN BUF LEN %lld\n", bufLength);
            if (memx_bin->dfp_src == DFP_FROM_SEPERATE_CONFIG) {
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "DFP_FROM_SEPERATE_CONFIG \n");
            } else if (memx_bin->dfp_src == DFP_FROM_SEPERATE_WTMEM) {
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "DFP_FROM_SEPERATE_WTMEM \n");
            } else {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "Unsupport DFP source\n");
                break;
            }
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "memx_bin total size:%d \n", memx_bin->total_size);
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "dfp_cnt rgcfg %d\n", *(ULONG*)(memx_bin->buf + 12));
            status = memx_seperate_dfp_download(pDevContext, Request, memx_bin->buf, memx_bin->dfp_src);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "MEMX_RUNTIMEDWN_DFP failed\n");
                break;
            }
            break;
        case MEMX_IFMAP_FLOW:
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "MEMX_IFMAP_FLOW \n");
            status = WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &in_buf, &bufLength);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "WdfRequestRetrieveInputBuffer failed\n");
                break;
            }
            struct memx_flow *memx_flow = (struct memx_flow*)in_buf;

            if (memx_flow->flow_id > MEMX_TOTAL_FLOW_COUNT) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "Invalid flow id %d\n", memx_flow->flow_id);
                break;
            }
            pDevContext->flow_id = memx_flow->flow_id;
            break;
        case MEMX_DRIVER_MPU_IN_SIZE:
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "MEMX_DRIVER_MPU_IN_SIZE \n");
            status = WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &in_buf, &bufLength);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "WdfRequestRetrieveInputBuffer failed\n");
                break;
            }
            struct memx_mpu_size *memx_mpu_size = (struct memx_mpu_size*)in_buf;

            for (int i = 0; i < MEMX_TOTAL_FLOW_COUNT; i++) {
                if(memx_mpu_size->flow_size[i]) {
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Flow[%d]: size:%d, buf_size:%d \n", i, memx_mpu_size->flow_size[i], memx_mpu_size->buffer_size[i]);
                }
                pDevContext->flow_size[i]   = memx_mpu_size->flow_size[i];
                pDevContext->buffer_size[i] = memx_mpu_size->buffer_size[i];
            }

            pDevContext->usb_first_chip_pipeline_flag = memx_mpu_size->usb_first_chip_pipeline_flag;
            pDevContext->usb_last_chip_pingpong_flag = memx_mpu_size->usb_last_chip_pingpong_flag;

            FW_SendCfgHeader(pDevContext, Request, FWCFG_ID_MPU_INSIZE, 4 * (MEMX_TOTAL_FLOW_COUNT + 2));
            FW_Send(pDevContext, Request, pDevContext->buffer_size, 4 * (MEMX_TOTAL_FLOW_COUNT + 2));
            clear_fw_id(pDevContext, Request);
            break;
        case MEMX_GET_FWUPDATE_STATUS:
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "MEMX_GET_FWUPDATE_STATUS \n");
            if (!OutputBufferLength) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "OutputBufferLength invalid\n");
                break;
            }
            status = WdfRequestRetrieveOutputBuffer(Request, OutputBufferLength, &ioBuffer, &bufLength);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "WdfRequestRetrieveOutputBuffer failed\n");
                break;
            }
            struct memx_reg *memx_rreg = (struct memx_reg *)ioBuffer;
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "*(PULONG)(memx_rreg->buf_addr) %lx\n", *(PULONG)(memx_rreg->buf_addr));

            // using local buffer to prevent DRIVER_IRQL_NOT_LESS_OR_EQUAL issue
            ULONG fwupdate_status = 0;
            status = FW_Recv(pDevContext, Request, &fwupdate_status, 4);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "FW_Recv failed\n");
                break;
            }
            RtlCopyMemory((PULONG)(memx_rreg->buf_addr), (PULONG)&fwupdate_status, 4);
            break;
        case MEMX_GET_HW_INFO: {
            struct hw_info *out_hw_info = NULL;
            struct hw_info *in_hw_info = NULL;

            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "MEMX_GET_HW_INFO \n");
            if (!OutputBufferLength) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "OutputBufferLength invalid\n");
                break;
            }
            status = WdfRequestRetrieveOutputBuffer(Request, OutputBufferLength, &ioBuffer, &bufLength);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfRequestRetrieveOutputBuffer failed\n");
                break;
            }
            out_hw_info = (struct hw_info *)ioBuffer;

            if (InputBufferLength >= sizeof(struct hw_info)) {
                status = WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &in_buf, &bufLength);
                if (NT_SUCCESS(status)) {
                    in_hw_info = (struct hw_info *)in_buf;
                    pDevContext->hw_info.chip.generation = in_hw_info->chip.generation; // keep chip generation from user config
                }
            }

            RtlCopyMemory(out_hw_info, &pDevContext->hw_info, sizeof(struct hw_info));
            length = sizeof(struct hw_info);
            break;
        }
        case MEMX_SET_CHIP_ID:
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "MEMX_SET_CHIP_ID \n");
            if (!InputBufferLength) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "InputBufferLength invalid\n");
                break;
            }
            status = WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &in_buf, &bufLength);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "WdfRequestRetrieveInputBuffer failed\n");
                break;
            }
            struct memx_chip_id *memx_chip_id = (struct memx_chip_id *)in_buf;
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "set chip id: %d\n", memx_chip_id->chip_id);

            set_dfp_chip_id(pDevContext, Request, memx_chip_id->chip_id);
            break;
        case MEMX_READ_REG:
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "MEMX_READ_REG \n");
            if (!InputBufferLength) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "InputBufferLength invalid\n");
                break;
            }
            if (!OutputBufferLength) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "OutputBufferLength invalid\n");
                break;
            }
            status = WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &in_buf, &bufLength);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "WdfRequestRetrieveInputBuffer failed\n");
                break;
            }
            status = WdfRequestRetrieveOutputBuffer(Request, OutputBufferLength, &ioBuffer, &bufLength);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "WdfRequestRetrieveOutputBuffer failed\n");
                break;
            }
            struct memx_reg *in_memx_reg  = (struct memx_reg *)in_buf;
            struct memx_reg *out_memx_reg = (struct memx_reg *)ioBuffer;
            ULONG reg_addr = in_memx_reg->reg_start;
            ULONG read_size = in_memx_reg->size;
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Start Read reg: %lx, read size %ld\n", reg_addr, read_size);

            FW_SendCfgHeader(pDevContext, Request, FWCFG_ID_RREG_ADR, 4);
            FW_Send(pDevContext, Request, &reg_addr, 4);
            clear_fw_id(pDevContext, Request);

            FW_SendCfgHeader(pDevContext, Request, FWCFG_ID_RREG, 4);
            FW_Send(pDevContext, Request, &read_size, 4);
            clear_fw_id(pDevContext, Request);

            // using local buffer to prevent DRIVER_IRQL_NOT_LESS_OR_EQUAL issue
            PUCHAR result_buf = ExAllocatePool2(POOL_FLAG_NON_PAGED, read_size, POOL_TAG);
            if (result_buf == NULL) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "ExAllocatePool2 failed\n");
                status = STATUS_INVALID_PARAMETER;
                break;
            }
            status = FW_Recv(pDevContext, Request, result_buf, read_size);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "FW_Recv failed\n");
                ExFreePool(result_buf);
                break;
            }
            RtlCopyMemory((PULONG)(out_memx_reg->buf_addr), (PULONG)result_buf, read_size);
            ExFreePool(result_buf);
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Read reg: %lx, read size %ld done \n", reg_addr, read_size);
            break;
        case MEMX_CONFIG_MPU_GROUP:
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "MEMX_CONFIG_MPU_GROUP \n");
            status = WdfRequestRetrieveOutputBuffer(Request, OutputBufferLength, &ioBuffer, &bufLength);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "WdfRequestRetrieveOutputBuffer failed\n");
                break;
            }

            struct hw_info *hw_info = (struct hw_info *)ioBuffer;
            ULONG chip_roles[MAX_SUPPORT_CHIP_NUM] = { ROLE_UNCONFIGURED };
            for (int chip_id = 0; chip_id < MAX_SUPPORT_CHIP_NUM; chip_id++) {
                chip_roles[chip_id] = hw_info->chip.roles[chip_id];
            }

            FW_SendCfgHeader(pDevContext, Request, FWCFG_ID_MPU_GROUP, MAX_SUPPORT_CHIP_NUM * 4);
            FW_Send(pDevContext, Request, &chip_roles, MAX_SUPPORT_CHIP_NUM * 4);
            clear_fw_id(pDevContext, Request);
            memx_usb_update_mpu_groups_info(hw_info);
            RtlCopyMemory(&pDevContext->hw_info.chip, &hw_info->chip, sizeof(pDevContext->hw_info.chip));
            length = sizeof(struct hw_info);
            break;
        case MEMX_RESET_DEVICE:
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "MEMX_RESET_DEVICE \n");
            status = FW_SendCfgHeader(pDevContext, Request, FWCFG_ID_RESET_DEVICE, 4);
            status = FW_Send(pDevContext, Request, &temp, 4);
            clear_fw_id(pDevContext, Request);
            break;
        case MEMX_SET_THROUGHPUT_INFO:
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "MEMX_UPDATE_BANDWIDTH_INFO \n");
            break;
        case MEMX_GET_DEVICE_FEATURE: //backward
        case MEMX_SET_DEVICE_FEATURE: //backward
        case MEMX_ADMIN_COMMAND:
            status = WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &ioBuffer, &length);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfRequestRetrieveInputBuffer 0x%0x", status);
            }else{
                status = _admin_command_handler(Request, pDevContext, ioBuffer, &CompleteRequest);
            }
            break;
        case MEMX_WRITE_REG:
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "MEMX_WRITE_REG \n");
            if (!InputBufferLength) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "InputBufferLength invalid\n");
                break;
            }
            status = WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &in_buf, &bufLength);
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "WdfRequestRetrieveInputBuffer failed\n");
                break;
            }

            struct memx_reg *write_memx_reg  = (struct memx_reg *)in_buf;

            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Start Write reg: 0x%X, write size %ld\n", *(ULONG *)write_memx_reg->buf_addr, write_memx_reg->size);

            FW_SendCfgHeader(pDevContext, Request, FWCFG_ID_WREG, 8);
            FW_Send(pDevContext, Request, write_memx_reg->buf_addr, write_memx_reg->size);
            clear_fw_id(pDevContext, Request);
            break;
        default:
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }

    if (!NT_SUCCESS(status) || CompleteRequest) {
        WdfRequestCompleteWithInformation(Request, status, length);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE,"<-- %!FUNC!: IoControlCode 0x%x status 0x%x\n", IoControlCode, status);
    }
}
