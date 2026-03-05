/**
 * @file queue.c
 * @author Gary Chang (gary.chang@memryx.com)
 * @brief This file contains the queue entry points and callbacks.
 * @version 0.1
 * @date 2023
 *
 * @copyright Copyright (c) 2023 MemryX. Inc. All Rights Reserved.
 *
 */

#include "driver.h"
#include "queue.tmh"

#ifdef ALLOC_PRAGMA
    #pragma alloc_text(PAGE, MemxEvtDeviceFileCreate)
    #pragma alloc_text(PAGE, MemxEvtDeviceFileClose)
#endif
#define SEP_NEXT_OFS                    (4)
#define MEMX_MPU_MAX_CORE_GROUP         (6)
#define PMU_MPU_PWR_MGMT_ADDR           (0x20000300)
#define ALL_MPU_CORE_GROUP_ON           (0x222222)
#define PMU_MPU_CORE_GROUP_MASK         (0xF)
#define PMU_MPU_CORE_GROUP_ON           (0x2)
#define PMU_MPU_CORE_GROUP_OFF          (0x1)
#define PMU_MPU_CORE_GROUP_ON_STATUS    (0x4)
#define PMU_MPU_CORE_GROUP_OFF_STATUS   (0x8)
#define PMU_MPU_CORE_GROUP_OFFSET       (0x4)

static void _feature_data_from_device(PDEVICE_CONTEXT devContext, UCHAR chip_id, struct transport_cmd* pCmd)
{
    ULONG index = 0;
    UCHAR read_data_start = 0;
    UCHAR read_data_end = 0;
    PULONG pAdminCmd = (PULONG)devContext->CommonBufferBaseDriver[BUFFER_IDX_DEBUG1];
    ULONG read_offset = U32_ADMCMD_CQ_DATA_OFFSET;

    if (pCmd->SQ.subOpCode == FID_DEVICE_THROUGHPUT) {
        read_data_start = (chip_id == CHIP_ID0) ? 0 : 4;
        read_data_end = (chip_id == CHIP_ID0) ? 4 : 8;
    }
    else {
        read_data_start = 0;
        read_data_end = 31;
    }

    if (pCmd->SQ.subOpCode == FID_DEVICE_I2C_TRANSCEIVE) {
        read_offset = 0;
    }

    for (index = read_data_start; index < read_data_end; index++) {
        if (chip_id < CHIP_ID_MAX) {
            pAdminCmd = (PULONG)(devContext->CommonBufferBaseDriver[BUFFER_IDX_DEBUG1] + (MEMX_ADMCMD_VIRTUAL_OFFSET - (DEF_KB(512) * 3)) + MEMX_ADMCMD_SIZE * chip_id);
        }

        pCmd->CQ.data[index] = READ_REGISTER_ULONG((volatile ULONG*) &pAdminCmd[read_offset + index]);
    }
}

static enum CASCADE_PLUS_ADMINCMD_ERROR_STATUS _feature_fetch_result(PDEVICE_CONTEXT devContext, UCHAR chip_id, struct transport_cmd* pCmd)
{
	enum CASCADE_PLUS_ADMINCMD_STATUS device_status = STATUS_IDLE;
	enum CASCADE_PLUS_ADMINCMD_ERROR_STATUS error_status = ERROR_STATUS_NO_ERROR;
    LARGE_INTEGER startTime, endTime, elapsedTime;
    PULONG pAdminCmd = (PULONG)devContext->CommonBufferBaseDriver[BUFFER_IDX_DEBUG1];
    if (chip_id < CHIP_ID_MAX) {
        pAdminCmd = (PULONG)(devContext->CommonBufferBaseDriver[BUFFER_IDX_DEBUG1] + (MEMX_ADMCMD_VIRTUAL_OFFSET - (DEF_KB(512) * 3)) + MEMX_ADMCMD_SIZE * chip_id);
    }

    KeQuerySystemTime(&startTime);
	do {
		device_status = READ_REGISTER_ULONG((volatile ULONG*)&pAdminCmd[U32_ADMCMD_STATUS_OFFSET]);
		if (device_status == STATUS_COMPLETE) {
			error_status = READ_REGISTER_ULONG((volatile ULONG*)&pAdminCmd[U32_ADMCMD_CQ_STATUS_OFFSET]);
            if (error_status != ERROR_STATUS_NO_ERROR) {
                TraceEvents(TRACE_LEVEL_WARNING, TRACE_DRIVER, " %!FUNC!: SubOp: 0x%08x Error 0x%x\n", pCmd->SQ.subOpCode, error_status);
            } else {
                _feature_data_from_device(devContext, chip_id, pCmd);
            }
        }
        else {
            KeQuerySystemTime(&endTime);
            elapsedTime.QuadPart = (endTime.QuadPart - startTime.QuadPart) / 10000;
            if (elapsedTime.QuadPart > 3000) { //sec
                error_status = ERROR_STATUS_TIMEOUT_FAIL;
                TraceEvents(TRACE_LEVEL_WARNING, TRACE_DRIVER, " %!FUNC!: SubOp: 0x%08x Timeout\n", pCmd->SQ.subOpCode);
                break;
            }
        }

	} while (device_status != STATUS_COMPLETE);
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, " %!FUNC!: Fetch Adm Cmd from Chip %d\n", chip_id);
    WRITE_REGISTER_ULONG((volatile ULONG*) &pAdminCmd[U32_ADMCMD_STATUS_OFFSET],  STATUS_IDLE);

	return error_status;
}

static void _host_trigger_admcmd(PDEVICE_CONTEXT devContext, UCHAR chip_id, struct transport_cmd* pCmd)
{
    PULONG pAdminCmd = (PULONG)devContext->CommonBufferBaseDriver[BUFFER_IDX_DEBUG1];
    if (chip_id < CHIP_ID_MAX) {
        pAdminCmd = (PULONG)(devContext->CommonBufferBaseDriver[BUFFER_IDX_DEBUG1] + (MEMX_ADMCMD_VIRTUAL_OFFSET - (DEF_KB(512) * 3)) + MEMX_ADMCMD_SIZE * chip_id);
    }
    UCHAR U64Count = (sizeof(struct transport_cmd) / DEF_BYTE(8));
    WRITE_REGISTER_BUFFER_ULONG64((volatile ULONG64*)pAdminCmd, (PULONG64)pCmd, U64Count);
    WRITE_REGISTER_ULONG((volatile ULONG*) &pAdminCmd[U32_ADMCMD_STATUS_OFFSET], STATUS_RECEIVE);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, " %!FUNC!: Trigger Adm Cmd to Chip %d\n", chip_id);
}

static ULONG _parsing_configuration_space(PULONG plBuf, UCHAR offs) {
    ULONG       LinkCap     = 0;
    PUCHAR      pbBuf       = (PUCHAR)plBuf;
    UCHAR       NextCapPtr  = 0;
    UCHAR       CapID       = 0;
    UCHAR       ByteOffset  = 0;
    ULONG       limitation_num = 128; // Capibility list 256 / Minimun header 2

    if (plBuf) {
        NextCapPtr = pbBuf[0x34];
        while (NextCapPtr && limitation_num) {
            ByteOffset = NextCapPtr;
            CapID = pbBuf[ByteOffset];
            if (CapID == PCI_CAPABILITY_ID_PCI_EXPRESS) {
                LinkCap = plBuf[(ByteOffset + offs) >> 2];
            }
            NextCapPtr = pbBuf[ByteOffset + 1];
            limitation_num--;
        }
    }

    if (limitation_num == 0) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, " Warning capibility not found!\n");
        LinkCap = 0xFFFFFFFF;
    }

    return LinkCap;
}

static NTSTATUS _get_device_feature(PDEVICE_CONTEXT devContext, PVOID data, PBOOLEAN complete_request){
    NTSTATUS                status      = STATUS_SUCCESS;
    UCHAR                   chip_id     = 0;
    ULONG                   bytesRead   = 0;
    struct transport_cmd    *pCmd       = (struct transport_cmd *) data;
    PULONG                  plBuf       = NULL;
    ULONG                   capibility_data = 0;

    switch(pCmd->SQ.subOpCode){
        case FID_DEVICE_THROUGHPUT://driver command
            break;
        case FID_DEVICE_TEMPERATURE:
        case FID_DEVICE_INFO:
        case FID_DEVICE_VOLTAGE:
        case FID_DEVICE_POWER:
            _host_trigger_admcmd(devContext, CHIP_ID0, pCmd);
            pCmd->CQ.status = _feature_fetch_result(devContext, CHIP_ID0, pCmd);
            break;
        case FID_DEVICE_INTERFACE_INFO:
            if (devContext->busInterface.GetBusData != NULL) {
                plBuf = (PULONG)ExAllocatePool2(POOL_FLAG_NON_PAGED, PCI_CONFIG_SPACE_SIZE, 'cfgs');
                if (plBuf) {
                    bytesRead = devContext->busInterface.GetBusData(devContext->busInterface.Context, PCI_WHICHSPACE_CONFIG, (PVOID) plBuf, 0, PCI_CONFIG_SPACE_SIZE);
                    if (bytesRead != PCI_CONFIG_SPACE_SIZE) {
                        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DRIVER, "Get Data %d only but expect %d\n", bytesRead, PCI_CONFIG_SPACE_SIZE);
                    }
                    pCmd->CQ.data[0] = _parsing_configuration_space(plBuf, 0xc); // PCI_EXP_LNKCAP
                    capibility_data = _parsing_configuration_space(plBuf, 0x10);
                    pCmd->CQ.data[1] = (capibility_data < 0xFFFFFFFF) ? (_parsing_configuration_space(plBuf, 0x10) >> 16) : capibility_data; // PCI_EXP_LNKSTA (actually 0x12, but need 4 bytes alignment)
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, " FIND Link Cap 0x%x\n", pCmd->CQ.data[0]);
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, " FIND Link Sts 0x%x\n", pCmd->CQ.data[1]);
                    ExFreePool(plBuf);
                } else {
                    status = STATUS_NOT_SUPPORTED;
                }
            } else {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "PciConfigSpace not open on read!\n");
                status = STATUS_NOT_SUPPORTED;
            }
            pCmd->CQ.status = ERROR_STATUS_NO_ERROR;
            break;
        case FID_DEVICE_HW_INFO:
            chip_id = (UCHAR)pCmd->SQ.cdw2;
            if ((chip_id < devContext->hw_info.chip.total_chip_cnt)) {
                pCmd->CQ.data[0] = devContext->hw_info.chip.generation;
                pCmd->CQ.data[1] = devContext->hw_info.chip.total_chip_cnt;
                pCmd->CQ.data[2] = devContext->hw_info.chip.curr_config_chip_count;
                pCmd->CQ.data[3] = devContext->hw_info.chip.group_count;
            } else {
                pCmd->CQ.status = ERROR_STATUS_PARAMETER_FAIL;
            }
            break;
        case FID_DEVICE_MPU_UTILIZATION:
            pCmd->CQ.data[0] = MemxUtilSramRead(devContext, (DEV_CODE_SRAM_MPUUTIL_BASE+(pCmd->SQ.cdw2 << 2)));
            pCmd->CQ.status = ERROR_STATUS_NO_ERROR;
            break;
        default:
            chip_id = (UCHAR)pCmd->SQ.cdw2;
            if ((chip_id < devContext->hw_info.chip.total_chip_cnt)) {
                _host_trigger_admcmd(devContext, chip_id, pCmd);
                pCmd->CQ.status = _feature_fetch_result(devContext, chip_id, pCmd);

                if (pCmd->SQ.subOpCode == FID_DEVICE_DMA_TRIGGER_TYPE) {
                    devContext->hw_info.chip.input_dma_trigger_type[chip_id] = pCmd->CQ.data[0];
                }
            } else {
                pCmd->CQ.status = ERROR_STATUS_PARAMETER_FAIL;
            }
            break;
    }

    *complete_request = (BOOLEAN)TRUE;

    return status;
}

static NTSTATUS _set_device_feature(PDEVICE_CONTEXT devContext, PVOID data, PBOOLEAN complete_request){
    NTSTATUS    status              = STATUS_SUCCESS;
    UCHAR chip_id;
    struct transport_cmd *pCmd = (struct transport_cmd *) data;

    switch(pCmd->SQ.subOpCode){
        case FID_DEVICE_VOLTAGE:
        case FID_DEVICE_POWER_THRESHOLD:
            _host_trigger_admcmd(devContext, CHIP_ID0, pCmd);
            pCmd->CQ.status = _feature_fetch_result(devContext, CHIP_ID0, pCmd);
            break;
        case FID_DEVICE_INTERFACE_INFO: //driver command
            break;
        default:
            chip_id = (UCHAR)pCmd->SQ.cdw2;
            if ((chip_id < devContext->hw_info.chip.total_chip_cnt)) {
                _host_trigger_admcmd(devContext, chip_id, pCmd);
                pCmd->CQ.status = _feature_fetch_result(devContext, chip_id, pCmd);

                if (pCmd->SQ.subOpCode == FID_DEVICE_DMA_TRIGGER_TYPE) {
                    devContext->hw_info.chip.input_dma_trigger_type[chip_id] = pCmd->SQ.cdw3;
                }
            } else {
                pCmd->CQ.status = ERROR_STATUS_PARAMETER_FAIL;
            }
            break;
    }

    *complete_request = (BOOLEAN)TRUE;

    return status;
}

static NTSTATUS _device_selftest(PDEVICE_CONTEXT devContext, PVOID data, PBOOLEAN complete_request){
    NTSTATUS    status              = STATUS_SUCCESS;
    UCHAR chip_id;
    struct transport_cmd *pCmd = (struct transport_cmd *) data;

    switch(pCmd->SQ.subOpCode){
        case TESTID_PCIE_BANDWIDTH:
            chip_id = (UCHAR)pCmd->SQ.cdw2;
            if ((chip_id < devContext->hw_info.chip.total_chip_cnt)) {
                _host_trigger_admcmd(devContext, chip_id, pCmd);
                pCmd->CQ.status = _feature_fetch_result(devContext, chip_id, pCmd);
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Status 0x%08x, Read %ld MB/s Write %ld MB/s\n", pCmd->CQ.status, pCmd->CQ.data[0], pCmd->CQ.data[1]);
            } else {
                pCmd->CQ.status = ERROR_STATUS_PARAMETER_FAIL;
            }
            break;
        default:
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER," %!FUNC!: invalid SubOp: 0x%08x \n", pCmd->SQ.subOpCode);
            break;
    }

    *complete_request = (BOOLEAN)TRUE;

    return status;
}

static NTSTATUS _admin_command_handler(PDEVICE_CONTEXT devContext, PVOID data, PBOOLEAN complete_request){
    NTSTATUS    status              = STATUS_SUCCESS;
    struct transport_cmd *pCmd = (struct transport_cmd *) data;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER," %!FUNC!: Admin Op: 0x%08x SubOp 0x%08x\n", pCmd->SQ.opCode, pCmd->SQ.subOpCode);

    switch(pCmd->SQ.opCode){
        case MEMX_ADMIN_CMD_SET_FEATURE:
            status = _set_device_feature(devContext, data, complete_request);
            break;
        case MEMX_ADMIN_CMD_GET_FEATURE:
            status = _get_device_feature(devContext, data, complete_request);
            break;
        case MEMX_ADMIN_CMD_DOWNLOAD_DFP:
            //windowus do not use this
            break;
        case MEMX_ADMIN_CMD_SELFTEST:
            status = _device_selftest(devContext, data, complete_request);
            break;
        case MEMX_ADMIN_CMD_DEVIOCTRL:
            if (pCmd->SQ.subOpCode == FID_DEVICE_I2C_TRANSCEIVE) {
                pCmd->SQ.opCode = MEMX_ADMIN_CMD_SET_FEATURE;
                status = _set_device_feature(devContext, data, complete_request);
            } else {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER," %!FUNC!: Not Support SubOp: 0x%08x \n", pCmd->SQ.subOpCode);
                pCmd->CQ.status = ERROR_STATUS_SUBOP_NOT_SUPPORT_FAIL;
            }
            break;
        default:
            pCmd->CQ.status = ERROR_STATUS_OPCODE_NOT_SUPPORT_FAIL;
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER," %!FUNC!: Not Support Op: 0x%08x \n", pCmd->SQ.opCode);
            break;
    }

    return status;
}

static NTSTATUS _config_mpu_group_pmu(PDEVICE_CONTEXT devContext, UCHAR base_chip_id, UCHAR num_of_mpu)
{
    NTSTATUS    status = STATUS_SUCCESS;

    do {
        if (!devContext) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "%!FUNC!: !devContext");
            break;
        }
        if (base_chip_id >= MAX_SUPPORT_CHIP_NUM) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "%!FUNC!: Invalid base_chip_id(%u)", base_chip_id);
            break;
        }
        if ((base_chip_id + num_of_mpu) >= MAX_SUPPORT_CHIP_NUM) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "%!FUNC!: Invalid num_of_mpu(%u)", num_of_mpu);
            break;
        }

        for (CHAR chip_id = base_chip_id + num_of_mpu - 1; chip_id >= base_chip_id; --chip_id) {
            if (devContext->dfpRgcfgPmuConfig[chip_id] != ALL_MPU_CORE_GROUP_ON) {
                status = MemxUtilXflowWrite(devContext, (UCHAR)chip_id, PMU_MPU_PWR_MGMT_ADDR, 0, devContext->dfpRgcfgPmuConfig[chip_id], TO_CONFIG_OUTPUT);
            }
        }

        for (CHAR chip_id = base_chip_id + num_of_mpu - 1; chip_id >= base_chip_id; --chip_id) {
            if (devContext->dfpRgcfgPmuConfig[chip_id] != ALL_MPU_CORE_GROUP_ON) {
                ULONG indicator = 0;
                ULONG temp = devContext->dfpRgcfgPmuConfig[chip_id];
                for (UCHAR cg = 0; cg < MEMX_MPU_MAX_CORE_GROUP; ++cg) {
                    ULONG op = temp & PMU_MPU_CORE_GROUP_MASK;
                    if (op == PMU_MPU_CORE_GROUP_ON) {
                        indicator |= (PMU_MPU_CORE_GROUP_ON_STATUS << (cg * PMU_MPU_CORE_GROUP_OFFSET));
                    }
                    if (op == PMU_MPU_CORE_GROUP_OFF) {
                        indicator |= (PMU_MPU_CORE_GROUP_OFF_STATUS << (cg * PMU_MPU_CORE_GROUP_OFFSET));
                    }
                    temp >>= PMU_MPU_CORE_GROUP_OFFSET;
                }

                do {
                    temp = MemxUtilXflowRead(devContext, (UCHAR)chip_id, PMU_MPU_PWR_MGMT_ADDR, 0, TO_CONFIG_OUTPUT);
                    if (temp == 0xFFFFFFFF) {
                        status = STATUS_DEVICE_NOT_READY;
                        break;
                    }
                } while ((temp & indicator) != indicator);

                status = MemxUtilXflowWrite(devContext, (UCHAR)chip_id, PMU_MPU_PWR_MGMT_ADDR, 0, 0, TO_CONFIG_OUTPUT);
            }
        }
    }while(0);

    return status;
}

static NTSTATUS _parsing_weight_memory(PDEVICE_CONTEXT devContext, PVOID data)
{
    NTSTATUS    status              = STATUS_SUCCESS;
    PULONG      plBuf               = (PULONG)data;
    UCHAR       base_chip_id        = devContext->dfpImageInfo[1];
    ULONG       num_of_mpu          = plBuf[3];
    ULONG       chip_id             = 0;
    ULONG       mpu_index           = 0;
    ULONG       cur_index           = 0;
    ULONG       cur_wtmem_index     = 0;
    ULONG       curr_rgcfg_size     = 0;
    ULONG       total_wtmem_block_count = 0;
    ULONG       crc = 0;
    ULONG       chipcfig = 0;
    cur_index = 4;
    for (mpu_index = 0; mpu_index < num_of_mpu; mpu_index++)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "num_of_mpu(%d) mpu_index %d\n", num_of_mpu, mpu_index);

        chip_id = plBuf[cur_index++] + base_chip_id;
        TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "chip_id(%d)\n", chip_id);

        if(chip_id >= CHIP_ID_MAX){
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        curr_rgcfg_size = plBuf[cur_index++];
        TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "curr_rgcfg_size(%d)\n", curr_rgcfg_size);

        cur_wtmem_index = cur_index;

        total_wtmem_block_count = plBuf[cur_wtmem_index++];
        TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "total_wtmem_block_count(%d)\n", total_wtmem_block_count);

        if(total_wtmem_block_count){
            devContext->download_wmem_context[chip_id].uwMaxEntryCnt = (USHORT) total_wtmem_block_count;
            devContext->download_wmem_context[chip_id].uwCurEntryIdx = 0;
            devContext->download_wmem_context[chip_id].uwValidEntryCnt = 0;
            devContext->download_wmem_context[chip_id].pContext = (pWmemEntry) ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(WmemEntry) * devContext->download_wmem_context[chip_id].uwMaxEntryCnt, '0mxm' + chip_id);
            if(devContext->download_wmem_context[chip_id].pContext == NULL){
                status = STATUS_MEMORY_NOT_ALLOCATED;
                break;
            } else {
                RtlZeroMemory(devContext->download_wmem_context[chip_id].pContext, sizeof(WmemEntry) * devContext->download_wmem_context[chip_id].uwMaxEntryCnt);

                for (ULONG wtmem_block_count = 0; wtmem_block_count < total_wtmem_block_count; wtmem_block_count++)
                {
                    ULONG curr_wtmem_common_header[2] = { 0 }; // { total_size, target_address}
                    ULONG curr_wtmem_block_size = 0;
                    ULONG curr_wtmem_target_address = 0;

                    curr_wtmem_common_header[0] = plBuf[cur_wtmem_index++];
                    curr_wtmem_common_header[1] = plBuf[cur_wtmem_index++];

                    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "wtmem_block_count(%u) header0 0x%x header1 0x%x\n", wtmem_block_count, curr_wtmem_common_header[0], curr_wtmem_common_header[1]);

                    if (!curr_wtmem_common_header[0])
                    {
                        continue;
                    }
                    else
                    {
                        curr_wtmem_block_size = curr_wtmem_common_header[0] - 8; // subtract common header size(8)
                        curr_wtmem_target_address = curr_wtmem_common_header[1];

                        TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "curr_wtmem_block_size(%u) curr_wtmem_target_address 0x%x\n", curr_wtmem_block_size, curr_wtmem_target_address);

                        if (curr_wtmem_block_size > 0) {
                            devContext->download_wmem_context[chip_id].pContext[devContext->download_wmem_context[chip_id].uwValidEntryCnt].chip_id = chip_id;
                            devContext->download_wmem_context[chip_id].pContext[devContext->download_wmem_context[chip_id].uwValidEntryCnt].des_addr_base = curr_wtmem_target_address;
                            devContext->download_wmem_context[chip_id].pContext[devContext->download_wmem_context[chip_id].uwValidEntryCnt].src_buf_base = &plBuf[cur_wtmem_index];
                            devContext->download_wmem_context[chip_id].pContext[devContext->download_wmem_context[chip_id].uwValidEntryCnt].total_size = curr_wtmem_block_size;
                            devContext->download_wmem_context[chip_id].pContext[devContext->download_wmem_context[chip_id].uwValidEntryCnt].written_size = 0;
                            devContext->download_wmem_context[chip_id].uwValidEntryCnt++;
                            cur_wtmem_index += (curr_wtmem_block_size >> 2); // adjust UCHAR count to ULONG count
                        }

                        crc = plBuf[cur_wtmem_index++];
                        TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "CRC(0x%x)\n", crc);
                    }
                }
            }
        }

        chipcfig = plBuf[cur_index++];
        TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "chip's config (0x%x)\n", chipcfig);

        TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "Update cur_index from (%u) to (%u)\n", cur_index, cur_index + (curr_rgcfg_size / 4));
        cur_index += (curr_rgcfg_size / 4);
    }

    return status;
}

static NTSTATUS _download_dfp_memory_parallel(PDEVICE_CONTEXT devContext, UCHAR base_chip_id, ULONG num_of_mpu) {
    NTSTATUS                status              = STATUS_SUCCESS;
    const UCHAR             max_parallel_count  = 4; //MAX support 4 parallel downloading
    PUCHAR                  ubtxBuf             = devContext->CommonBufferBaseDriver[BUFFER_IDX_WRITE];
    PUCHAR                  ubrxBuf             = devContext->CommonBufferBaseDriver[BUFFER_IDX_READ];
    struct transport_cmd    cmd_k[4] = {0};
    UCHAR                   chip_id;
    UCHAR                   non_finshed_chip_count;
    ULONG                   chip_bitmap;
    USHORT                  uwidx;
    PUCHAR                  ubBuf;
    ULONG                   length;
    ULONG                   max_data_length;
    UCHAR                   src_type;
    UCHAR                   valid_count;

    for (UCHAR cur_base_id = base_chip_id; cur_base_id < (base_chip_id + num_of_mpu); cur_base_id += max_parallel_count) {
        non_finshed_chip_count = max_parallel_count;
        chip_bitmap = 0;

        for (UCHAR idx = 0; idx < max_parallel_count; idx++) {//some chip has nothing to do, mark it
            chip_id = cur_base_id + idx;
            if (!devContext->download_wmem_context[chip_id].uwValidEntryCnt) {
                chip_bitmap |= BIT(chip_id);
                non_finshed_chip_count--;
            }
        }

        //Reset cmd_k
        RtlZeroMemory(&cmd_k[0], sizeof(struct transport_cmd) * 4);

        while (non_finshed_chip_count) {
            //reset valid chip
            valid_count = 0;

            for (UCHAR idx = 0; idx < max_parallel_count; idx++) {//Fill SQ part and prepare data
                chip_id = cur_base_id + idx;

                if (!(chip_bitmap & BIT(chip_id))) {//for each non finished chip
                    valid_count++;
                    uwidx = devContext->download_wmem_context[chip_id].uwCurEntryIdx;
                    ubBuf = (PUCHAR)devContext->download_wmem_context[chip_id].pContext[uwidx].src_buf_base;

                    //Fill SQ
                    cmd_k[idx].SQ.opCode    = MEMX_ADMIN_CMD_DOWNLOAD_DFP;
                    cmd_k[idx].SQ.cmdLen    = sizeof(struct transport_cmd);
                    cmd_k[idx].SQ.subOpCode = DFP_DOWNLOAD_WEIGHT_MEMORY;
                    cmd_k[idx].SQ.reqLen    = sizeof(struct transport_cq);
                    cmd_k[idx].SQ.attr      = 0;
                    cmd_k[idx].SQ.cdw2      = chip_id;
                    cmd_k[idx].SQ.cdw3      = devContext->download_wmem_context[chip_id].pContext[uwidx].des_addr_base;
                    cmd_k[idx].SQ.cdw4      = devContext->download_wmem_context[chip_id].pContext[uwidx].written_size;

                    max_data_length = (non_finshed_chip_count > 2) ? DEF_KB(256) : DEF_KB(512);
                    if ((devContext->download_wmem_context[chip_id].pContext[uwidx].total_size - devContext->download_wmem_context[chip_id].pContext[uwidx].written_size) > max_data_length) {
                        length = max_data_length;
                    }
                    else {
                        length = (devContext->download_wmem_context[chip_id].pContext[uwidx].total_size - devContext->download_wmem_context[chip_id].pContext[uwidx].written_size);
                    }
                    cmd_k[idx].SQ.cdw5      = length;

                    if (valid_count == 1) {
                        src_type = 0x10;
                    }
                    else if (valid_count == 2) {
                        src_type = 0x20;
                    }
                    else if (valid_count == 3) {
                        src_type = 0x14;
                    }
                    else if (valid_count == 4) {
                        src_type = 0x24;
                    }
                    else {
                        src_type = 0;
                    }
                    cmd_k[idx].SQ.cdw6      = src_type;

                    if (uwidx == 0 && devContext->download_wmem_context[chip_id].pContext[uwidx].written_size == 0) {
                        cmd_k[idx].SQ.attr |= BIT(0);//Firtst DMA
                    }
                    if (((uwidx + 1) == devContext->download_wmem_context[chip_id].uwValidEntryCnt) && ((devContext->download_wmem_context[chip_id].pContext[uwidx].written_size + length) == devContext->download_wmem_context[chip_id].pContext[uwidx].total_size)) {
                        cmd_k[idx].SQ.attr |= BIT(1);//Last DMA
                        cmd_k[idx].SQ.attr |= BIT(2);//Polling this DMA done
                    }
                    if (length <= DEF_KB(64)) {
                        cmd_k[idx].SQ.attr |= BIT(2);//single trunk (<= 64KB) Polling this DMA done
                    }

                    //Prepare DMA data
                    if (src_type == 0x10) {
                        RtlCopyMemory(ubtxBuf, ubBuf + devContext->download_wmem_context[chip_id].pContext[uwidx].written_size, length);
                    }
                    else if (src_type == 0x14) {
                        RtlCopyMemory(ubtxBuf + DEF_KB(256), ubBuf + devContext->download_wmem_context[chip_id].pContext[uwidx].written_size, length);
                    }
                    else if (src_type == 0x20) {
                        RtlCopyMemory(ubrxBuf, ubBuf + devContext->download_wmem_context[chip_id].pContext[uwidx].written_size, length);
                    }
                    else if (src_type == 0x24) {
                        RtlCopyMemory(ubrxBuf + DEF_KB(256), ubBuf + devContext->download_wmem_context[chip_id].pContext[uwidx].written_size, length);
                    }
                    else {
                        status = STATUS_INVALID_PARAMETER;
                        goto done;
                    }

                    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "cmd %d chip %d flag 0x%x des 0x%x src 0x%x offset 0x%x length 0x%x", idx, cmd_k[idx].SQ.cdw2, cmd_k[idx].SQ.attr, cmd_k[idx].SQ.cdw3, cmd_k[idx].SQ.cdw6, cmd_k[idx]. SQ.cdw4, cmd_k[idx].SQ.cdw5);
                }
            }

            for (UCHAR idx = 0; idx < max_parallel_count; idx++) { //trigger Admin Command
                if (cmd_k[idx].SQ.opCode) {
                    _host_trigger_admcmd(devContext, (UCHAR)cmd_k[idx].SQ.cdw2, &cmd_k[idx]);
                }
            }

            for (UCHAR idx = 0; idx < max_parallel_count; idx++) { //polling admin command done, require all command done
                if(cmd_k[idx].SQ.opCode && (cmd_k[idx].SQ.cdw2 < devContext->hw_info.chip.total_chip_cnt)) {
                    cmd_k[idx].CQ.status = _feature_fetch_result(devContext, (UCHAR) cmd_k[idx].SQ.cdw2, &cmd_k[idx]);
                    if (cmd_k[idx].CQ.status != ERROR_STATUS_NO_ERROR) {
                        status = STATUS_INVALID_PARAMETER;
                        goto done;
                    }
                }
            }

            for (UCHAR idx = 0; idx < max_parallel_count; idx++) {//Update information
                if (cmd_k[idx].SQ.opCode) {
                    chip_id = (UCHAR) cmd_k[idx].SQ.cdw2;
                    uwidx = devContext->download_wmem_context[chip_id].uwCurEntryIdx;
                    if (!(chip_bitmap & BIT(chip_id))) {//for each non finished chip
                        if (devContext->download_wmem_context[chip_id].uwCurEntryIdx < devContext->download_wmem_context[chip_id].uwValidEntryCnt) {
                            devContext->download_wmem_context[chip_id].pContext[uwidx].written_size += cmd_k[idx].SQ.cdw5;

                            if (devContext->download_wmem_context[chip_id].pContext[uwidx].written_size == devContext->download_wmem_context[chip_id].pContext[uwidx].total_size) {
                                devContext->download_wmem_context[chip_id].uwCurEntryIdx++;
                                if (devContext->download_wmem_context[chip_id].uwCurEntryIdx == devContext->download_wmem_context[chip_id].uwValidEntryCnt) {
                                    chip_bitmap |= BIT(chip_id);
                                    non_finshed_chip_count--;
                                    //Reset SQ
                                    RtlZeroMemory(&cmd_k[idx], sizeof(struct transport_cmd));
                                }
                            }
                        }
                    }
                }
            }
        } //while (non_finshed_chip_count)
    } //for (UCHAR cur_base_id = base_chip_id; cur_base_id < (base_chip_id + num_of_mpu); cur_base_id += max_parallel_count)

done:
    for (UCHAR mpu_index = 0; mpu_index < num_of_mpu; mpu_index++) {
        chip_id = base_chip_id + mpu_index;
        if (devContext->download_wmem_context[chip_id].pContext) {
            ExFreePool(devContext->download_wmem_context[chip_id].pContext);
        }
        RtlZeroMemory((void*)&(devContext->download_wmem_context[chip_id]), sizeof(DevWmemContext));
    }

    return status;
}

static NTSTATUS _download_dfp_memory_legacy(PDEVICE_CONTEXT devContext, UCHAR base_chip_id, ULONG num_of_mpu) {
    NTSTATUS    status = STATUS_SUCCESS;
    UCHAR       chip_id;
    USHORT      uwIdx;
    ULONG       base_adr;
    ULONG       total_size;
    ULONG       written_size;
    PUCHAR      src_buf;
    for (UCHAR idx = 0; idx < num_of_mpu; idx++) {
        chip_id = base_chip_id + idx;
        if(chip_id < devContext->hw_info.chip.total_chip_cnt){
            if (devContext->download_wmem_context[chip_id].uwValidEntryCnt) {
                for (devContext->download_wmem_context[chip_id].uwCurEntryIdx = 0; devContext->download_wmem_context[chip_id].uwCurEntryIdx < devContext->download_wmem_context[chip_id].uwValidEntryCnt; devContext->download_wmem_context[chip_id].uwCurEntryIdx++) {
                    uwIdx           = devContext->download_wmem_context[chip_id].uwCurEntryIdx;
                    base_adr        = devContext->download_wmem_context[chip_id].pContext[uwIdx].des_addr_base;
                    total_size      = devContext->download_wmem_context[chip_id].pContext[uwIdx].total_size;
                    written_size    = devContext->download_wmem_context[chip_id].pContext[uwIdx].written_size;
                    src_buf         = (PUCHAR) devContext->download_wmem_context[chip_id].pContext[uwIdx].src_buf_base;

                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Program chip %d from %p to 0x%x with size %ld\n", chip_id, devContext->download_wmem_context[chip_id].pContext[uwIdx].src_buf_base, base_adr, total_size);

                    if (base_adr == 0x38800000) {
                        // fmem occupy 128 bytes on bus, skip write data to 0xc because hardware will clear all fmem
                        for (written_size = 0; written_size < total_size; written_size += DEF_BYTE(16)) {
                            for (int offs = 0; offs < DEF_BYTE(12); offs += DEF_BYTE(4)) {
                                status = MemxUtilXflowWrite(devContext, chip_id, base_adr, written_size + offs, *(PULONG)(src_buf + written_size + offs), TO_MPU);
                            }
                        }
                        devContext->download_wmem_context[chip_id].pContext[uwIdx].written_size = total_size;
                    }
                    else {
                        // wtmem
                        status = MemxUtilXflowBurstWriteWTMEM(devContext, (UCHAR)chip_id, base_adr, devContext->download_wmem_context[chip_id].pContext[uwIdx].src_buf_base, total_size, TO_MPU);
                        devContext->download_wmem_context[chip_id].pContext[uwIdx].written_size = total_size;
                    }
                }
            }
        } else {
            status = STATUS_INVALID_PARAMETER;
            break;
        }
    }

    for (UCHAR mpu_index = 0; mpu_index < num_of_mpu; mpu_index++) {
        chip_id = base_chip_id + mpu_index;
        if (devContext->download_wmem_context[chip_id].pContext) {
            ExFreePool(devContext->download_wmem_context[chip_id].pContext);
        }
        RtlZeroMemory((void*)&(devContext->download_wmem_context[chip_id]), sizeof(DevWmemContext));
    }

    return status;
}

static NTSTATUS _download_data_flow_program(PDEVICE_CONTEXT devContext, PVOID data, size_t size)
{
    NTSTATUS    status              = STATUS_SUCCESS;
    PULONG      plBuf               = (PULONG)data;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "buf %p bufsize %Iu type(%x)", data, size, devContext->dfpImageInfo[0]);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "hw_version(%x) datecode(%d) reserve(%d) num of mpu(%d)\n", plBuf[0], plBuf[1], plBuf[2], plBuf[3]);

    UCHAR       type         = devContext->dfpImageInfo[0];
    UCHAR       base_chip_id = devContext->dfpImageInfo[1];
    UCHAR       flag         = devContext->dfpImageInfo[2];
    ULONG       num_of_mpu  = plBuf[3];
    ULONG       chip_id     = 0;
    ULONG       mpu_index   = 0;
    ULONG       cur_index   = 0;
    ULONG       curr_rgcfg_size = 0;

    switch (type)
    {
        case DFP_FROM_SEPERATE_WTMEM:
            status = _parsing_weight_memory(devContext, data);
            if (NT_SUCCESS(status)) {
                if (flag & BIT(6)) {
                    status = _download_dfp_memory_legacy(devContext, base_chip_id, num_of_mpu);
                } else {
                    status = _download_dfp_memory_parallel(devContext, base_chip_id,num_of_mpu);
                }
            }
            break;
        case DFP_FROM_SEPERATE_CONFIG:
            cur_index = 4;
            for (mpu_index = 0; mpu_index < num_of_mpu; mpu_index++)
            {
                TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "num_of_mpu(%d) mpu_index %d\n", num_of_mpu, mpu_index);

                chip_id = plBuf[cur_index++] + base_chip_id;
                TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "chip_id(%d)\n", chip_id);

                curr_rgcfg_size = plBuf[cur_index++];
                TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "curr_rgcfg_size(%d)\n", curr_rgcfg_size);

                ULONG cur_rgcfg_index = cur_index;
                ULONG curr_rgcfg_block_size = curr_rgcfg_size;
                ULONG curr_rgcfg_target_address = 0;
                ULONG curr_rgcfg_value = 0;

                TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "curr_rgcfg_block_size(%u)\n", curr_rgcfg_block_size);
                // Allow FM SRAM clock to be off automatically when not in use
                status = MemxUtilXflowWrite(devContext, (UCHAR) chip_id, 0x38000004, 0x0, 0x0, TO_MPU);

                for (UCHAR cg_id = 0; cg_id < MEMX_MPU_MAX_CORE_GROUP; cg_id++) {
                    // Enable dynamic clock gating in core groups
                    status = MemxUtilXflowWrite(devContext, (UCHAR) chip_id, 0x31800000 | ((cg_id+1) << 24), 0x18, 0x5, TO_MPU);

                    // Allow WT SRAM clock to be off automatically when not in use
                    status = MemxUtilXflowWrite(devContext, (UCHAR) chip_id, 0x31800000 | ((cg_id+1) << 24), 0x28, 0x0, TO_MPU);
                }

                while (curr_rgcfg_block_size) {
                    curr_rgcfg_target_address   = plBuf[cur_rgcfg_index++];
                    curr_rgcfg_value            = plBuf[cur_rgcfg_index++];

                    // adjust chip id of c_egr_write_ahb_addr for stream mode egress dcore to write correct next MPU address in muti-group case
                    if ((curr_rgcfg_target_address & 0x30208010) == 0x30208010 && (curr_rgcfg_value & 0x60000000) == 0x60000000) {
                        curr_rgcfg_value &= ~(0xf << 18);
                        curr_rgcfg_value |= ((chip_id + 1) << 18);
                    }

                    // Turn off clock for unused M/A-cores
                    if (curr_rgcfg_target_address == 0x31800008) status = MemxUtilXflowWrite(devContext, (UCHAR) chip_id, 0x31800000, 0x0, ~curr_rgcfg_value, TO_MPU);
                    if (curr_rgcfg_target_address == 0x3180000c) status = MemxUtilXflowWrite(devContext, (UCHAR) chip_id, 0x31800004, 0x0, ~curr_rgcfg_value, TO_MPU);
                    if (curr_rgcfg_target_address == 0x32800008) status = MemxUtilXflowWrite(devContext, (UCHAR) chip_id, 0x32800000, 0x0, ~curr_rgcfg_value, TO_MPU);
                    if (curr_rgcfg_target_address == 0x3280000c) status = MemxUtilXflowWrite(devContext, (UCHAR) chip_id, 0x32800004, 0x0, ~curr_rgcfg_value, TO_MPU);
                    if (curr_rgcfg_target_address == 0x33800008) status = MemxUtilXflowWrite(devContext, (UCHAR) chip_id, 0x33800000, 0x0, ~curr_rgcfg_value, TO_MPU);
                    if (curr_rgcfg_target_address == 0x3380000c) status = MemxUtilXflowWrite(devContext, (UCHAR) chip_id, 0x33800004, 0x0, ~curr_rgcfg_value, TO_MPU);
                    if (curr_rgcfg_target_address == 0x34800008) status = MemxUtilXflowWrite(devContext, (UCHAR) chip_id, 0x34800000, 0x0, ~curr_rgcfg_value, TO_MPU);
                    if (curr_rgcfg_target_address == 0x3480000c) status = MemxUtilXflowWrite(devContext, (UCHAR) chip_id, 0x34800004, 0x0, ~curr_rgcfg_value, TO_MPU);
                    if (curr_rgcfg_target_address == 0x35800008) status = MemxUtilXflowWrite(devContext, (UCHAR) chip_id, 0x35800000, 0x0, ~curr_rgcfg_value, TO_MPU);
                    if (curr_rgcfg_target_address == 0x3580000c) status = MemxUtilXflowWrite(devContext, (UCHAR) chip_id, 0x35800004, 0x0, ~curr_rgcfg_value, TO_MPU);
                    if (curr_rgcfg_target_address == 0x36800008) status = MemxUtilXflowWrite(devContext, (UCHAR) chip_id, 0x36800000, 0x0, ~curr_rgcfg_value, TO_MPU);
                    if (curr_rgcfg_target_address == 0x3680000c) status = MemxUtilXflowWrite(devContext, (UCHAR) chip_id, 0x36800004, 0x0, ~curr_rgcfg_value, TO_MPU);

                    // set xflow write to access other  PMU config address, set xflow write to access other
                    if(curr_rgcfg_target_address == PMU_MPU_PWR_MGMT_ADDR){
                        devContext->dfpRgcfgPmuConfig[chip_id] = curr_rgcfg_value;
                    } else {
                        status = MemxUtilXflowWrite(devContext, (UCHAR) chip_id, curr_rgcfg_target_address, 0, curr_rgcfg_value, TO_MPU);
                    }

                    curr_rgcfg_block_size -= 8;
                }

                ULONG chipcfig = plBuf[cur_index++];
                TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "chip's config (0x%x)\n", chipcfig);

                TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "Update cur_index from (%u) to (%u)\n", cur_index, cur_index + (curr_rgcfg_size / 4));
                cur_index += (curr_rgcfg_size / 4);
            }

            status = _config_mpu_group_pmu(devContext, base_chip_id, (UCHAR)num_of_mpu);
            break;
        default:
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Status 0x%0x\n", status);

    return status;
}

NTSTATUS MemxEmptyPendingQueue(PDEVICE_CONTEXT devContext)
{   //The drivers can cancel the requests that they own by completing them with a completion status of STATUS_CANCELLED.
    //For read, write and IOCTL requests, it is necessary for the driver to call WdfRequestCompleteWithInformation
    NTSTATUS    status          = STATUS_SUCCESS;
    WDFREQUEST  request;
    ULONG_PTR   information;

    do
    {
        status = WdfIoQueueRetrieveNextRequest(devContext->IoctlPendingQueue, &request);

        if (NT_SUCCESS(status))
        {
            WDF_REQUEST_PARAMETERS params;
            WDF_REQUEST_PARAMETERS_INIT(&params);
            WdfRequestGetParameters(request, &params);
            information = params.Parameters.DeviceIoControl.InputBufferLength;
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Cancel Request in IoctlPendingQueue\n");
            WdfRequestCompleteWithInformation(request, STATUS_CANCELLED, information);
        }
    }
    while(NT_SUCCESS(status));

    do
    {
        status = WdfIoQueueRetrieveNextRequest(devContext->WritePendingQueue, &request);

        if (NT_SUCCESS(status))
        {
            information = MSIX_ERROR_INDICTOR_VAL;
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Cancel Request in WritePendingQueue\n");
            WdfRequestCompleteWithInformation(request, STATUS_CANCELLED, information);
        }
    }
    while(NT_SUCCESS(status));

    WdfSpinLockAcquire(devContext->IoQueueLockHandle[QUEUE_IDX_READ]);

    do
    {
        status = WdfIoQueueRetrieveNextRequest(devContext->ReadPendingQueue, &request);

        if (NT_SUCCESS(status))
        {
            information = MSIX_ERROR_INDICTOR_VAL;
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Cancel Request in ReadPendingQueue\n");
            WdfRequestCompleteWithInformation(request, STATUS_CANCELLED, information);
        }
    }
    while(NT_SUCCESS(status));

    devContext->is_read_abort = 1;

    WdfSpinLockRelease(devContext->IoQueueLockHandle[QUEUE_IDX_READ]);

    return STATUS_SUCCESS;
}

VOID MemxEvtDeviceFileCreate(WDFDEVICE Device, WDFREQUEST Request, WDFFILEOBJECT FileObject)
{
    NTSTATUS            status = STATUS_SUCCESS;
    PUNICODE_STRING     fileName;
    PFILE_CONTEXT       pFileContext;
    PDEVICE_CONTEXT     pDevContext;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    pDevContext     = GetDeviceContext(Device);
    pFileContext    = GetFileContext(FileObject);
    fileName        = WdfFileObjectGetFileName(FileObject);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "Device File Name %wZ\n", fileName);

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

    fileName = WdfFileObjectGetFileName(FileObject);
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "Device File Name %wZ\n", fileName);

    device = WdfFileObjectGetDevice(FileObject);

    if (device) {
        pDevContext = GetDeviceContext(device);
        WdfSpinLockAcquire(pDevContext->device_lock);
        pDevContext->reference_count--;
        WdfSpinLockRelease(pDevContext->device_lock);
    }
}

VOID MemxEvtIoDeviceControl(WDFQUEUE Queue, WDFREQUEST Request, size_t OutputBufferLength, size_t InputBufferLength, ULONG IoControlCode)
{
    NTSTATUS            status          = STATUS_SUCCESS;
    PDEVICE_CONTEXT     devContext      = NULL;
    PVOID               buffer          = NULL;
    size_t              bufLength;
    BOOLEAN             CompleteRequest = FALSE;
    ULONG_PTR           information     = 0;
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "--> %!FUNC! OutputBufferLength %d InputBufferLength %d IoControlCode 0x%x\r\n", (int) OutputBufferLength, (int) InputBufferLength, IoControlCode);

    devContext = GetDeviceContext(WdfIoQueueGetDevice(Queue));

    WdfSpinLockAcquire(devContext->IoQueueLockHandle[QUEUE_IDX_IOCTL]);

    if (devContext->deviceState == DEVICE_RUNNING) {
        switch (IoControlCode)
        {
            case MEMX_ABORT_TRANSFER:
                CompleteRequest = TRUE;
                information     = InputBufferLength;
                status          = MemxEmptyPendingQueue(devContext);
                break;
            case MEMX_DOWNLOAD_FIRMWARE:
                status = WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &buffer, &InputBufferLength);
                if (!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfRequestRetrieveInputBuffer 0x%0x", status);
                }
                else {
                    information         = InputBufferLength;
                    ULONG  img_size     = *((PULONG)buffer);
                    PULONG tx_dma_buf   = (PULONG)devContext->CommonBufferBaseDriver[BUFFER_IDX_WRITE];
                    RtlCopyMemory((PVOID)tx_dma_buf, buffer, InputBufferLength);
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Get Firmware Total Size %u Image Size %u CRC 0x%x MID 0x%x\r\n", (ULONG)InputBufferLength, img_size, tx_dma_buf[(InputBufferLength - 8) >> 2], tx_dma_buf[(InputBufferLength - 4) >> 2]);
                    status = MemxUtilSendFwCommand(devContext, PCIE_CMD_VENDOR_1, (USHORT)DEF_BYTE(256), CHIP_ID0);
                }
                break;
            case MEMX_GET_HW_INFO:
                status = WdfRequestRetrieveOutputBuffer(Request, OutputBufferLength, &buffer, NULL);
                if (!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfRequestRetrieveOutputBuffer 0x%0x", status);
                } else {
                    RtlCopyMemory(buffer, &devContext->hw_info, sizeof(struct hw_info));
                    CompleteRequest = TRUE;
                    information = OutputBufferLength;
                }
                //FIXME: Since udriver send MEMX_GET_HW_INFO earlier than create read thread
                //empty read Indicator queue to avoid garbage read INT when new round start
                devContext->IndicatorHeader = devContext->IndicatorTail;
                break;
            case MEMX_INIT_WTMEM_FMAP:
                status = WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &buffer, &InputBufferLength);
                if (!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfRequestRetrieveInputBuffer 0x%0x", status);
                } else {
                    UCHAR chip_id = *((PUCHAR)buffer);
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "chip_id %d", chip_id);
                    status = MemxUtilSendFwCommand(devContext, PCIE_CMD_INIT_WTMEM_FMAP, (USHORT)InputBufferLength, chip_id);
                }
                break;
            case MEMX_RESET_MPU:
                status = WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &buffer, &InputBufferLength);
                if (!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfRequestRetrieveInputBuffer 0x%0x", status);
                } else {
                    UCHAR chip_id = *((PUCHAR)buffer);
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "chip_id %d", chip_id);
                    status = MemxUtilSendFwCommand(devContext, PCIE_CMD_RESET_MPU, (USHORT)InputBufferLength, chip_id);
                }
                break;
            case MEMX_SET_DRIVER_INFO:
                status = WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &buffer, &bufLength);
                if (!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfRequestRetrieveInputBuffer 0x%0x", status);
                } else {
                    CompleteRequest = TRUE;
                    information = InputBufferLength;
                    struct memx_driver_info *pSQ = (struct memx_driver_info *) (buffer);
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "dfp_src %d", pSQ->CDW[10]);
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "base_chip_id %d", pSQ->CDW[11]);
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "legacy method %d", pSQ->CDW[12]);
                    devContext->dfpImageInfo[0] = (UCHAR)pSQ->CDW[10];
                    devContext->dfpImageInfo[1] = (UCHAR)pSQ->CDW[11];
                    devContext->dfpImageInfo[2] = (UCHAR)pSQ->CDW[12];
                }
                break;
            case MEMX_RUNTIMEDWN_DFP:
                status = WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &buffer, &bufLength);
                if (!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfRequestRetrieveInputBuffer 0x%0x", status);
                } else {
                    CompleteRequest = TRUE;
                    information     = InputBufferLength;
                    status = _download_data_flow_program(devContext, buffer, InputBufferLength);
                }
                break;
            case MEMX_XFLOW_ACCESS:
                status = WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &buffer, &bufLength);
                if (!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfRequestRetrieveInputBuffer 0x%0x", status);
                } else {
                    CompleteRequest = TRUE;
                    information = InputBufferLength;
                    struct memx_xflow_param *ptr = (struct memx_xflow_param *) buffer;

                    if (ptr->is_read) {
                        ptr->value = MemxUtilXflowRead(devContext, ptr->chip_id, ptr->base_addr, ptr->base_offset, ptr->access_mpu);
                    } else {
                        status = MemxUtilXflowWrite(devContext, ptr->chip_id, ptr->base_addr, ptr->base_offset, ptr->value, ptr->access_mpu);
                    }
                }
                break;
            case MEMX_CONFIG_MPU_GROUP:
                status = WdfRequestRetrieveOutputBuffer(Request, OutputBufferLength, &buffer, &bufLength);
                if (!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfRequestRetrieveOutputBuffer 0x%0x", status);
                } else {
                    information = OutputBufferLength;
                    struct hw_info *hw_info = (struct hw_info *)buffer;
                    for (int chip_id = 0; chip_id < MAX_SUPPORT_CHIP_NUM; chip_id++) {
                        devContext->hw_info.chip.roles[chip_id] = hw_info->chip.roles[chip_id];
                    }
                    status = MemxUtilSendFwCommand(devContext, PCIE_CMD_CONFIG_MPU_GROUP, DEF_BYTE(256), CHIP_ID0);
                }
                break;
            case MEMX_GET_DEVICE_FEATURE: //backward
            case MEMX_SET_DEVICE_FEATURE: //backward
            case MEMX_ADMIN_COMMAND:
                status = WdfRequestRetrieveOutputBuffer(Request, OutputBufferLength, &buffer, &bufLength);
                if (!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfRequestRetrieveOutputBuffer 0x%0x", status);
                } else {
                    information = OutputBufferLength;
                    status = _admin_command_handler(devContext, buffer, &CompleteRequest);
                }
                break;
            case MEMX_SET_THROUGHPUT_INFO:
                CompleteRequest = TRUE;
                break;
            case MEMX_DEVICE_IRQ:
                status = WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &buffer, &bufLength);
                if (!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfRequestRetrieveInputBuffer 0x%0x", status);
                } else {
                    CompleteRequest = TRUE;
                    information = InputBufferLength;
                    memx_device_irq_param_t *ptr = (memx_device_irq_param_t *) buffer;

                    status = MemxPcieTriggerDeviceIrq(devContext, ptr->irq_id, ptr->chip_id);
                }
                break;
            default:
                status = STATUS_INVALID_DEVICE_REQUEST;
                break;
        }
    } else {
        CompleteRequest = TRUE;
        status          = STATUS_DEVICE_NOT_READY;
        information     = MSIX_ERROR_INDICTOR_VAL;
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_QUEUE, "%!FUNC!: devContext->deviceState Not Ready %d\n", devContext->deviceState);
    }

    if ((CompleteRequest == FALSE) && NT_SUCCESS(status)){
        status = WdfRequestForwardToIoQueue(Request, devContext->IoctlPendingQueue);
    }

    WdfSpinLockRelease(devContext->IoQueueLockHandle[QUEUE_IDX_IOCTL]);

    if (CompleteRequest || (!NT_SUCCESS(status)))
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "<-- %!FUNC! status 0x%0x", status);
        WdfRequestCompleteWithInformation(Request, status, information);
    }
}

VOID MemxEvtIoRead(WDFQUEUE Queue, WDFREQUEST Request, size_t Length)
{
    NTSTATUS        status          = STATUS_SUCCESS;
    PUCHAR          readBuffer      = NULL;
    ULONG_PTR       chipIndicter    = MSIX_ERROR_INDICTOR_VAL;
    PDEVICE_CONTEXT devContext      = NULL;
    BOOLEAN         CompleteRequest = FALSE;

    devContext = GetDeviceContext(WdfIoQueueGetDevice(Queue));

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "--> %!FUNC!: Length %d", (int)Length);

    WdfSpinLockAcquire(devContext->IoQueueLockHandle[QUEUE_IDX_READ]);

    if (devContext->deviceState == DEVICE_RUNNING) {
        BOOLEAN empty = ((devContext->IndicatorTail) == (devContext->IndicatorHeader));

        if (devContext->is_read_abort == 0) {
            if (empty) {
                status = WdfRequestForwardToIoQueue(Request, devContext->ReadPendingQueue);
            } else {
                status = WdfRequestRetrieveOutputBuffer(Request, Length, &readBuffer, NULL);
                if (NT_SUCCESS(status)) {
                    chipIndicter = devContext->IndicatorQueue[devContext->IndicatorHeader];
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Found Indicator Queue[%d] has entry %llu\n", devContext->IndicatorHeader, chipIndicter);

                    PUCHAR pbSrcBuf     = (PUCHAR) (devContext->CommonBufferBaseDriver[BUFFER_IDX_READ]);
                    PUCHAR pbDesBuf     = readBuffer;
                    ULONG  copy_length  = 0;

                    pbSrcBuf += devContext->hw_info.fw.egress_dcore_rx_dma_buffer_offset[chipIndicter];
                    pbDesBuf += devContext->hw_info.fw.egress_dcore_rx_dma_buffer_offset[chipIndicter];
                    PULONG plBuffer = (PULONG) pbSrcBuf;
                    ULONG ofmap_flows_triggered_bits    = plBuffer[2];
                    ULONG output_buf_cnt = plBuffer[5];
                    while (ofmap_flows_triggered_bits) {
                        plBuffer = (PULONG)(pbSrcBuf + copy_length);
                        ULONG hw_flow_id = plBuffer[14] & 0x1f;
                        ULONG hw_buffer_count = plBuffer[15];
                        copy_length += (MEMX_MPUIO_COMMON_HEADER_SIZE + hw_buffer_count);
                        if (output_buf_cnt & (1 << hw_flow_id)) {
                            output_buf_cnt &= ~(1 << hw_flow_id);
                        } else {
                            ofmap_flows_triggered_bits &= (ofmap_flows_triggered_bits - 1);
                        }
                    }

                    if((devContext->hw_info.fw.egress_dcore_rx_dma_buffer_offset[chipIndicter] + copy_length) >= Length) {
                        //if unexpected out of range error, just move data in size.
                        pbSrcBuf    = (PUCHAR) (devContext->CommonBufferBaseDriver[BUFFER_IDX_READ]);
                        pbDesBuf    = readBuffer;
                        copy_length = (ULONG) Length;
                    }

                    RtlCopyMemory(pbDesBuf, pbSrcBuf, copy_length);
                    CompleteRequest = TRUE;
                    devContext->IndicatorHeader = (devContext->IndicatorHeader + 1) % QUEUE_SIZE;
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "<-- %!FUNC!: Rx ofmap with data size %u done\n", (ULONG) copy_length);
                }
            }
        } else {
            devContext->is_read_abort = 0;
            CompleteRequest = TRUE;
            status = STATUS_CANCELLED;
            chipIndicter = MSIX_ERROR_INDICTOR_VAL;
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Cancel Request in ReadPendingQueue at MemxEvtIoRead\n");
        }
    } else {
        status = STATUS_DEVICE_NOT_READY;
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_QUEUE, "<-- %!FUNC!: devContext->deviceState Not Ready %d\n", devContext->deviceState);
    }

    WdfSpinLockRelease(devContext->IoQueueLockHandle[QUEUE_IDX_READ]);

    if ((CompleteRequest) || (!NT_SUCCESS(status))) {
        WdfRequestCompleteWithInformation(Request, status, chipIndicter);
    }
}

VOID MemxEvtIoWrite(WDFQUEUE Queue, WDFREQUEST Request, size_t Length)
{
    NTSTATUS            status      = STATUS_SUCCESS;
    PDEVICE_CONTEXT     devContext  = NULL;
    ULONG_PTR           information = MSIX_ERROR_INDICTOR_VAL;
    PULONG              inputBuffer = NULL;
    PULONG              tx_dma_buf  = NULL;

    devContext = GetDeviceContext(WdfIoQueueGetDevice(Queue));

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "--> %!FUNC!: Length %d", (int)Length);

    WdfSpinLockAcquire(devContext->IoQueueLockHandle[QUEUE_IDX_WRITE]);

    do {

        if (devContext->deviceState != DEVICE_RUNNING) {
            status = STATUS_DEVICE_NOT_READY;
            TraceEvents(TRACE_LEVEL_WARNING, TRACE_QUEUE, "<-- %!FUNC!: devContext->deviceState Not Ready %d\n", devContext->deviceState);
            break;
        }

        status = WdfRequestRetrieveInputBuffer(Request, Length, &inputBuffer, NULL);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_WARNING, TRACE_QUEUE, "<-- %!FUNC!: Get inputBuffer fail!!!\n");
            break;
        }

        ULONG writeChipIdx = inputBuffer[2];
        if (writeChipIdx >= MAX_SUPPORT_CHIP_NUM) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "<-- %!FUNC!: writeChipIdx(%d) >= MAX_SUPPORT_CHIP_NUM(%d)", writeChipIdx, MAX_SUPPORT_CHIP_NUM);
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        else
        {
            tx_dma_buf = (PULONG)devContext->CommonBufferBaseDriver[BUFFER_IDX_WRITE];
            RtlCopyMemory((PVOID)tx_dma_buf, inputBuffer, Length);

            if ((writeChipIdx == 0) && (devContext->hw_info.chip.roles[writeChipIdx] == ROLE_SINGLE)) {
                ULONGLONG mapping_ingress_dcore_sram_offset = devContext->hw_info.fw.ingress_dcore_mapping_sram_base[writeChipIdx] - PHYSICAL_SRAM_BASE;
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "%!FUNC!: chipIdx(%d) mapping_ingress_dcore_sram_offset(%llu)\n", writeChipIdx, mapping_ingress_dcore_sram_offset);

                if (mapping_ingress_dcore_sram_offset >= MEMX_PCIE_IFMAP_DMA_COHERENT_BUFFER_SIZE_512KB) {
                    TraceEvents(TRACE_LEVEL_WARNING, TRACE_QUEUE, "<-- %!FUNC!: mapping_ingress_dcore_sram_offset is invalid\n");
                    status = STATUS_INVALID_PARAMETER;
                    break;
                }

                volatile ULONG* chip0_igr_sram_buf = (volatile ULONG*)((PUCHAR)devContext->Bar[devContext->BarInfo.SramIdx].MappingAddress + mapping_ingress_dcore_sram_offset);
                chip0_igr_sram_buf[1] = (ULONG)Length;
                chip0_igr_sram_buf[3] = 0x1; // set host_dma_toggle_bits to sigle chip to send data into mpu di
            }
            else
            {   // only (chip0 && chip0 is ROLE_SINGLE) case use polling dma method others need trigger mpu sw irq 5 to request mpu to move ifmap data which host already put in ingress dcore sram start address.
                if (devContext->hw_info.chip.input_dma_trigger_type[writeChipIdx] == INPUT_DMA_TRIGGER_TYPE_CHIP) {
                    status = MemxPcieTriggerDeviceIrq(devContext, MOVE_SRAMDATA_TO_DIPORT, (UCHAR)writeChipIdx);
                } else {
                    ULONG transfersz = (ULONG)Length;
                    ULONG igr_buf_idx = MemxUtilXflowRead(devContext,  (UCHAR)writeChipIdx, MXCNST_IGR_BUF_WPTR, 0, TO_CONFIG_OUTPUT) & 0x1;

                    memx_dma_transfer_mb(devContext, (UCHAR)writeChipIdx, ARMDMA_CH0, 0x58080000, 0x4006D000+(igr_buf_idx<<18), ((transfersz>>1) & ~0x3));
                    memx_dma_transfer_mb(devContext, (UCHAR)writeChipIdx, ARMDMA_CH1, 0x58080000+((transfersz>>1) & ~0x3), 0x4006D000+(igr_buf_idx<<18)+((transfersz>>1) & ~0x3), transfersz - ((transfersz>>1) & ~0x3));
                }
            }

            if (!NT_SUCCESS(status))
            {
                TraceEvents(TRACE_LEVEL_WARNING, TRACE_QUEUE, "<-- %!FUNC!: MemxUtilXflowWrite fail!!!\n");
                break;
            }
            else
            {
                status = WdfRequestForwardToIoQueue(Request, devContext->WritePendingQueue);
            }
        }
    } while (0);

    WdfSpinLockRelease(devContext->IoQueueLockHandle[QUEUE_IDX_WRITE]);

    if (!NT_SUCCESS(status)) {
        WdfRequestCompleteWithInformation(Request, status, information);
    }
}