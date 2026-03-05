/**
 * @file device.c
 * @author Gary Chang (gary.chang@memryx.com)
 * @brief This file contains the device entry points and callbacks.
 * @version 0.1
 * @date 2023
 *
 * @copyright Copyright (c) 2023 MemryX. Inc. All Rights Reserved.
 *
 */

#include "driver.h"
#include "device.tmh"

#ifdef ALLOC_PRAGMA
    #pragma alloc_text (PAGE, MemxUtilXflowDownloadFirmware)
#endif

static ULONG    GetBarModeOffset(PDEVICE_CONTEXT devContext, BOOLEAN is_vbuf_addr);
static NTSTATUS XflowCheck(PDEVICE_CONTEXT devContext, UCHAR bar_idx, UCHAR chip_id);
static NTSTATUS XflowSetAccessMode(PDEVICE_CONTEXT devContext, UCHAR chip_id, BOOLEAN access_mpu);
static NTSTATUS XflowSetBaseAddress(PDEVICE_CONTEXT devContext, UCHAR chip_id, ULONG base_addr);
static NTSTATUS XflowWriteVirtualBufferAddress(PDEVICE_CONTEXT devContext, UCHAR chip_id, ULONG base_addr_offset, ULONG value);
static ULONG    XflowReadVirtualBufferAddress(PDEVICE_CONTEXT devContext, UCHAR chip_id, ULONG base_addr_offset);
static void     A1ChipWorkAround(PDEVICE_CONTEXT devContext, ULONG token);

ULONG GetBarModeOffset(PDEVICE_CONTEXT devContext, BOOLEAN is_vbuf_addr) {
    ULONG result = (is_vbuf_addr == TRUE) ? devContext->BarInfo.XflowVbufOffset : devContext->BarInfo.XflowConfOffset;
    return result;
}

NTSTATUS XflowCheck(PDEVICE_CONTEXT devContext, UCHAR bar_idx, UCHAR chip_id)
{
    NTSTATUS status = STATUS_NOT_SUPPORTED;

    do {
        if (devContext == NULL)
        {
            break;
        }

        if (devContext->Bar[bar_idx].BaseAddress == 0)
        {
            break;
        }

        if (bar_idx > BAR5)
        {
            break;
        }

        if (chip_id >= CHIP_ID_MAX)
        {
            break;
        }

        status = STATUS_SUCCESS;
    } while (0);


    return status;
}

NTSTATUS XflowSetAccessMode(PDEVICE_CONTEXT devContext, UCHAR chip_id, BOOLEAN access_mpu)
{
    NTSTATUS    status                  = STATUS_SUCCESS;
    ULONGLONG   control_register_addr   = 0;
    UCHAR       bar_idx                 = devContext->BarInfo.XflowConfIdx;

    status = XflowCheck(devContext, bar_idx, chip_id);
    if (status != STATUS_SUCCESS)
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "XflowSetAccessMode basic check fail\n");
    }
    else
    {
        ULONG barModeOffset   = GetBarModeOffset(devContext, FALSE);
        if ((chip_id == 0) || (devContext->BarInfo.BarMode != MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM)) {
            control_register_addr = ( (ULONGLONG)(devContext->Bar[bar_idx].MappingAddress) + GET_XFLOW_OFFSET(chip_id, XFLOW_CONFIG_REG_PREFIX) + XFLOW_CONTROL_REGISTER_OFFSET - barModeOffset);
            WRITE_REGISTER_ULONG((volatile ULONG*) control_register_addr, access_mpu);
        } else {
            ULONGLONG indirect_base_addr_reg_addr              = 0;
            ULONGLONG indirect_control_register_addr           = 0;
            ULONGLONG indirect_virtual_buffer_target_address   = 0;
            ULONG      vbarmapofs = devContext->BarInfo.XflowVbufOffset;
            UCHAR      vbar_idx   = devContext->BarInfo.XflowVbufIdx;

            indirect_base_addr_reg_addr             = ((ULONGLONG)(devContext->Bar[bar_idx].MappingAddress)  + GET_XFLOW_OFFSET(CHIP_ID0, XFLOW_CONFIG_REG_PREFIX) + XFLOW_BASE_ADDRESS_REGISTER_OFFSET - barModeOffset);
            indirect_control_register_addr          = ((ULONGLONG)(devContext->Bar[bar_idx].MappingAddress)  + GET_XFLOW_OFFSET(CHIP_ID0, XFLOW_CONFIG_REG_PREFIX) + XFLOW_CONTROL_REGISTER_OFFSET - barModeOffset);
            indirect_virtual_buffer_target_address  = ((ULONGLONG)(devContext->Bar[vbar_idx].MappingAddress) + GET_XFLOW_OFFSET(CHIP_ID0, XFLOW_VIRTUAL_BUFFER_PREFIX) - vbarmapofs);

            //CHIP0 TO CONFIG_OUTPUT and Base Address
            WRITE_REGISTER_ULONG((volatile ULONG*)indirect_control_register_addr, TO_CONFIG_OUTPUT);
            WRITE_REGISTER_ULONG((volatile ULONG*)indirect_base_addr_reg_addr, MXCNST_RP_XFLOW_ADDR + GET_XFLOW_OFFSET(chip_id, XFLOW_CONFIG_REG_PREFIX) + XFLOW_CONTROL_REGISTER_OFFSET);
            //Set Target Chip Access Mode
            WRITE_REGISTER_ULONG((volatile ULONG*)indirect_virtual_buffer_target_address, access_mpu);
        }
    }

    return status;
}

NTSTATUS XflowSetBaseAddress(PDEVICE_CONTEXT devContext, UCHAR chip_id, ULONG base_addr)
{
    NTSTATUS    status                  = STATUS_SUCCESS;
    ULONGLONG   control_register_addr   = 0;
    UCHAR       bar_idx                 = devContext->BarInfo.XflowConfIdx;

    status = XflowCheck(devContext, bar_idx, chip_id);
    if (status != STATUS_SUCCESS)
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "XflowSetBaseAddress basic check fail\n");
    }
    else
    {
        ULONG barModeOffset = GetBarModeOffset(devContext, FALSE);
        if ((chip_id == 0) || (devContext->BarInfo.BarMode != MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM)) {
            control_register_addr = ((ULONGLONG)(devContext->Bar[bar_idx].MappingAddress) + GET_XFLOW_OFFSET(chip_id, XFLOW_CONFIG_REG_PREFIX) + XFLOW_BASE_ADDRESS_REGISTER_OFFSET - barModeOffset);
            WRITE_REGISTER_ULONG((volatile ULONG*)control_register_addr, base_addr);
        } else {
            ULONGLONG indirect_base_addr_reg_addr              = 0;
            ULONGLONG indirect_control_register_addr           = 0;
            ULONGLONG indirect_virtual_buffer_target_address   = 0;
            ULONG  vbarmapofs = devContext->BarInfo.XflowVbufOffset;
            UCHAR  vbar_idx = devContext->BarInfo.XflowVbufIdx;

            indirect_base_addr_reg_addr    = ((ULONGLONG)(devContext->Bar[bar_idx].MappingAddress) + GET_XFLOW_OFFSET(CHIP_ID0, XFLOW_CONFIG_REG_PREFIX) + XFLOW_BASE_ADDRESS_REGISTER_OFFSET - barModeOffset);
            indirect_control_register_addr = ((ULONGLONG)(devContext->Bar[bar_idx].MappingAddress) + GET_XFLOW_OFFSET(CHIP_ID0, XFLOW_CONFIG_REG_PREFIX) + XFLOW_CONTROL_REGISTER_OFFSET - barModeOffset);
            indirect_virtual_buffer_target_address = ((ULONGLONG)(devContext->Bar[vbar_idx].MappingAddress) + GET_XFLOW_OFFSET(CHIP_ID0, XFLOW_VIRTUAL_BUFFER_PREFIX) - vbarmapofs);

            //CHIP0 TO CONFIG_OUTPUT and Base Address
            WRITE_REGISTER_ULONG((volatile ULONG*)indirect_control_register_addr, TO_CONFIG_OUTPUT);
            WRITE_REGISTER_ULONG((volatile ULONG*)indirect_base_addr_reg_addr, MXCNST_RP_XFLOW_ADDR + GET_XFLOW_OFFSET(chip_id, XFLOW_CONFIG_REG_PREFIX) + XFLOW_BASE_ADDRESS_REGISTER_OFFSET);
            //Set Target Chip Base Address
            WRITE_REGISTER_ULONG((volatile ULONG*)indirect_virtual_buffer_target_address, base_addr);
        }
    }

    return status;
}

NTSTATUS XflowWriteVirtualBufferAddress(PDEVICE_CONTEXT devContext, UCHAR chip_id, ULONG base_addr_offset, ULONG value)
{
    NTSTATUS    status                          = STATUS_SUCCESS;
    ULONGLONG   virtual_buffer_target_address   = 0;
    UCHAR       bar_idx                         = devContext->BarInfo.XflowVbufIdx;
    ULONG       barmapofs                       = devContext->BarInfo.XflowVbufOffset;

    status = XflowCheck(devContext, bar_idx, chip_id);
    if (status != STATUS_SUCCESS)
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "MemxUtilXflowAccessVirtualBufferAddress basic check fail\n");
    }
    else
    {
        ULONG barModeOffset = GetBarModeOffset(devContext, TRUE);
        if ((chip_id == 0) || (devContext->BarInfo.BarMode != MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM)) {
            virtual_buffer_target_address = ((ULONGLONG)(devContext->Bar[bar_idx].MappingAddress) + GET_XFLOW_OFFSET(chip_id, XFLOW_VIRTUAL_BUFFER_PREFIX) + base_addr_offset - barModeOffset);
            WRITE_REGISTER_ULONG((volatile ULONG*)virtual_buffer_target_address, value);
        } else {
            ULONGLONG indirect_base_addr_reg_addr              = 0;
            ULONGLONG indirect_control_register_addr           = 0;
            ULONGLONG indirect_virtual_buffer_target_address   = 0;
            ULONG  cbarmapofs   = devContext->BarInfo.XflowConfOffset;
            UCHAR  cbar_idx     = devContext->BarInfo.XflowConfIdx;

            indirect_base_addr_reg_addr             = ((ULONGLONG)(devContext->Bar[cbar_idx].MappingAddress) + GET_XFLOW_OFFSET(CHIP_ID0, XFLOW_CONFIG_REG_PREFIX) + XFLOW_BASE_ADDRESS_REGISTER_OFFSET - cbarmapofs);
            indirect_control_register_addr          = ((ULONGLONG)(devContext->Bar[cbar_idx].MappingAddress) + GET_XFLOW_OFFSET(CHIP_ID0, XFLOW_CONFIG_REG_PREFIX) + XFLOW_CONTROL_REGISTER_OFFSET - cbarmapofs);
            indirect_virtual_buffer_target_address  = ((ULONGLONG)(devContext->Bar[bar_idx].MappingAddress)  + GET_XFLOW_OFFSET(CHIP_ID0, XFLOW_VIRTUAL_BUFFER_PREFIX) + base_addr_offset - barmapofs);

            //CHIP0 TO CONFIG_OUTPUT and Base Address
            WRITE_REGISTER_ULONG((volatile ULONG*)indirect_control_register_addr, TO_CONFIG_OUTPUT);
            WRITE_REGISTER_ULONG((volatile ULONG*)indirect_base_addr_reg_addr, MXCNST_RP_XFLOW_ADDR + GET_XFLOW_OFFSET(chip_id, XFLOW_VIRTUAL_BUFFER_PREFIX));
            //Set Target Chip Virtual Buffer
            WRITE_REGISTER_ULONG((volatile ULONG*)indirect_virtual_buffer_target_address, value);
        }
    }

    return status;
}

ULONG XflowReadVirtualBufferAddress(PDEVICE_CONTEXT devContext, UCHAR chip_id, ULONG base_addr_offset)
{
    ULONGLONG   virtual_buffer_target_address   = 0;
    UCHAR       bar_idx                         = devContext->BarInfo.XflowVbufIdx;
    ULONG       barmapofs                       = devContext->BarInfo.XflowVbufOffset;
    ULONG       val                             = 0xFFFFFFFF;

    if (XflowCheck(devContext, bar_idx, chip_id) != STATUS_SUCCESS)
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "XflowReadVirtualBufferAddress basic check fail\n");
    }
    else
    {
        ULONG barModeOffset = GetBarModeOffset(devContext, TRUE);
        if ((chip_id == 0) || (devContext->BarInfo.BarMode != MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM)) {
            virtual_buffer_target_address = ((ULONGLONG)(devContext->Bar[bar_idx].MappingAddress) + GET_XFLOW_OFFSET(chip_id, XFLOW_VIRTUAL_BUFFER_PREFIX) + base_addr_offset - barModeOffset);
            val = READ_REGISTER_ULONG((volatile ULONG*)virtual_buffer_target_address);
        } else {
            ULONGLONG indirect_base_addr_reg_addr              = 0;
            ULONGLONG indirect_control_register_addr           = 0;
            ULONGLONG indirect_virtual_buffer_target_address   = 0;
            ULONG  cbarmapofs = devContext->BarInfo.XflowConfOffset;
            UCHAR  cbar_idx = devContext->BarInfo.XflowConfIdx;

            indirect_base_addr_reg_addr             = ((ULONGLONG)(devContext->Bar[cbar_idx].MappingAddress) + GET_XFLOW_OFFSET(CHIP_ID0, XFLOW_CONFIG_REG_PREFIX) + XFLOW_BASE_ADDRESS_REGISTER_OFFSET - cbarmapofs);
            indirect_control_register_addr          = ((ULONGLONG)(devContext->Bar[cbar_idx].MappingAddress) + GET_XFLOW_OFFSET(CHIP_ID0, XFLOW_CONFIG_REG_PREFIX) + XFLOW_CONTROL_REGISTER_OFFSET - cbarmapofs);
            indirect_virtual_buffer_target_address  = ((ULONGLONG)(devContext->Bar[bar_idx].MappingAddress)  + GET_XFLOW_OFFSET(CHIP_ID0, XFLOW_VIRTUAL_BUFFER_PREFIX) + base_addr_offset - barmapofs);

            //CHIP0 TO CONFIG_OUTPUT and Base Address
            WRITE_REGISTER_ULONG((volatile ULONG*)indirect_control_register_addr, TO_CONFIG_OUTPUT);
            WRITE_REGISTER_ULONG((volatile ULONG*)indirect_base_addr_reg_addr, MXCNST_RP_XFLOW_ADDR + GET_XFLOW_OFFSET(chip_id, XFLOW_VIRTUAL_BUFFER_PREFIX));
            //Get Target Chip Virtual Buffer
            val = READ_REGISTER_ULONG((volatile ULONG*)indirect_virtual_buffer_target_address);
        }
    }

    return val;
}

NTSTATUS MemxUtilXflowBurstWriteWTMEM(PDEVICE_CONTEXT devContext, UCHAR chip_id, ULONG base_addr, UCHAR *data, ULONG write_size, BOOLEAN access_mpu)
{
    NTSTATUS status = STATUS_SUCCESS;

    do {
        if (write_size > XFLOW_MAX_BURST_WRITE_SIZE || write_size % XFLOW_BURST_WRITE_BATCH_SIZE) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,"memx_xflow_burst_write: invalid write size: %d \n", write_size);
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        UCHAR bar_idx       = devContext->BarInfo.XflowVbufIdx;
        ULONG barModeOffset = GetBarModeOffset(devContext, TRUE);

        status = XflowSetAccessMode(devContext, chip_id, access_mpu);
        if (status != STATUS_SUCCESS) { break; }

        status = XflowSetBaseAddress(devContext, chip_id, base_addr);
        if (status != STATUS_SUCCESS) { break; }

        // hack: wtmem are always 64 bytes-alignment
        ULONGLONG virtual_buffer_target_address;
        PULONG64 pllBuf = (PULONG64) (data);

        for (ULONG i = 0; i < (write_size >> 3); i++) {
            virtual_buffer_target_address = ((ULONGLONG)(devContext->Bar[bar_idx].MappingAddress) + GET_XFLOW_OFFSET(chip_id, XFLOW_VIRTUAL_BUFFER_PREFIX) + i * DEF_BYTE(8) - barModeOffset);
            WRITE_REGISTER_ULONG64((volatile ULONG64*)virtual_buffer_target_address, pllBuf[i]); // adjust UCHAR count to ULONG count
        }

        if(access_mpu == TO_CONFIG_OUTPUT){
            status = XflowSetAccessMode(devContext, chip_id, TO_MPU);
            if (status != STATUS_SUCCESS) { break; }
            else {
                if ((devContext->BarInfo.BarMode == MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM) && (chip_id > 0)) {
                    status = XflowSetAccessMode(devContext, chip_id, TO_MPU);
                    if (status != STATUS_SUCCESS) { break; }
                }
            }
        }
    } while (0);

    return status;
}

NTSTATUS MemxUtilXflowWrite(PDEVICE_CONTEXT devContext, UCHAR chip_id, ULONG base_addr, ULONG base_addr_offset, ULONG value, BOOLEAN access_mpu)
{
    NTSTATUS status = STATUS_SUCCESS;
    WdfSpinLockAcquire(devContext->XflowLockHandle[chip_id]);

    do
    {
        status = XflowSetAccessMode(devContext, chip_id, access_mpu);
        if (status != STATUS_SUCCESS)
        {
            break;
        }

        status = XflowSetBaseAddress(devContext, chip_id, base_addr);
        if (status != STATUS_SUCCESS)
        {
            break;
        }

        status = XflowWriteVirtualBufferAddress(devContext, chip_id, base_addr_offset, value);
        if(access_mpu == TO_CONFIG_OUTPUT){
            status = XflowSetAccessMode(devContext, chip_id, TO_MPU);
            if (status != STATUS_SUCCESS) { break; }
            else {
                if ((devContext->BarInfo.BarMode == MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM) && (chip_id > 0)) {
                    status = XflowSetAccessMode(devContext, chip_id, TO_MPU);
                    if (status != STATUS_SUCCESS) { break; }
                }
            }
        }
    } while (0);

    WdfSpinLockRelease(devContext->XflowLockHandle[chip_id]);

    return status;
}

ULONG MemxUtilXflowRead(PDEVICE_CONTEXT devContext, UCHAR chip_id, ULONG base_addr, ULONG base_addr_offset, BOOLEAN access_mpu)
{
    NTSTATUS status = STATUS_SUCCESS;
    ULONG val = 0xFFFFFFFF;
    WdfSpinLockAcquire(devContext->XflowLockHandle[chip_id]);

    do
    {
        status = XflowSetAccessMode(devContext, chip_id, access_mpu);
        if (status != STATUS_SUCCESS)
        {
            break;
        }

        status = XflowSetBaseAddress(devContext, chip_id, base_addr);
        if (status != STATUS_SUCCESS)
        {
            break;
        }

        val = XflowReadVirtualBufferAddress(devContext, chip_id, base_addr_offset);

        if(access_mpu == TO_CONFIG_OUTPUT){
            status = XflowSetAccessMode(devContext, chip_id, TO_MPU);
            if (status != STATUS_SUCCESS) { break; }
            else {
                if ((devContext->BarInfo.BarMode == MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM) && (chip_id > 0)) {
                    status = XflowSetAccessMode(devContext, chip_id, TO_MPU);
                    if (status != STATUS_SUCCESS) { break; }
                }
            }
        }
    } while (0);

    WdfSpinLockRelease(devContext->XflowLockHandle[chip_id]);

    return val;
}

void MemxUtilSramWrite(PDEVICE_CONTEXT devContext, ULONG axi_base_addr, ULONG value)
{
    ULONGLONG       sram_offset = 0;
    volatile ULONG* plChip0SramBuf = NULL;

    if ((axi_base_addr < DEV_CODE_SRAM_BASE) || (axi_base_addr >= (DEV_CODE_SRAM_BASE + DEV_CODE_SRAM_SIZE))) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, " %!FUNC!: invalid axi_base_addr: 0x%08x \n", axi_base_addr);
    }
    else {
        sram_offset = axi_base_addr - PHYSICAL_SRAM_BASE;
        plChip0SramBuf = (volatile ULONG*)((PUCHAR)devContext->Bar[devContext->BarInfo.SramIdx].MappingAddress + sram_offset);
        plChip0SramBuf[0] = value;
    }
}

ULONG MemxUtilSramRead(PDEVICE_CONTEXT devContext, ULONG axi_base_addr)
{
    ULONGLONG       sram_offset = 0;
    volatile ULONG* plChip0SramBuf = NULL;
    ULONG           result = 0xFFFFFFFF;

    if ((axi_base_addr < DEV_CODE_SRAM_BASE) || (axi_base_addr >= (DEV_CODE_SRAM_BASE + DEV_CODE_SRAM_SIZE))) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, " %!FUNC!: invalid axi_base_addr: 0x%08x \n", axi_base_addr);
    }
    else {
        sram_offset = axi_base_addr - PHYSICAL_SRAM_BASE;
        plChip0SramBuf = (volatile ULONG*)((PUCHAR)devContext->Bar[devContext->BarInfo.SramIdx].MappingAddress + sram_offset);
        result = plChip0SramBuf[0];
    }

    return result;
}

NTSTATUS MemxUtilSendFwCommand(PDEVICE_CONTEXT devContext, USHORT op_code, USHORT expected_payload_length, UCHAR chip_id)
{
    NTSTATUS status = STATUS_SUCCESS;
    pcie_fw_cmd_format_t* fw_cmd_buffer = NULL;
    do
    {
        if ((devContext == NULL) || (devContext->mmap_fw_command_buffer_base == NULL))
        {
            status = STATUS_NOT_SUPPORTED;
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "MemxUtilSendFwCommand NULL parameter\n");
            break;
        }

        if (expected_payload_length == 0)
        {
            status = STATUS_NOT_SUPPORTED;
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "MemxUtilSendFwCommand zero length\n");
            break;
        }

        fw_cmd_buffer = (pcie_fw_cmd_format_t*)devContext->mmap_fw_command_buffer_base;
        RtlZeroMemory(fw_cmd_buffer, sizeof(pcie_fw_cmd_format_t));

        fw_cmd_buffer->firmware_command = op_code;
        fw_cmd_buffer->expected_data_length = expected_payload_length;

        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "send cmd[0x%x] len[0x%x] to fw  buf[0x%I64X]\n",
                                                op_code, expected_payload_length, (ULONG_PTR)fw_cmd_buffer);

        switch (op_code)
        {
            case PCIE_CMD_INIT_HOST_BUF_MAPPING:
            {
                fw_cmd_buffer->data[0] = devContext->CommonBufferBaseDevice[devContext->BufMappingIdx].LowPart;
                fw_cmd_buffer->data[1] = devContext->CommonBufferBaseDevice[devContext->BufMappingIdx].HighPart;
                fw_cmd_buffer->data[2] = DEF_KB(512);
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "INIT_HOST_BUF_MAPPING %d Device Adr:[0x%x, 0x%x] Size:[0x%x] to fw\n",
                                                                                devContext->BufMappingIdx, fw_cmd_buffer->data[0], fw_cmd_buffer->data[1], fw_cmd_buffer->data[2]);
            }
            break;
            case PCIE_CMD_INIT_WTMEM_FMAP:
            {
                fw_cmd_buffer->data[0] = chip_id;
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "PCIE_CMD_INIT_WTMEM_FMAP send data0[0x%x] to fw\n", fw_cmd_buffer->data[0]);
            }
            break;
            case PCIE_CMD_RESET_MPU:
            {
                fw_cmd_buffer->data[0] = chip_id;
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "PCIE_CMD_RESET_MPU send data0[0x%x] to fw\n", fw_cmd_buffer->data[0]);
            }
            break;
            case PCIE_CMD_CONFIG_MPU_GROUP:
            {
                for (int i = 0; i < MAX_SUPPORT_CHIP_NUM; i++) {
                    fw_cmd_buffer->data[i] = devContext->hw_info.chip.roles[i];
                }
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "PCIE_CMD_CONFIG_MPU_GROUP send data to fw\n");
            }
            case PCIE_CMD_VENDOR_1:
            {
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "PCIE_CMD_VENDOR_1 send data to fw\n");
            }
            break;
        default:
            // don't need to modify data area.
            break;
        }

        // trigger mpu sw irq 4 to request mpu to process firmware command which host already put in command event buffer and
        // when fw write back to the same buffer in data area as event result, firmware will issue a msix to nitify udriver  the cmd process done.
        status = MemxPcieTriggerDeviceIrq(devContext, FW_CMD_DONE, CHIP_ID0);
    } while (0);

    return status;
}

void A1ChipWorkAround(PDEVICE_CONTEXT devContext, ULONG token){
    //this function used to resolve too many error message in ZSBL issue
    ULONG epsts = 0;
    if (token != 0) {
        epsts = MemxUtilXflowRead(devContext, CHIP_ID0, PCIE_EP0_LM_LOCAL_ERROR_STATUS, 0x0, TO_CONFIG_OUTPUT);
        if (epsts != 0) {
            for (ULONG i = 0; i < 64; i++) {
                MemxUtilXflowWrite(devContext, CHIP_ID0, PCIE_EP0_LM_LOCAL_ERROR_STATUS, 0x0, epsts, TO_CONFIG_OUTPUT);
            }
            TraceEvents(TRACE_LEVEL_WARNING, TRACE_DRIVER, "epsts = 0x%08x\n", epsts);
        }
    }
}

NTSTATUS MemxUtilXflowDownloadFirmware(PDEVICE_CONTEXT devContext){
    PAGED_CODE();
    NTSTATUS            status = STATUS_SUCCESS;
    UNICODE_STRING      uniName;
    OBJECT_ATTRIBUTES   objAttr;
    HANDLE              handle;
    IO_STATUS_BLOCK     ioStatusBlock;

    RtlInitUnicodeString(&uniName, GLOBAL_FIRMWARE_DEFUALT_PATH);
    InitializeObjectAttributes(&objAttr, &uniName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
        // Do not try to perform any file operations at higher IRQL levels. Instead, you may use a work item or a system worker thread to perform file operations.
        status = STATUS_INVALID_DEVICE_STATE;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "KeGetCurrentIrql fail irq %u\n", KeGetCurrentIrql());
    } else {
        ULONG            BUFFER_SIZE    = DEF_KB(128);
        PULONG           plBuffer       = NULL;;
        LARGE_INTEGER    byteOffset     = { .LowPart = 0, .HighPart = 0 };
        ULONG            token          = 0;
        ULONG            firmware_size  = 0;
        ULONG            imgFmt         = 0;
        ULONG            flag           = 0; //BIT0 : Close Handle BIT1: Free Buffer

        token = MemxUtilSramRead(devContext, DEV_CODE_SRAM_BASE);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "downlaod_rom_image: token = 0x%08x\n", token);

        if (token == FW_DEFAULT_VALUE) {
            devContext->deviceState = DEVICE_INITIAL;

            do{
                status = ZwCreateFile(&handle, GENERIC_READ, &objAttr, &ioStatusBlock, NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
                if(!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "ZwCreateFile fail status 0x%x Io status 0x%x \n", status, ioStatusBlock.Status);
                    break;
                } else {
                    flag |= BIT(0); //should close Handle
                }

                plBuffer = (PULONG)ExAllocatePool2(POOL_FLAG_PAGED, (SIZE_T)BUFFER_SIZE, 'mxfw');
                if(plBuffer == NULL) {
                    status = STATUS_UNSUCCESSFUL;
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "ExAllocatePool2 fail \n");
                    break;
                } else {
                    flag |= BIT(1); //should free Buffer
                }

                status = ZwReadFile(handle, NULL, NULL, NULL, &ioStatusBlock, plBuffer, BUFFER_SIZE, &byteOffset, NULL);
                if(!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "ZwReadFile fail status 0x%x Io status 0x%x \n", status, ioStatusBlock.Status);
                    break;
                }

                if (plBuffer[(MEMX_FW_IMGFMT_OFFSET >> 2)] == 1) {
                    imgFmt = 1;
                    firmware_size = (MEMX_FSBL_SECTION_SIZE) + MEMX_FW_IMGSIZE_LEN + plBuffer[(MEMX_FW_IMGSIZE_OFFSET >> 2)] + MEMX_FW_IMGCRC_LEN;
                    if ((firmware_size + DEF_BYTE(8)) <= BUFFER_SIZE) {
                        plBuffer[(firmware_size) >> 2] = plBuffer[0];  //Padding FSBL Length
                        plBuffer[(firmware_size + 4) >> 2] = imgFmt;   //Padding ImgFmt
                        firmware_size = firmware_size - DEF_BYTE(4) + DEF_BYTE(8); //skip FSBL Length and plus two padding
                        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "ZwReadFile byteOffset %x FSBL size %d firmware size %d\n", byteOffset.LowPart, plBuffer[0], firmware_size);
                    } else {
                        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "ZwReadFile OverSize firmware size %d\n", firmware_size);
                        status = STATUS_UNSUCCESSFUL;
                        break;
                    }
                } else {
                    imgFmt = 0;
                    firmware_size = plBuffer[0]; //first 4 byte bin is size
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "ZwReadFile byteOffset %x firmware size %d\n", byteOffset.LowPart, firmware_size);
                }

                MemxUtilSramWrite(devContext, DEV_CODE_SRAM_BASE, firmware_size);
                do { //Waiting for size value to be cleared by EP...
                    token = MemxUtilSramRead(devContext, DEV_CODE_SRAM_BASE);
                    A1ChipWorkAround(devContext, token);
                } while (token != 0);

                //round-up firmware size to multiple 4byte and start after firmware size info
                ULONG dwordCount = (BYTE_ROUND_UP(firmware_size, DEF_BYTE(4)) / DEF_BYTE(4));
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "downlaod_rom_image: loop_count(%d)\n", dwordCount);

                for (ULONG i = 0; i < dwordCount; i++) {
                    //+ 1 to skip header size info
                    MemxUtilSramWrite(devContext, DEV_CODE_SRAM_BASE + i * DEF_BYTE(4), plBuffer[i + 1]);
                }

                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "downlaod_rom_image: FW image written, wait for device ready\n");
            }while(0);

            if(flag & BIT(0)) {
                ZwClose(handle);
            }

            if(flag & BIT(1)) {
                ExFreePool(plBuffer);
            }
        } else {
            devContext->deviceState = DEVICE_FWDONE;
            status = MemxUtilSendFwCommand(devContext, PCIE_CMD_INIT_HOST_BUF_MAPPING, DEF_BYTE(8), CHIP_ID0);
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "downlaod_rom_image: FW image already existed, wait for device ready\n");
        }
    }

    return status;
}

NTSTATUS MemxPcieTriggerDeviceIrq(PDEVICE_CONTEXT devContext, XFLOW_MPU_SWIRQ_ID_T sw_irq_idx, UCHAR chip_id) {
    ULONG write_value = 0;

    NTSTATUS status = STATUS_SUCCESS;

	if (status != STATUS_SUCCESS) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "MemxPcieTriggerDeviceIrq: basic check fail.\n");
	} else {
		switch (sw_irq_idx) {
        case MPU_EGRESS_DCORE_DONE:
        case DUMP_XFLOW_ERRSTS:
        case RESERVE_ID2:
        case RESERVE_ID3:
        case FW_CMD_DONE:
        case MOVE_SRAMDATA_TO_DIPORT:
        case INIT_WTMEM_FMEM:
        case RESET_MPU:
			if (devContext->BarInfo.BarMode != MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM) {
				write_value = (0x1 << sw_irq_idx);
                status = MemxUtilXflowWrite(devContext, chip_id, AHB_HUB_IRQ_EN_BASE, 0x0, write_value, TO_MPU);
			} else {
                status = XflowCheck(devContext, devContext->BarInfo.DeviceIrqIdx, chip_id);
                if (status == STATUS_SUCCESS) {
                    volatile ULONG *device_irq_register_addr = NULL;
                    write_value = sw_irq_idx - 2;
                    device_irq_register_addr = (volatile ULONG*)((PUCHAR)devContext->Bar[devContext->BarInfo.DeviceIrqIdx].MappingAddress + MEMX_PCIE_IRQ_OFFSET(chip_id, write_value));
                    WRITE_REGISTER_ULONG(device_irq_register_addr, chip_id);
                }
			}
		break;
		default:
            status = STATUS_INVALID_PARAMETER;
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "MemxPcieTriggerDeviceIrq: Invalid sw_irq_idx(%u), it should not be used.\n", sw_irq_idx);
		}
	}

    return status;
}
