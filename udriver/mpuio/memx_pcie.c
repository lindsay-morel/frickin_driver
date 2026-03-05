/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#include "memx_pcie.h"
#include "memx_mpu_comm.h"
#include "memx_ioctl.h"
#include "memx_list.h"
#include "memx_device_manager.h"
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#if __linux__
#include <time.h>
unsigned int write_size = 0;
unsigned int read_size = 0;
unsigned int write_time = 0;
unsigned int write_time_idle = 0;
unsigned int read_time = 0;
unsigned int read_time_idle = 0;
static clockid_t clock_id = CLOCK_REALTIME;
struct timespec prev_time_write;
struct timespec prev_time_read;

#define PMU_MPU_PWR_MGMT_ADDR           (0x20000300)
#define ALL_MPU_CORE_GROUP_ON           (0x222222)
#define PMU_MPU_CORE_GROUP_MASK         (0xF)
#define PMU_MPU_CORE_GROUP_ON           (0x2)
#define PMU_MPU_CORE_GROUP_OFF          (0x1)
#define PMU_MPU_CORE_GROUP_ON_STATUS    (0x4)
#define PMU_MPU_CORE_GROUP_OFF_STATUS   (0x8)
#define PMU_MPU_CORE_GROUP_OFFSET       (0x4)

uint32_t g_pmuConfig[MEMX_MPUIO_MAX_HW_MPU_COUNT] = {0};
#endif


/***************************************************************************//**
* implementation
******************************************************************************/
static uint8_t _check_pcie_abort_status(MemxPcie* pcie);
static uint8_t _check_pcie_read_abort_status(MemxPcie* pcie);
/**
* @brief The main process of background worker 'crawler'. Crawler is designed
* to keep pulling out data from device and write data into the 'shared' ring
* buffer waiting to be decoded.
*/
void* _memx_pcie_background_crawler_main_loop(void *user_data)
{
    if (!user_data) { return NULL; }
    platform_device_t *memx_dev = (platform_device_t *)user_data;
    if (!memx_dev->pdev.pcie) { return NULL; }
    MemxPcie *pcie = memx_dev->pdev.pcie;
#if __linux__
    void *rx_buffer = malloc(MEMX_OFMAP_SRAM_COMMON_HEADER_SIZE);
#elif _WIN32
    void *rx_buffer = pcie->rx_buffer;
#endif
    int read_abort = 0;

    while (1) {
        platform_int32_t chip_idx_indictor = 0;
        if (_check_pcie_abort_status(pcie)) {
            break;
        } else {
            // copy header only for getting msix event
#if __linux__
            chip_idx_indictor = platform_read(pcie->rx_event, &pcie->fd, rx_buffer, MEMX_OFMAP_SRAM_COMMON_HEADER_SIZE);
#elif _WIN32
            chip_idx_indictor = platform_read(pcie->rx_event, &pcie->fd, rx_buffer, MEMX_PCIE_READ_BUFFER_SIZE);
#endif
        }
        if (chip_idx_indictor < 0 || chip_idx_indictor == 512) { break; }
        if (chip_idx_indictor >= 0 && chip_idx_indictor < MAX_SUPPORT_CHIP_NUM) {
            uint8_t *rx_dma_buffer_pos = pcie->rx_buffer;
            rx_dma_buffer_pos += pcie->hw_info.fw.egress_dcore_rx_dma_buffer_offset[chip_idx_indictor];
            uint32_t ofmap_flows_triggered_bits = *((uint32_t *)(rx_dma_buffer_pos + MEMX_OFMAP_SRAM_COMMON_HEADER_TRIGGER_BITS_OFFSET));
            uint32_t output_chip_id = *((uint32_t *)(rx_dma_buffer_pos + MEMX_OFMAP_SRAM_COMMON_HEADER_CHIP_ID_OFFSET));
            uint32_t output_buf_cnt = *((uint32_t *)(rx_dma_buffer_pos + MEMX_OFMAP_SRAM_COMMON_HEADER_OUTPUT_BUF_CNT_OFFSET));

            MEMX_LOG(MEMX_LOG_MPUIO_OFMAP_FLOW, "ofmap_flows_triggered_bits = 0x%x\n", ofmap_flows_triggered_bits);
            MEMX_LOG(MEMX_LOG_MPUIO_OFMAP_FLOW, "ofmap_flows_total_length = %u\n", *((uint32_t *)(rx_dma_buffer_pos + MEMX_OFMAP_SRAM_COMMON_HEADER_TOTAL_LENGTH_OFFSET)));
            MEMX_LOG(MEMX_LOG_MPUIO_OFMAP_FLOW, "output_chip_id = 0x%x\n", output_chip_id);
            while (ofmap_flows_triggered_bits) {
                uint32_t hw_flow_id = (*((uint32_t *)(rx_dma_buffer_pos + MEMX_PCIE_EGRESS_DCORE_HEADER0_OFFSET)) & MEMX_PCIE_EGRESS_DCORE_HEADER0_FLOW_ID_MASK);
                uint32_t hw_buffer_count = *((uint32_t *)(rx_dma_buffer_pos + MEMX_PCIE_EGRESS_DCORE_HEADER1_OFFSET));
                MEMX_LOG(MEMX_LOG_MPUIO_OFMAP_FLOW, "read flow_id[%u], hw_buf_cnt[%u] data form chip\n", hw_flow_id, hw_buffer_count);
                while (1) {
                    MemxRingBufferData *data_node = (MemxRingBufferData *)malloc(sizeof(MemxRingBufferData));
                    if (!data_node) {
                        printf("Memory allocate for MemxRingBufferData fail\n");
                        continue;
                    }
                    data_node->buffer = malloc(hw_buffer_count);
                    if (!data_node->buffer) {
                        printf("Memory allocate for data_node->buffer fail\n");
                        free(data_node);
                        data_node = NULL;
                        continue;
                    }
                    data_node->length = hw_buffer_count;
                    memcpy(data_node->buffer, (const void *)(rx_dma_buffer_pos + MEMX_MPUIO_OFMAP_HEADER_SIZE), hw_buffer_count);

                    // wait untill list empty
                    while (memx_list_count(pcie->flow_ringbuffer[output_chip_id][hw_flow_id]) >= MEMX_MPUIO_MAX_PENDDING_LIST_SIZE) {
                        platform_usleep(100);
                        if (read_abort || _check_pcie_read_abort_status(pcie)) {
                            free(data_node->buffer);
                            data_node->buffer = NULL;
                            free(data_node);
                            data_node = NULL;
                            read_abort = 1;

                            break;
                        }
                    }

                    if (read_abort == 1) break;

                    // push until succes
                    while (memx_list_push(pcie->flow_ringbuffer[output_chip_id][hw_flow_id], data_node));
                    break;
                }
                rx_dma_buffer_pos += (MEMX_MPUIO_OFMAP_HEADER_SIZE + hw_buffer_count);

                if (output_buf_cnt & (1 << hw_flow_id)) {
                    output_buf_cnt &= ~(1 << hw_flow_id);
                } else {
                    ofmap_flows_triggered_bits &= (ofmap_flows_triggered_bits - 1);
                }
            }
            memx_pcie_trigger_device_irq(pcie, (uint8_t)output_chip_id, enable_mpu_nvic_idx_2);
        }
    }
#if __linux__
    free(rx_buffer);
#endif
    return NULL;
}

static uint8_t _check_pcie_abort_status(MemxPcie* pcie)
{
    platform_mutex_lock(&pcie->context_guard);
    uint8_t status = pcie->is_ready_exit;
    platform_mutex_unlock(&pcie->context_guard);
    return  status;
}

static void _set_pcie_abort_status(MemxPcie* pcie)
{
    // stop memx_dev rx background workers so set exit flag first
    platform_mutex_lock(&pcie->context_guard);
    pcie->is_ready_exit = 1;
    // FIXME: shoue we have cascade pcie just like USB, first for input and last for output ?
    if (pcie->role == MEMX_MPU_TYPE_SPLIT) {
        if (platform_ioctl(pcie->ctrl_event, &pcie->fdo, MEMX_ABORT_TRANSFER, NULL, 0)) { printf("failed to ioctl MEMX_ABORT_TRANSFER!\n"); }
    }
    else {
        if (platform_ioctl(pcie->ctrl_event, &pcie->fd, MEMX_ABORT_TRANSFER, NULL, 0)) { printf("failed to ioctl MEMX_ABORT_TRANSFER!\n"); }
    }
    platform_mutex_unlock(&pcie->context_guard);
}

static uint8_t _check_pcie_read_abort_status(MemxPcie* pcie)
{
    return pcie->is_read_abort;
}

static void _set_pcie_read_abort_status(MemxPcie* pcie)
{
    // This flag is no need to use lock to protect, because background read is polling and "close flow" is only one thread.
    pcie->is_read_abort = 1;
#if __linux__
    if (platform_ioctl(pcie->ctrl_event, (pcie->role == MEMX_MPU_TYPE_SPLIT) ? (&pcie->fdo) : (&pcie->fd), MEMX_SET_ABORT_READ, NULL, 0)) {
        printf("failed to ioctl MEMX_SET_ABORT_READ!\n");
    }
#endif
}

static platform_int32_t _check_pcie_open_result(MemxPcie *pDev, platform_uint8_t group_id, uint8_t chip_gen)
{
    platform_int32_t    result      = 0;
    if (pDev == NULL) { return -1; }
#if __linux__
    if (chip_gen == MEMX_MPU_CHIP_GEN_CASCADE_PLUS) {
        if (pDev->role == MEMX_MPU_TYPE_SPLIT) {
            if ((pDev->fdi < 0) || (pDev->fdo < 0)) {
                printf("Can not open device %d: fdi %d fdo %d.\n", group_id, pDev->fdi, pDev->fdo);
                result = -3;
            }
        } else if (pDev->fd < 0) {
            printf("Can not open device %d: fd %d.\n", group_id, pDev->fd);
            result = -3;
        }
    } else {
        result = -5;
    }
#endif

#if _WIN32
    if (chip_gen == MEMX_MPU_CHIP_GEN_CASCADE_PLUS) {
        if (pDev->role == MEMX_MPU_TYPE_SPLIT) {
            if ((pDev->fdi == INVALID_HANDLE_VALUE) || (pDev->fdo == INVALID_HANDLE_VALUE)) {
                printf("Can not open device %d: %u\n", group_id, GetLastError());
                result = -3;
            }
        } else if (pDev->fd == INVALID_HANDLE_VALUE) {
            printf("Can not open device %d: %u\n", group_id, GetLastError());
            result = -3;
        }
    } else {
        result = -5;
    }

    if (result == 0) {
        pDev->rx_event = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (!pDev->rx_event) {
            printf("device %d:rx_event create failed: %u\n", group_id, GetLastError());
            result = -4;
        }

        pDev->tx_event = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (!pDev->tx_event) {
            printf("device %d:tx_event create failed: %u\n", group_id, GetLastError());
            result = -4;
        }

        pDev->ctrl_event = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (!pDev->ctrl_event) {
            printf("device %d:ctrl_event create failed: %u\n", group_id, GetLastError());
            result = -4;
        }
    }
#endif
    return result;
}

static platform_int32_t _memx_pcie_open(MemxPcie *pDev, platform_uint8_t group_id, uint8_t chip_gen)
{
    pMemxDeviceMeta     pDevMeta    = memx_device_group_get_devicemeta(group_id);

    if (pDev == NULL) { return -1; }
    if (pDevMeta == NULL) { return -2; }

    if (chip_gen == MEMX_MPU_CHIP_GEN_CASCADE_PLUS) {
        pDev->role = pDevMeta->role;
        if (pDev->role == MEMX_MPU_TYPE_SPLIT) {
            pDev->fdi = platform_open(pDevMeta->device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS], 0);
            pDev->fdo = platform_open(pDevMeta->device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS_SPLIT], 0);
            pDev->fd  = pDev->fdi;
        } else {
            pDev->fd   = platform_open(pDevMeta->device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS], 0);
        }
    }

    return _check_pcie_open_result(pDev, group_id, chip_gen);
}
#ifdef __linux__
static platform_uint32_t _memx_pcie_get_mmap_buf_size(MemxPcie *pcie)
{
    platform_uint32_t mmap_buf_size = 0;

    if (pcie == NULL) { return 0; }

    switch (pcie->hw_info.chip.pcie_bar_mode)
    {
        case MEMXBAR_XFLOW256MB_SRAM1MB:
            mmap_buf_size = MEMX_PCIE_BAR0_MMAP_SIZE_256MB;
            break;
        case MEMXBAR_SRAM1MB:
            mmap_buf_size = MEMX_PCIE_BAR1_MMAP_SIZE_1MB;
            break;
        case MEMXBAR_XFLOW128MB64B_SRAM1MB:
            mmap_buf_size = MEMX_PCIE_BAR0_MMAP_SIZE_128MB;
            break;
        case MEMXBAR_3BAR_BAR0VB_BAR2CI_16MB_BAR4SRAM:
            mmap_buf_size = MEMX_PCIE_BAR0_MMAP_SIZE_16MB;
            break;
        case MEMXBAR_3BAR_BAR0VB_BAR2CI_64MB_BAR4SRAM:
            mmap_buf_size = MEMX_PCIE_BAR0_MMAP_SIZE_64MB;
            break;
        case MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM:
            mmap_buf_size = MEMX_PCIE_BAR0_MMAP_SIZE_256KB;
            break;
        default:
            printf("wrong pcie bar mode!: %d\n", pcie->hw_info.chip.pcie_bar_mode);
            break;
    }

    return mmap_buf_size;
}
#endif

memx_status memx_sync_device_dma_trigger_type(MemxPcie *pcie, uint8_t group_id)
{
    memx_status status = MEMX_STATUS_OK;
    uint64_t device_dma_trigger_type = INPUT_DMA_TRIGGER_TYPE_CHIP;
    uint8_t chip_id = 0;

    for (chip_id = 0; chip_id < MAX_SUPPORT_CHIP_NUM; chip_id++) {
        if (pcie->hw_info.chip.roles[chip_id] == ROLE_MULTI_FIRST) {
            status = memx_device_get_feature(group_id, chip_id, OPCDOE_GET_DEVICE_DMA_TRIGGER_TYPE, &device_dma_trigger_type);
            if (status == MEMX_STATUS_OK) {
                pcie->hw_info.chip.input_dma_trigger_type[chip_id] = device_dma_trigger_type;
            } else {
                break;
            }
        } else if (pcie->hw_info.chip.roles[chip_id] == ROLE_UNCONFIGURED) {
            break;
        }
    }

    return status;
}

memx_status memx_pcie_create_context(platform_device_t **memx_dev_ptr_src, uint8_t group_id, uint8_t chip_gen)
{
    *memx_dev_ptr_src = (platform_device_t *)malloc(sizeof(platform_device_t));
    platform_device_t *memx_dev = *memx_dev_ptr_src;
    if (!memx_dev) { printf("malloc for memx_dev failed\n"); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    memx_dev->hif = MEMX_MPUIO_INTERFACE_PCIE;
    memx_dev->pdev.pcie = (MemxPcie *)malloc(sizeof(MemxPcie));
    if (!memx_dev->pdev.pcie) { printf("malloc for memx_pcie device failed\n"); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    MemxPcie *pcie = memx_dev->pdev.pcie;
    memset(pcie, 0, sizeof(MemxPcie)); // init. all pointers to nullptr

    platform_int32_t result = _memx_pcie_open(pcie, group_id, chip_gen);
    if (result != 0) {
        printf("memx_pcie_open failed %d\n", result);
        return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
    }

    result = platform_ioctl(pcie->ctrl_event, &pcie->fd, MEMX_GET_HW_INFO, &pcie->hw_info,
                            sizeof(struct hw_info));
    if (result != 0) {
        printf("ioctl MEMX_GET_HW_INFO failed\n");
        return MEMX_STATUS_PLATFORM_IOCTL_FAIL;
    }
    memx_sync_device_dma_trigger_type(pcie, group_id);
    pcie->hw_info.chip.generation = chip_gen;
    pcie->group_id = group_id;

#ifdef __linux__
    platform_uint32_t mmap_buf_size = _memx_pcie_get_mmap_buf_size(pcie);
    if (mmap_buf_size == 0) {
        return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
    }
    pcie->mmap_mpu_base = (volatile uint8_t *)mmap(NULL, mmap_buf_size, PROT_READ | PROT_WRITE, MAP_SHARED, pcie->fd, 0);
    if (pcie->mmap_mpu_base == MAP_FAILED) {
        printf("mmap to mpu fail\n");
        return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
    }
    if ((pcie->hw_info.chip.pcie_bar_mode == MEMXBAR_3BAR_BAR0VB_BAR2CI_16MB_BAR4SRAM) ||
        (pcie->hw_info.chip.pcie_bar_mode == MEMXBAR_3BAR_BAR0VB_BAR2CI_64MB_BAR4SRAM)) {
        pcie->mmap_xflow_vbuf_base = (volatile uint8_t *)mmap(NULL, mmap_buf_size, PROT_READ | PROT_WRITE, MAP_SHARED, pcie->fd, mmap_buf_size);
        if (pcie->mmap_xflow_vbuf_base == MAP_FAILED) {
            printf("mmap to xflow_base virtual buffer fail\n");
            return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
        }
    } else if (pcie->hw_info.chip.pcie_bar_mode == MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM) {
        pcie->mmap_xflow_vbuf_base = (volatile uint8_t *)mmap(NULL, MEMX_PCIE_BAR0_MMAP_SIZE_512KB, PROT_READ | PROT_WRITE, MAP_SHARED, pcie->fd, mmap_buf_size);
        if (pcie->mmap_xflow_vbuf_base == MAP_FAILED) {
            printf("mmap to xflow_base virtual buffer fail\n");
            return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
        }
        pcie->mmap_device_irq_base = (volatile uint8_t *)mmap(NULL, MEMX_PCIE_BAR0_MMAP_SIZE_4KB, PROT_READ | PROT_WRITE, MAP_SHARED, pcie->fd, MEMX_PCIE_BAR0_MMAP_SIZE_4KB);
        if (pcie->mmap_device_irq_base == MAP_FAILED) {
            printf("mmap to device irq base fail\n");
            return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
        }
    }
#endif
    for (uint8_t mpu_group_id = 0; mpu_group_id < pcie->hw_info.chip.group_count; mpu_group_id++) {
        uint8_t chip_id = pcie->hw_info.chip.groups[mpu_group_id].output_chip_id;
        // allocate internal flow-ringbuffer
        for (uint8_t flow_id = 0; flow_id < MEMX_MPUIO_FLOW_RING_BUFFER_NUMBER; flow_id++) {
            pcie->flow_ringbuffer[chip_id][flow_id] = memx_list_create();
            if (!pcie->flow_ringbuffer[chip_id][flow_id]) { printf("pcie->flow_ringbuffer[%d][%d] create failed\n", chip_id, flow_id); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
        }
    }
#if __linux__
    // use mmap for rx_buffer and tx_buffer, skip malloc
    pcie->rx_buffer = (uint8_t *)mmap(NULL, (MEMX_PCIE_DMA_BUFFER_SIZE), PROT_READ | PROT_WRITE, MAP_SHARED, pcie->fd, 0);
    if (pcie->rx_buffer == MAP_FAILED) {
        printf("mmap to mpu fail\n");
        return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
    }
    pcie->tx_buffer = (uint8_t *)pcie->rx_buffer + MEMX_PCIE_READ_BUFFER_SIZE;
#elif _WIN32
    pcie->rx_buffer = (uint8_t *)malloc(MEMX_PCIE_READ_BUFFER_SIZE);
    pcie->tx_buffer = (uint8_t *)malloc(MEMX_PCIE_WRITE_BUFFER_SIZE);

    if (!pcie->rx_buffer) { printf("pcie->rx_buffer create failed\n"); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    memset(pcie->rx_buffer, 0x0, MEMX_PCIE_READ_BUFFER_SIZE);

    if (!pcie->tx_buffer) { printf("pcie->tx_buffer create failed\n"); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    memset(pcie->tx_buffer, 0x0, MEMX_PCIE_WRITE_BUFFER_SIZE);
#endif

    for (uint8_t chip_id = 0; chip_id < MEMX_MPUIO_MAX_HW_MPU_COUNT; chip_id++) {
        if (platform_mutex_create(&pcie->xflow_write_guard[chip_id], NULL)) {
            printf("pcie xflow_write_guard %d create failed\n", chip_id);
            return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
        }
    }
    if (platform_mutex_create(&pcie->xflow_access_guard_first_chip_control, NULL)) {
        printf("pcie xflow_access_guard_first_chip_control create failed\n");
        return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
    }
    if (platform_mutex_create(&pcie->write_guard, NULL)) {
        printf("pcie write_guard create failed\n");
        return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
    }
    if (platform_mutex_create(&pcie->context_guard, NULL)) {
        printf("pcie context_guard create failed\n");
        return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
    }

    // start background workers
    if (platform_thread_create(&pcie->background_crawler, NULL, &_memx_pcie_background_crawler_main_loop, memx_dev)) {
        printf("pcie background_crawler create failed\n");
        return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
    }
#if _WIN32
    SetThreadPriority(&pcie->background_crawler, THREAD_PRIORITY_TIME_CRITICAL); // THREAD_PRIORITY_TIME_CRITICAL
#endif

    return MEMX_STATUS_OK;
}

memx_status memx_pcie_operation(MemxMpuIo *mpuio, uint8_t chip_id, int32_t cmd_id, void *data, uint32_t size, int32_t timeout)
{
    unused(chip_id);
    unused(size);
    unused(timeout);
    memx_status status = MEMX_STATUS_OK;
    if (!mpuio || !mpuio->context) { MEMX_LOG(MEMX_LOG_MPUIO_OFMAP_FLOW, "invaild context\n"); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    platform_device_t *memx_dev = (platform_device_t *)mpuio->context;
    if (!memx_dev->pdev.pcie) { MEMX_LOG(MEMX_LOG_MPUIO_OFMAP_FLOW, "invaild memx pcie device ptr\n"); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    MemxPcie *pcie = memx_dev->pdev.pcie;
    if (size != 0 && !data) { MEMX_LOG(MEMX_LOG_MPUIO_OFMAP_FLOW, "size(%u) but data pointer is NULL\n", size); return MEMX_STATUS_MPUIO_INVALID_DATA; }

    switch (cmd_id) {

        case MEMX_CMD_READ_TOTAL_CHIP_COUNT: {
            memcpy(data, &pcie->hw_info.chip.total_chip_cnt, 1);
        } break;

        case MEMX_CMD_GET_FW_DOWNLOAD_STATUS: {
            int32_t firmware_update_status = memx_xflow_read(pcie, chip_id,  pcie->hw_info.fw.firmware_download_sram_base, 0, true);
            if (firmware_update_status < 0) {
                MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "MEMX_CMD_GET_FW_DOWNLOAD_STATUS fail!, result(%d)\n", firmware_update_status);
                return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
            }
            if (firmware_update_status == 0x4D4D4D4D) {
                MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "FW still non-download yap!, firmware_update_status(%d)\n", firmware_update_status);
                return MEMX_STATUS_MPU_DOWNLOAD_FIRMWARE_FAIL;
            }
        } break;

        case MEMX_CMD_CONFIG_MPU_GROUP: {
            if (pcie->hw_info.chip.generation != MEMX_MPU_CHIP_GEN_CASCADE_PLUS) {
                return MEMX_STATUS_MPUIO_INVALID_CHIP_GEN;
            }
            uint8_t mpu_group_config = *(uint8_t*)data;
            // 1. update chip role base on differnt mpu group config
            status = mpuio_comm_config_mpu_group(mpu_group_config, &pcie->hw_info);
            if (memx_status_error(status)) {
                return status;
            }

            // 2. send updated chip role to firmware
            if (platform_ioctl(pcie->ctrl_event, &pcie->fd, MEMX_CONFIG_MPU_GROUP, &pcie->hw_info, sizeof(struct hw_info))) {
                printf("ioctl MEMX_CONFIG_MPU_GROUP failed\n");
                return MEMX_STATUS_PLATFORM_IOCTL_FAIL;
            }
            pcie->hw_info.chip.generation = MEMX_MPU_CHIP_GEN_CASCADE_PLUS;
            // 3. update and allocate internal flow-ringbuffer for each group if new group detected
            for (uint8_t mpu_group_id = 0; mpu_group_id < pcie->hw_info.chip.group_count; mpu_group_id++) {
                uint8_t output_chip_id = pcie->hw_info.chip.groups[mpu_group_id].output_chip_id;
                // allocate internal flow-ringbuffer
                for (uint8_t flow_id = 0; flow_id < MEMX_MPUIO_FLOW_RING_BUFFER_NUMBER; flow_id++) {
                    if (!pcie->flow_ringbuffer[output_chip_id][flow_id]) {
                        pcie->flow_ringbuffer[output_chip_id][flow_id] = memx_list_create();
                    }
                    if (!pcie->flow_ringbuffer[output_chip_id][flow_id]) { printf("pcie->flow_ringbuffer[%d][%d] create failed\n", output_chip_id, flow_id); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
                }
            }

            // 4. update mpuio hw_info for mpu layer mpu_group info
            memcpy((void*)&mpuio->hw_info, (void *)&pcie->hw_info, sizeof(struct hw_info));
        } break;
        case MEMX_CMD_RESET_DEVICE: {
            uint32_t delay_for_mpu_reset = 0;

            delay_for_mpu_reset = (pcie->hw_info.chip.total_chip_cnt) ? (2000 * (pcie->hw_info.chip.total_chip_cnt - 1) + 1000) : 1000;
            if (pcie->hw_info.chip.generation != MEMX_MPU_CHIP_GEN_CASCADE_PLUS) {
                return MEMX_STATUS_MPUIO_INVALID_CHIP_GEN;
            }
            memx_pcie_trigger_device_irq(pcie, chip_id, reset_device_idx_3);
            platform_usleep(delay_for_mpu_reset);
            _set_pcie_abort_status(pcie);
        } break;
        case MEMX_CMD_SET_QSPI_RESET_RELEASE: {
            uint32_t reset_register_value = memx_xflow_read(pcie, chip_id,  0x20000208, 0, false);

            if (*(uint16_t*)data) {
                reset_register_value |= (MEMX_MPU_RESET_QSPI_M_BIT | MEMX_MPU_RESET_QSPI_S_BIT);
            } else {
                reset_register_value &= (~(MEMX_MPU_RESET_QSPI_M_BIT | MEMX_MPU_RESET_QSPI_S_BIT));
            }

            memx_xflow_write(pcie, chip_id, 0x20000208, 0, reset_register_value, false);
        } break;
        case MEMX_CMD_GET_QSPI_RESET_RELEASE: {
            uint32_t register_value = memx_xflow_read(pcie, chip_id,  0x20000208, 0, false);

            register_value = ((register_value & (MEMX_MPU_RESET_QSPI_M_BIT | MEMX_MPU_RESET_QSPI_S_BIT)) >> MEMX_MPU_RESET_QSPI_M_BIT_POSITION);
            *((uint32_t *)data) = register_value;
        } break;
    default:
        status = MEMX_STATUS_INVALID_OPCODE;
    }

    return status;
}
#if __linux__
void memx_pcie_throughput_add(unsigned int *current_size, unsigned int additional_size)
{
    if (*current_size > (0xffffffff - additional_size)) {
        write_size = 0;
        read_size = 0;
        write_time = 0;
        write_time_idle = 0;
        read_time = 0;
        read_time_idle = 0;
    } else {
        *current_size += additional_size;
    }
}

void memx_pcie_set_throughput_info(MemxPcie *pcie)
{
    struct memx_throughput_info memx_throughput_info;
    memx_throughput_info.stream_write_us = write_time + write_time_idle;
    memx_throughput_info.stream_write_kb = write_size / KBYTE;
    memx_throughput_info.stream_read_us = read_time + read_time_idle;
    memx_throughput_info.stream_read_kb =  read_size / KBYTE;

    // stop memx_dev rx background workers so set exit flag first
    platform_mutex_lock(&pcie->context_guard);
    if (platform_ioctl(pcie->ctrl_event, &pcie->fd, MEMX_SET_THROUGHPUT_INFO, &memx_throughput_info, sizeof(struct memx_throughput_info))) {
        printf("failed to ioctl MEMX_SET_THROUGHPUT_INFO!\n");
    }
    platform_mutex_unlock(&pcie->context_guard);
    write_size = 0;
    read_size = 0;
    write_time = 0;
    write_time_idle = 0;
    read_time = 0;
    read_time_idle = 0;
}

void memx_pcie_clear_dummy_read(MemxPcie *pcie)
{
    platform_int32_t output_chip_id = 0;

    while (1) {
        output_chip_id = platform_ioctl(pcie->ctrl_event, (pcie->role == MEMX_MPU_TYPE_SPLIT) ? (&pcie->fdo) : (&pcie->fd), MEMX_DUMMY_READ, NULL, 0);

        if (output_chip_id >= 0 && output_chip_id < MAX_SUPPORT_CHIP_NUM) {
            memx_pcie_trigger_device_irq(pcie, (uint8_t)output_chip_id, enable_mpu_nvic_idx_2);
        } else {
            break;
        }
    }
}
#endif
memx_status memx_pcie_set_read_abort_start(MemxMpuIo *mpuio)
{
    if (!mpuio || !mpuio->context) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }

    platform_device_t *memx_dev = (platform_device_t *)mpuio->context;

    if (memx_dev->pdev.pcie) {
        _set_pcie_read_abort_status(memx_dev->pdev.pcie);
        return MEMX_STATUS_OK;
    } else {
        return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
    }
}
void memx_pcie_destroy(MemxMpuIo *mpuio)
{
    if (!mpuio || !mpuio->context) { return; }
    // start to free resource for read/write file
    platform_device_t *memx_dev = (platform_device_t *)mpuio->context;
    if (!memx_dev->pdev.pcie) {
        free(memx_dev);
        memx_dev = NULL;
        return;
    }
    MemxPcie *pcie = memx_dev->pdev.pcie;

#if __linux__
    memx_pcie_set_throughput_info(pcie);
#endif
    _set_pcie_abort_status(pcie);

    platform_thread_join(&pcie->background_crawler, NULL);
#if __linux__
    if (_check_pcie_read_abort_status(pcie)) {
        memx_pcie_clear_dummy_read(pcie);
    }
#endif

    for (uint8_t mpu_group_id = 0; mpu_group_id < pcie->hw_info.chip.group_count; mpu_group_id++) {
        uint8_t chip_id = pcie->hw_info.chip.groups[mpu_group_id].output_chip_id;
        // release internal ringbuffer
        for (uint8_t flow_id = 0; flow_id < MEMX_MPUIO_FLOW_RING_BUFFER_NUMBER; ++flow_id) {
            while (memx_list_count(pcie->flow_ringbuffer[chip_id][flow_id])) {
                MemxRingBufferData *data_node = (MemxRingBufferData *)memx_list_pop(pcie->flow_ringbuffer[chip_id][flow_id]);
                free(data_node->buffer);
                data_node->buffer = NULL;
                free(data_node);
                data_node = NULL;
            }
            memx_list_destroy(pcie->flow_ringbuffer[chip_id][flow_id]);
        }
    }
#if __linux__
    platform_uint32_t mmap_buf_size = _memx_pcie_get_mmap_buf_size(pcie);
    if (mmap_buf_size != 0) {
        munmap((void *)pcie->mmap_mpu_base, mmap_buf_size);
    }
    if ((pcie->hw_info.chip.pcie_bar_mode == MEMXBAR_3BAR_BAR0VB_BAR2CI_16MB_BAR4SRAM) ||
        (pcie->hw_info.chip.pcie_bar_mode == MEMXBAR_3BAR_BAR0VB_BAR2CI_64MB_BAR4SRAM)) {
        munmap((void *)pcie->mmap_xflow_vbuf_base, mmap_buf_size);
    }
    if (pcie->hw_info.chip.pcie_bar_mode == MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM) {
        munmap((void *)pcie->mmap_xflow_vbuf_base, MEMX_PCIE_BAR0_MMAP_SIZE_512KB);
        munmap((void *)pcie->mmap_device_irq_base, MEMX_PCIE_BAR0_MMAP_SIZE_4KB);
    }
    munmap((void *)pcie->rx_buffer, (MEMX_PCIE_DMA_BUFFER_SIZE));
    if (pcie->role == MEMX_MPU_TYPE_SPLIT) {
        if (pcie->fdi > 0) { platform_close(&pcie->fdi);}
        if (pcie->fdo > 0) { platform_close(&pcie->fdo);}
    } else {
        if (pcie->fd > 0) { platform_close(&pcie->fd);}
    }
#elif _WIN32
    free(pcie->rx_buffer); pcie->rx_buffer = NULL;
    free(pcie->tx_buffer); pcie->tx_buffer = NULL;
    if (pcie->tx_event) { platform_close(&pcie->tx_event); }
    if (pcie->rx_event) { platform_close(&pcie->rx_event); }
    if (pcie->ctrl_event) { platform_close(&pcie->ctrl_event); }
    if (pcie->role == MEMX_MPU_TYPE_SPLIT) {
        platform_close(&pcie->fdi);
        pcie->fdi = NULL;
        platform_close(&pcie->fdo);
        pcie->fdo = NULL;
    } else {
        platform_close(&pcie->fd);
        pcie->fd = NULL;
    }
#endif
    for (uint8_t chip_id = 0; chip_id < MEMX_MPUIO_MAX_HW_MPU_COUNT; chip_id++) {
        platform_mutex_unlock(&pcie->xflow_write_guard[chip_id]);
        platform_mutex_destory(&pcie->xflow_write_guard[chip_id]);
    }
    platform_mutex_unlock(&pcie->xflow_access_guard_first_chip_control);
    platform_mutex_destory(&pcie->xflow_access_guard_first_chip_control);
    platform_mutex_unlock(&pcie->write_guard);
    platform_mutex_destory(&pcie->write_guard);
    platform_mutex_unlock(&pcie->context_guard);
    platform_mutex_destory(&pcie->context_guard);
    free(pcie);
    pcie = NULL;
    free(memx_dev);
    memx_dev = NULL;
}

static memx_status move_sram_data_to_host_buf(platform_device_t *memx_dev, uint8_t chip_id, uint8_t flow_id, uint8_t *data, int32_t length)
{
    if (!memx_dev || !memx_dev->pdev.pcie) { MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "Invalid memx pcie device\n"); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    MemxPcie *pcie = memx_dev->pdev.pcie;
    // copy common ifmap header(64B) and ifmap data to ingress dcore sram start address.
    MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "flow[%u] write %d data to chip[%u]\n", flow_id, length, chip_id);

    platform_mutex_lock(&pcie->write_guard);
    platform_uint32_t *p_tx_buffer = (platform_uint32_t*)(pcie->tx_buffer);
    p_tx_buffer[0] = flow_id;
    p_tx_buffer[1] = length;
    p_tx_buffer[2] = chip_id;
    platform_memcpy((void *)&p_tx_buffer[16], data, length);
    platform_int32_t total_len = platform_write(pcie->tx_event, &pcie->fd, p_tx_buffer, length + MEMX_MPUIO_IFMAP_HEADER_SIZE);
    platform_mutex_unlock(&pcie->write_guard);

    if (total_len <= 0) {
        printf("move_sram_data_to_host_buf error %d \n", total_len);
        return MEMX_STATUS_PLATFORM_WRITE_FAIL;
    }
    return MEMX_STATUS_OK;
}

memx_status memx_pcie_stream_write(MemxMpuIo *mpuio, uint8_t chip_id, uint8_t flow_id, uint8_t *data, int32_t length, int32_t *transferred, int32_t timeout)
{
    unused(timeout);
    if (!mpuio || !mpuio->context) { MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "invaild context\n"); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    platform_device_t *memx_dev = (platform_device_t *)mpuio->context;
    if (!memx_dev->pdev.pcie) { MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "Invalid memx pcie device\n"); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    MemxPcie *pcie = memx_dev->pdev.pcie;
    if (pcie->hw_info.chip.generation != MEMX_MPU_CHIP_GEN_CASCADE_PLUS) {
        MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "Chip generation(%u) non-support memx_dev interface.\n", pcie->hw_info.chip.generation);
        return MEMX_STATUS_MPUIO_INVALID_CHIP_GEN;
    }
    if (chip_id >= MEMX_MPUIO_MAX_HW_MPU_COUNT) { MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "Invalid chip id(%u)", chip_id); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    if (flow_id >= MEMX_MPUIO_FLOW_RING_BUFFER_NUMBER) { MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "invaild flow id(%u)\n", flow_id); return MEMX_STATUS_MPUIO_INVALID_FLOW_ID; }
    if (!data) { MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "tx data is NULL\n"); return MEMX_STATUS_MPUIO_INVALID_DATA; }
    if (length <= 0) { MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "tx invaild length(%d)\n", length); return MEMX_STATUS_MPUIO_INVALID_DATALEN; }

#if __linux__
    struct timespec start_time;
    struct timespec end_time;
    clock_gettime(clock_id, &start_time);
    if (prev_time_write.tv_sec != 0) {
        memx_pcie_throughput_add(&write_time_idle, (start_time.tv_sec - prev_time_write.tv_sec) * 1000000 + (start_time.tv_nsec - prev_time_write.tv_nsec) / 1000);
    }
#endif
    // always reset actual transferred size
    if (transferred) { *transferred = 0; }
    int32_t remaining_length = length;
    int32_t max_transmit_length = (MEMX_PCIE_OFMAP_DMA_COHERENT_MMAP_SIZE_512KB/2 - MEMX_MPUIO_IFMAP_HEADER_SIZE);
    memx_status result = MEMX_STATUS_OK;
    while (remaining_length) {
        int32_t transmit_length = (remaining_length >= max_transmit_length) ? max_transmit_length : remaining_length;
        result = move_sram_data_to_host_buf(memx_dev, chip_id, flow_id, data, transmit_length);
        if (result != MEMX_STATUS_OK) {
            MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "flow[%u] write %d data to chip[%u] fail\n", flow_id, transmit_length, chip_id);
            return MEMX_STATUS_OTHERS;
        }
        data += transmit_length;
        remaining_length -= transmit_length;
    }
    if (transferred) { *transferred = length - remaining_length; }
    MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "flow[%u] acctually write %d data to chip[%u] success\n", flow_id, *transferred, chip_id);
#if __linux__
    clock_gettime(clock_id, &end_time);
    memx_pcie_throughput_add(&write_size, length);
    memx_pcie_throughput_add(&write_time, (end_time.tv_sec - start_time.tv_sec) * 1000000 + (end_time.tv_nsec - start_time.tv_nsec) / 1000);
    prev_time_write.tv_sec = end_time.tv_sec;
    prev_time_write.tv_nsec = end_time.tv_nsec;
#endif

    return result;
}

memx_status memx_pcie_stream_read(MemxMpuIo *mpuio, uint8_t chip_id, uint8_t flow_id, uint8_t *data, int32_t length, int32_t *transferred, int32_t timeout)
{
    unused(timeout);

    if (!mpuio || !mpuio->context) { MEMX_LOG(MEMX_LOG_MPUIO_OFMAP_FLOW, "invaild context\n"); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    platform_device_t *memx_dev = (platform_device_t *)mpuio->context;
    if (!memx_dev->pdev.pcie) { MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "Invalid memx pcie device\n"); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    MemxPcie *pcie = memx_dev->pdev.pcie;
    if (pcie->hw_info.chip.generation != MEMX_MPU_CHIP_GEN_CASCADE_PLUS) {
        MEMX_LOG(MEMX_LOG_MPUIO_OFMAP_FLOW, "Chip generation(%u) non-support memx_dev interface.\n", pcie->hw_info.chip.generation);
        return MEMX_STATUS_MPUIO_INVALID_CHIP_GEN;
    }
    if (flow_id >= MEMX_MPUIO_FLOW_RING_BUFFER_NUMBER) { MEMX_LOG(MEMX_LOG_MPUIO_OFMAP_FLOW, "invaild flow id(%u)\n", flow_id); return MEMX_STATUS_MPUIO_INVALID_FLOW_ID; }
    if (!data) { MEMX_LOG(MEMX_LOG_MPUIO_OFMAP_FLOW, "rx data is NULL\n"); return MEMX_STATUS_MPUIO_INVALID_DATA; }
    if (length <= 0) { MEMX_LOG(MEMX_LOG_MPUIO_OFMAP_FLOW, "rx invaild length(%d)\n", length); return MEMX_STATUS_MPUIO_INVALID_DATALEN; }
    if (transferred) { *transferred = 0; }

#if __linux__
    struct timespec start_time;
    struct timespec end_time;
    clock_gettime(clock_id, &start_time);
    if (prev_time_read.tv_sec != 0) {
        memx_pcie_throughput_add(&read_time_idle, (start_time.tv_sec - prev_time_read.tv_sec) * 1000000 + (start_time.tv_nsec - prev_time_read.tv_nsec) / 1000);
    }
#endif
    int32_t actual_size = 0;
    MemxRingBufferData *data_node = (MemxRingBufferData *)memx_list_pop(pcie->flow_ringbuffer[chip_id][flow_id]);
    if (data_node) {
        memcpy(data, data_node->buffer, data_node->length);
        free(data_node->buffer);
        data_node->buffer = NULL;
        actual_size = data_node->length;
        free(data_node);
        data_node = NULL;
    }

    if (transferred) { *transferred = actual_size; }
#if __linux__
    clock_gettime(clock_id, &end_time);
    memx_pcie_throughput_add(&read_size, actual_size);
    memx_pcie_throughput_add(&read_time, (end_time.tv_sec - start_time.tv_sec) * 1000000 + (end_time.tv_nsec - start_time.tv_nsec) / 1000);
    prev_time_read.tv_sec = end_time.tv_sec;
    prev_time_read.tv_nsec = end_time.tv_nsec;
#endif
    return MEMX_STATUS_OK;
}

memx_status memx_pcie_download_firmware(MemxMpuIo *mpuio, const char *file_path)
{
    if (!mpuio || !mpuio->context) { MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "invaild context\n"); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    platform_device_t *memx_dev = (platform_device_t *)mpuio->context;
    if (!memx_dev->pdev.pcie) { MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "Invalid memx pcie device\n"); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    MemxPcie *pcie = memx_dev->pdev.pcie;
    if (pcie->hw_info.chip.generation != MEMX_MPU_CHIP_GEN_CASCADE_PLUS) {
        MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "Chip generation(%u) non-support memx_dev interface.\n", pcie->hw_info.chip.generation);
        return MEMX_STATUS_MPUIO_INVALID_CHIP_GEN;
    }

    if (!file_path) { MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "file_path is NULL.\n"); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    char *file_name = mpuio_comm_find_file_name(file_path);
    if (!file_name) { MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "file_name is NULL.\n"); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }

    struct memx_firmware_bin memx_firmware_bin;
    strncpy(memx_firmware_bin.name, file_name, FILE_NAME_LENGTH - 1); // remove eof

#if __linux__
    memx_firmware_bin.request_firmware_update_in_linux = (memcmp(file_path, "/lib/firmware", 13) == 0) ? true : false;
#endif
    if (memx_status_error(memx_load_firmware(file_path, &(memx_firmware_bin.buffer), &memx_firmware_bin.size))) {
        free(memx_firmware_bin.buffer);
        memx_firmware_bin.buffer = NULL;
        return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
    }
    int32_t ret = platform_ioctl(pcie->ctrl_event, &pcie->fd, MEMX_DOWNLOAD_FIRMWARE, &memx_firmware_bin, sizeof(memx_firmware_bin));
    if (ret) {
        printf("failed to ioctl MEMX_DOWNLOAD_FIRMWARE!\n");
        return MEMX_STATUS_PLATFORM_IOCTL_FAIL;
    }
    return MEMX_STATUS_OK;
}

static void init_wtmem_and_fmem_group(MemxPcie *memx_dev, uint8_t base_chip_id, uint8_t num_of_mpu){
    if (!memx_dev) { printf("init_wtmem_and_fmem_group: Invalid memx_dev\n"); return; }
    if (base_chip_id >= MEMX_MPUIO_MAX_HW_MPU_COUNT) { printf("init_wtmem_and_fmem_group: Invalid base_chip_id(%u)\n", base_chip_id); return; }
    if (( base_chip_id + num_of_mpu)> MEMX_MPUIO_MAX_HW_MPU_COUNT) { printf("init_wtmem_and_fmem_group: Invalid base_chip_id(%u) num_of_mpu(%u)\n", base_chip_id, num_of_mpu); return; }

    for(int8_t chip_id = base_chip_id + num_of_mpu - 1; chip_id >= base_chip_id; --chip_id){
        //reset from back to avoid xflow issue
        memx_pcie_trigger_device_irq(memx_dev, chip_id, init_wtmem_and_fmem_idx_6);
    }
    platform_usleep(1000);
}

static void reset_mpu_group(MemxPcie *memx_dev, uint8_t base_chip_id, uint8_t num_of_mpu)
{
    if (!memx_dev) { printf("reset_mpu_group: Invalid memx_dev\n"); return; }
    if (base_chip_id >= MEMX_MPUIO_MAX_HW_MPU_COUNT) { printf("reset_mpu_group: Invalid base_chip_id(%u)\n", base_chip_id); return; }
    if (( base_chip_id + num_of_mpu)> MEMX_MPUIO_MAX_HW_MPU_COUNT) { printf("reset_mpu_group: Invalid base_chip_id(%u) num_of_mpu(%u)\n", base_chip_id, num_of_mpu); return; }

    for(int8_t chip_id = base_chip_id + num_of_mpu - 1; chip_id >= base_chip_id; --chip_id){
        //reset from back to avoid xflow issue
        memx_pcie_trigger_device_irq(memx_dev, chip_id, reset_mpu_idx_7);
    }
    platform_usleep(1000);
}

void memx_pcie_trigger_device_irq(struct _MemxPcie *memx_dev, uint8_t chip_id, xflow_mpu_sw_irq_idx_t sw_irq_idx)
{
    uint32_t write_value = 0;

    if (memx_xflow_basic_check(memx_dev, chip_id)) {
        printf("xflow_trigger_mpu_sw_irq: basic check fail.\n");
    } else {
        switch (sw_irq_idx) {
        case enable_mpu_nvic_idx_2:
        case reset_device_idx_3:
        case fw_cmd_idx_4:
        case move_sram_data_to_di_port_idx_5:
        case init_wtmem_and_fmem_idx_6:
        case reset_mpu_idx_7:
            if (memx_dev->hw_info.chip.pcie_bar_mode != MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM) {
                write_value = (0x1 << sw_irq_idx);
                memx_xflow_write(memx_dev, (uint8_t)chip_id, 0x30400008, 0x0, write_value, true);
            } else {
#if __linux__
                volatile uint32_t *device_irq_register_addr = NULL;
                write_value = sw_irq_idx - 2;
                device_irq_register_addr = (uint32_t*)(memx_dev->mmap_device_irq_base + MEMX_PCIE_IRQ_OFFSET(chip_id, write_value));
                *device_irq_register_addr = chip_id;
#elif _WIN32
                memx_device_irq_param_t parameter = {0};
                parameter.chip_id = chip_id;
                parameter.irq_id = sw_irq_idx;
                int32_t result = platform_ioctl(memx_dev->ctrl_event, &memx_dev->fd, MEMX_DEVICE_IRQ, &parameter, sizeof(memx_device_irq_param_t));
                if (result < 0) {
                    printf("memx_pcie_trigger_device_irq: ioctl fail. result(%d)\n", result);
                }
#endif
            }
        break;
        default:
            printf("Invalid sw_irq_idx(%u), it should not be used.\n", sw_irq_idx);
        }
    }
}

#if __linux__
static void config_mpu_group_pmu(MemxPcie *memx_dev, uint8_t base_chip_id, uint8_t num_of_mpu)
{
    if (!memx_dev) { printf("config_mpu_group_pmu: Invalid memx_dev\n"); return; }
    if (base_chip_id >= MEMX_MPUIO_MAX_HW_MPU_COUNT) { printf("config_mpu_group_pmu: Invalid base_chip_id(%u)", base_chip_id); return; }
    if (( base_chip_id + num_of_mpu)> MEMX_MPUIO_MAX_HW_MPU_COUNT) { printf("config_mpu_group_pmu: base_chip_id(%u) Invalid num_of_mpu(%u)\n", base_chip_id, num_of_mpu); return; }

    for(int8_t chip_id = base_chip_id + num_of_mpu - 1; chip_id >= base_chip_id; --chip_id){
        if(g_pmuConfig[chip_id] != ALL_MPU_CORE_GROUP_ON){
            memx_xflow_write(memx_dev, chip_id, PMU_MPU_PWR_MGMT_ADDR, 0, g_pmuConfig[chip_id], false);
        }
    }

    for(int8_t chip_id = base_chip_id + num_of_mpu - 1; chip_id >= base_chip_id; --chip_id){
        if(g_pmuConfig[chip_id] != ALL_MPU_CORE_GROUP_ON){
            uint32_t indicator = 0;
            uint32_t temp = g_pmuConfig[chip_id];
            for(uint8_t cg = 0; cg < MEMX_MPU_MAX_CORE_GROUP; ++cg){
                uint32_t op = temp & PMU_MPU_CORE_GROUP_MASK;
                if(op == PMU_MPU_CORE_GROUP_ON){
                    indicator |= (PMU_MPU_CORE_GROUP_ON_STATUS <<(cg * PMU_MPU_CORE_GROUP_OFFSET));
                }
                if(op == PMU_MPU_CORE_GROUP_OFF){
                    indicator |= (PMU_MPU_CORE_GROUP_OFF_STATUS <<(cg * PMU_MPU_CORE_GROUP_OFFSET));
                }
                temp >>= PMU_MPU_CORE_GROUP_OFFSET;
            }

            do{
                temp = memx_xflow_read(memx_dev, chip_id, PMU_MPU_PWR_MGMT_ADDR, 0, false);
            }while((temp&indicator) != indicator);

            memx_xflow_write(memx_dev, chip_id, PMU_MPU_PWR_MGMT_ADDR, 0, 0, false);
        }
    }
}

static memx_status _download_dfp_memory(MemxPcie *memx_dev, uint8_t type, uint8_t chip_id){
    memx_status         status  = MEMX_STATUS_OK;
    uint32_t            length  = 0;
    uint16_t            uwidx   = 0;
    uint8_t*            ubBuf   = NULL;
    platform_int32_t    result  = 0;
    struct transport_cmd       cmd;
    memset(&cmd, 0, sizeof(struct transport_cmd));

    platform_mutex_lock(&memx_dev->write_guard);
    for(memx_dev->download_wmem_context[chip_id].uwCurEntryIdx = 0; memx_dev->download_wmem_context[chip_id].uwCurEntryIdx < memx_dev->download_wmem_context[chip_id].uwValidEntryCnt; ++memx_dev->download_wmem_context[chip_id].uwCurEntryIdx){
        uwidx = memx_dev->download_wmem_context[chip_id].uwCurEntryIdx;
        ubBuf = (uint8_t* ) memx_dev->download_wmem_context[chip_id].pContext[uwidx].src_buf_base;

        while(memx_dev->download_wmem_context[chip_id].pContext[uwidx].written_size < memx_dev->download_wmem_context[chip_id].pContext[uwidx].total_size){
            if((memx_dev->download_wmem_context[chip_id].pContext[uwidx].total_size - memx_dev->download_wmem_context[chip_id].pContext[uwidx].written_size) > MEMX_PCIE_OFMAP_DMA_COHERENT_MMAP_SIZE_512KB){
                length = MEMX_PCIE_OFMAP_DMA_COHERENT_MMAP_SIZE_512KB;
            } else {
                length = (memx_dev->download_wmem_context[chip_id].pContext[uwidx].total_size - memx_dev->download_wmem_context[chip_id].pContext[uwidx].written_size);
            }
            platform_memcpy((void *)memx_dev->tx_buffer, ubBuf + memx_dev->download_wmem_context[chip_id].pContext[uwidx].written_size, length);

            cmd.SQ.subOpCode = type;
            cmd.SQ.cdw2 = memx_dev->download_wmem_context[chip_id].pContext[uwidx].chip_id;
            cmd.SQ.cdw3 = memx_dev->download_wmem_context[chip_id].pContext[uwidx].des_addr_base;
            cmd.SQ.cdw4 = memx_dev->download_wmem_context[chip_id].pContext[uwidx].written_size;
            cmd.SQ.cdw5 = length;
            result = platform_ioctl(memx_dev->ctrl_event, &memx_dev->fd, MEMX_VENDOR_CMD, &cmd, sizeof(struct transport_cmd));
            if (result != 0) {
                printf("failed %d Move data from 0x%p to 0x%p with length %d \r\n", uwidx, ubBuf + memx_dev->download_wmem_context[chip_id].pContext[uwidx].written_size, memx_dev->tx_buffer, length);
                status = MEMX_STATUS_PLATFORM_IOCTL_FAIL;
                goto done;
            }
            memx_dev->download_wmem_context[chip_id].pContext[uwidx].written_size += length;
        }
    }
done:
    platform_mutex_unlock(&memx_dev->write_guard);
    free(memx_dev->download_wmem_context[chip_id].pContext);
    memset((void *) &(memx_dev->download_wmem_context[chip_id]), 0, sizeof(DevWmemContext));
    return status;
}

static memx_status _download_dfp_memory_parallel(MemxPcie *memx_dev, uint8_t type, uint8_t base_chip_id, uint8_t num_of_mpu){
    memx_status         status              = MEMX_STATUS_OK;
    platform_int32_t    result              = 0;
    const uint8_t       max_parallel_count  = 4; //MAX support 4 parallel downloading
    uint8_t*            ubtxBuf             = (uint8_t*) memx_dev->tx_buffer;
    uint8_t*            ubrxBuf             = (uint8_t*) memx_dev->rx_buffer;
    struct transport_cmd       cmd;
    uint8_t             chip_id;
    uint8_t             non_finshed_chip_count;
    uint32_t            chip_bitmap;
    uint8_t             start_index;
    uint16_t            uwidx;
    uint8_t*            ubBuf;
    uint32_t            length;
    uint32_t            max_data_length ;
    uint8_t             des_type;
    uint8_t             src_type;
    uint8_t             valid_count;
    pMemxDeviceMeta     pDevMeta;

    platform_mutex_lock(&memx_dev->write_guard);
    for(uint8_t cur_base_id = base_chip_id; cur_base_id < (base_chip_id + num_of_mpu); cur_base_id += max_parallel_count){
        non_finshed_chip_count = max_parallel_count;
        chip_bitmap         = 0;

        for(uint8_t idx = 0; idx <  max_parallel_count; idx++){//some chip has nothing to do, mark it
            chip_id = cur_base_id + idx;
            if(!memx_dev->download_wmem_context[chip_id].uwValidEntryCnt){
                chip_bitmap |= BIT(chip_id);
                non_finshed_chip_count--;
            }
        }

        while(non_finshed_chip_count){
            //reset SQ and valid chip
            valid_count = 0;
            memset(&cmd, 0, sizeof(struct transport_cmd));
            cmd.SQ.opCode      = MEMX_ADMIN_CMD_DOWNLOAD_DFP;
            cmd.SQ.cmdLen      = sizeof(struct transport_cmd);
            cmd.SQ.subOpCode   = type;
            cmd.SQ.reqLen      = sizeof(struct transport_cq);
            cmd.sq_data[3]     = non_finshed_chip_count;

            for(uint8_t idx = 0; idx <  max_parallel_count; idx++){//Fill SQ part and prepare data
                chip_id = cur_base_id + idx;

                if( !(chip_bitmap & BIT(chip_id)) ){//for each non finished chip
                    uwidx = memx_dev->download_wmem_context[chip_id].uwCurEntryIdx;
                    ubBuf = (uint8_t* ) memx_dev->download_wmem_context[chip_id].pContext[uwidx].src_buf_base;
                    valid_count++;
                    start_index = 4 + idx * 3;

                    //Prepare SQ data
                    des_type = (uint8_t)((memx_dev->download_wmem_context[chip_id].pContext[uwidx].des_addr_base & 0x3F000000) >> 24);

                    if(valid_count == 1){
                        src_type = 0x10;
                    } else if (valid_count == 2){
                        src_type = 0x20;
                    } else if (valid_count == 3){
                        src_type = 0x14;
                    } else if (valid_count == 4){
                        src_type = 0x24;
                    } else {
                        src_type = 0;
                    }

                    max_data_length = (non_finshed_chip_count > 2) ? DEF_KB(256) : DEF_KB(512);
                    if((memx_dev->download_wmem_context[chip_id].pContext[uwidx].total_size - memx_dev->download_wmem_context[chip_id].pContext[uwidx].written_size) > max_data_length){
                        length = max_data_length;
                    } else {
                        length = (memx_dev->download_wmem_context[chip_id].pContext[uwidx].total_size - memx_dev->download_wmem_context[chip_id].pContext[uwidx].written_size);
                    }

                    cmd.sq_data[start_index]     = (((src_type) << 16)|((des_type) << 8)|(chip_id));
                    if(uwidx == 0 && memx_dev->download_wmem_context[chip_id].pContext[uwidx].written_size == 0){
                        cmd.sq_data[start_index] |= BIT(24);//Firtst DMA
                    }

                    if(((uwidx + 1) == memx_dev->download_wmem_context[chip_id].uwValidEntryCnt)  && ((memx_dev->download_wmem_context[chip_id].pContext[uwidx].written_size + length) == memx_dev->download_wmem_context[chip_id].pContext[uwidx].total_size)){
                        cmd.sq_data[start_index] |= BIT(25);//Last DMA
                        cmd.sq_data[start_index] |= BIT(26);//Polling this DMA done
                    }

                    if(length <= DEF_KB(64)){
                        cmd.sq_data[start_index] |= BIT(26);//single trunk (<= 64KB) Polling this DMA done
                    }

                    cmd.sq_data[start_index + 1] = memx_dev->download_wmem_context[chip_id].pContext[uwidx].written_size;
                    cmd.sq_data[start_index + 2] = length;

                    //Prepare DMA data
                    if(src_type == 0x10){
                        platform_memcpy(ubtxBuf, ubBuf + memx_dev->download_wmem_context[chip_id].pContext[uwidx].written_size, length);
                    } else if (src_type == 0x14){
                        platform_memcpy(ubtxBuf + DEF_KB(256), ubBuf + memx_dev->download_wmem_context[chip_id].pContext[uwidx].written_size, length);
                    } else if (src_type == 0x20){
                        platform_memcpy(ubrxBuf, ubBuf + memx_dev->download_wmem_context[chip_id].pContext[uwidx].written_size, length);
                    } else if (src_type == 0x24){
                        platform_memcpy(ubrxBuf + DEF_KB(256), ubBuf + memx_dev->download_wmem_context[chip_id].pContext[uwidx].written_size, length);
                    } else {
                        status = MEMX_STATUS_INTERNAL_ERROR;
                        goto done;
                    }
                }
            }

            //trigger IOCTL part
            pDevMeta = memx_device_group_get_devicemeta(memx_dev->group_id);
            if(pDevMeta){
                memx_dev->fd_feature  = platform_open(pDevMeta->device_path[MEMX_DEVICE_TYPE_FEATURE], 0);
                result = platform_ioctl(memx_dev->ctrl_event, &memx_dev->fd_feature, MEMX_ADMIN_DOWNLOAD_DFP, &cmd, cmd.SQ.cmdLen);
                platform_close(&memx_dev->fd_feature);

                if(result != 0 ){
                    status = MEMX_STATUS_PLATFORM_IOCTL_FAIL;
                    goto done;
                } else {
                    if(cmd.CQ.status){
                        status = MEMX_STATUS_INTERNAL_ERROR;
                        goto done;
                    }
                }
            } else {
                status = MEMX_STATUS_NULL_POINTER;
                goto done;
            }

            //Update information
            for(uint8_t idx = 0; idx <  max_parallel_count; idx++){
                chip_id = cur_base_id + idx;
                uwidx = memx_dev->download_wmem_context[chip_id].uwCurEntryIdx;
                start_index = 4 + idx * 3;
                if( !(chip_bitmap & BIT(chip_id)) ){//for each non finished chip
                    if(memx_dev->download_wmem_context[chip_id].uwCurEntryIdx < memx_dev->download_wmem_context[chip_id].uwValidEntryCnt){
                        memx_dev->download_wmem_context[chip_id].pContext[uwidx].written_size += cmd.sq_data[start_index + 2];

                        if(memx_dev->download_wmem_context[chip_id].pContext[uwidx].written_size == memx_dev->download_wmem_context[chip_id].pContext[uwidx].total_size){
                            memx_dev->download_wmem_context[chip_id].uwCurEntryIdx++;
                            if(memx_dev->download_wmem_context[chip_id].uwCurEntryIdx == memx_dev->download_wmem_context[chip_id].uwValidEntryCnt){
                                chip_bitmap |= (0x01 << chip_id);
                                non_finshed_chip_count--;
                            }
                        }
                    }
                }
            }
        }
    }

done:
    memset((void *) ubtxBuf, 0, MEMX_MPUIO_IFMAP_HEADER_SIZE);
    platform_mutex_unlock(&memx_dev->write_guard);
    for(uint8_t idx = 0; idx <  num_of_mpu; idx++){
        chip_id = base_chip_id + idx;
        free(memx_dev->download_wmem_context[chip_id].pContext);
        memset((void *) &(memx_dev->download_wmem_context[chip_id]), 0, sizeof(DevWmemContext));
    }

    return status;
}


static memx_status memx_parsing_weight_memory(MemxPcie *memx_dev, uint8_t * const curr_dfp_buffer_pos, uint8_t chip_id)
{
    memx_status status                  = MEMX_STATUS_OK;
    uint32_t    wtmem_block_count       = 0;
    uint8_t     *curr_wtmem_buffer_pos  = curr_dfp_buffer_pos;
    uint32_t    total_wtmem_block_count = 0;

    total_wtmem_block_count = *((uint32_t *)(curr_wtmem_buffer_pos));
    if (total_wtmem_block_count == 0) {
        MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "invalid total_wtmem_block_count(%u)\n", total_wtmem_block_count);
    } else {
        if(chip_id < MEMX_MPUIO_MAX_HW_MPU_COUNT){
            memx_dev->download_wmem_context[chip_id].uwMaxEntryCnt = total_wtmem_block_count;
            memx_dev->download_wmem_context[chip_id].uwCurEntryIdx = 0;
            memx_dev->download_wmem_context[chip_id].uwValidEntryCnt = 0;
            memx_dev->download_wmem_context[chip_id].pContext = (pWmemEntry) malloc(sizeof(WmemEntry) * memx_dev->download_wmem_context[chip_id].uwMaxEntryCnt);
            if(memx_dev->download_wmem_context[chip_id].pContext == NULL){
                printf("alloc memx_dev->download_wmem_context[chip_id].pContext failed \r\n");
                status = MEMX_STATUS_OUT_OF_MEMORY;
            }else{
                memset(memx_dev->download_wmem_context[chip_id].pContext, 0, sizeof(WmemEntry) * memx_dev->download_wmem_context[chip_id].uwMaxEntryCnt);

                curr_wtmem_buffer_pos += SEP_NEXT_OFS; // skip total_wtmem_block_count

                for (wtmem_block_count = 0; wtmem_block_count < total_wtmem_block_count; wtmem_block_count++) {
                    uint32_t curr_wtmem_block_size = 0;
                    uint32_t curr_wtmem_target_address = 0;

                    curr_wtmem_block_size = *((uint32_t *)(curr_wtmem_buffer_pos));
                    curr_wtmem_buffer_pos += SEP_NEXT_OFS; // skip wtmem block size(4)
                    curr_wtmem_target_address = *((uint32_t *)(curr_wtmem_buffer_pos));
                    curr_wtmem_buffer_pos += SEP_NEXT_OFS; // skip wtmem target address(4)
                    if (!curr_wtmem_block_size) { continue; }
                    curr_wtmem_block_size -= DFP_WEIGHT_MEMORY_COMMON_HEADER_SIZE; // subtract common header size(8)

                    if (curr_wtmem_block_size > 0) {
                            memx_dev->download_wmem_context[chip_id].pContext[memx_dev->download_wmem_context[chip_id].uwValidEntryCnt].chip_id       = chip_id;
                            memx_dev->download_wmem_context[chip_id].pContext[memx_dev->download_wmem_context[chip_id].uwValidEntryCnt].des_addr_base = curr_wtmem_target_address;
                            memx_dev->download_wmem_context[chip_id].pContext[memx_dev->download_wmem_context[chip_id].uwValidEntryCnt].src_buf_base  = curr_wtmem_buffer_pos;
                            memx_dev->download_wmem_context[chip_id].pContext[memx_dev->download_wmem_context[chip_id].uwValidEntryCnt].total_size    = curr_wtmem_block_size;
                            memx_dev->download_wmem_context[chip_id].pContext[memx_dev->download_wmem_context[chip_id].uwValidEntryCnt].written_size  = 0;
                            memx_dev->download_wmem_context[chip_id].uwValidEntryCnt++;

                            curr_wtmem_buffer_pos += curr_wtmem_block_size;

                    }
                    curr_wtmem_buffer_pos += SEP_NEXT_OFS; // skip to CRC at tail of wtmem block
                }
            }
        } else {
            status = MEMX_STATUS_DFP_INVALID_PARAMETER;
        }
    }

    return status;
}

static memx_status memx_download_rgcfg(MemxPcie *memx_dev, uint8_t * const curr_dfp_buffer_pos, uint8_t chip_id, uint32_t curr_rgcfg_size)
{
    uint8_t *curr_rgcfg_buffer_pos = curr_dfp_buffer_pos;
    uint32_t curr_rgcfg_target_address = 0;
    uint32_t curr_rgcfg_value = 0;
    uint32_t curr_rgcfg_block_size = curr_rgcfg_size;

    MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "curr_rgcfg_block_size(%u)\n", curr_rgcfg_block_size);

    if (curr_rgcfg_block_size == 0) {
        MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "invalid curr_rgcfg_block_size(%u)\n", curr_rgcfg_block_size);
        return MEMX_STATUS_MPU_DOWNLOAD_MODEL_FAIL;
    }

    // Allow FM SRAM clock to be off automatically when not in use
    memx_xflow_write(memx_dev, chip_id, 0x38000004, 0x0, 0x0, true);

    for (uint8_t cg_id = 0; cg_id < MEMX_MPU_MAX_CORE_GROUP; cg_id++) {
        // Enable dynamic clock gating in core groups
        memx_xflow_write(memx_dev, chip_id, 0x31800000 | ((cg_id+1) << 24), 0x18, 0x5, true);

        // Allow WT SRAM clock to be off automatically when not in use
        memx_xflow_write(memx_dev, chip_id, 0x31800000 | ((cg_id+1) << 24), 0x28, 0x0, true);
    }

    while (curr_rgcfg_block_size) {
        curr_rgcfg_target_address = *((uint32_t *)(curr_rgcfg_buffer_pos));
        curr_rgcfg_buffer_pos += SEP_NEXT_OFS; // skip rgcfg_addr

        curr_rgcfg_value = *((uint32_t *)(curr_rgcfg_buffer_pos));

        // adjust chip id of c_egr_write_ahb_addr for stream mode egress dcore to write correct next MPU address in muti-group case
        if ((curr_rgcfg_target_address & 0x30208010) == 0x30208010 && (curr_rgcfg_value & 0x60000000) == 0x60000000) {
            curr_rgcfg_value &= ~(0xf << 18);
            curr_rgcfg_value |= ((chip_id + 1) << 18);
        }

        // Turn off clock for unused M/A-cores
        if (curr_rgcfg_target_address == 0x31800008) memx_xflow_write(memx_dev, chip_id, 0x31800000, 0x0, ~curr_rgcfg_value, true);
        if (curr_rgcfg_target_address == 0x3180000c) memx_xflow_write(memx_dev, chip_id, 0x31800004, 0x0, ~curr_rgcfg_value, true);
        if (curr_rgcfg_target_address == 0x32800008) memx_xflow_write(memx_dev, chip_id, 0x32800000, 0x0, ~curr_rgcfg_value, true);
        if (curr_rgcfg_target_address == 0x3280000c) memx_xflow_write(memx_dev, chip_id, 0x32800004, 0x0, ~curr_rgcfg_value, true);
        if (curr_rgcfg_target_address == 0x33800008) memx_xflow_write(memx_dev, chip_id, 0x33800000, 0x0, ~curr_rgcfg_value, true);
        if (curr_rgcfg_target_address == 0x3380000c) memx_xflow_write(memx_dev, chip_id, 0x33800004, 0x0, ~curr_rgcfg_value, true);
        if (curr_rgcfg_target_address == 0x34800008) memx_xflow_write(memx_dev, chip_id, 0x34800000, 0x0, ~curr_rgcfg_value, true);
        if (curr_rgcfg_target_address == 0x3480000c) memx_xflow_write(memx_dev, chip_id, 0x34800004, 0x0, ~curr_rgcfg_value, true);
        if (curr_rgcfg_target_address == 0x35800008) memx_xflow_write(memx_dev, chip_id, 0x35800000, 0x0, ~curr_rgcfg_value, true);
        if (curr_rgcfg_target_address == 0x3580000c) memx_xflow_write(memx_dev, chip_id, 0x35800004, 0x0, ~curr_rgcfg_value, true);
        if (curr_rgcfg_target_address == 0x36800008) memx_xflow_write(memx_dev, chip_id, 0x36800000, 0x0, ~curr_rgcfg_value, true);
        if (curr_rgcfg_target_address == 0x3680000c) memx_xflow_write(memx_dev, chip_id, 0x36800004, 0x0, ~curr_rgcfg_value, true);

        if ((curr_rgcfg_target_address == PMU_MPU_PWR_MGMT_ADDR)) {
            g_pmuConfig[chip_id] = curr_rgcfg_value;
        } else {
            memx_xflow_write(memx_dev, chip_id, curr_rgcfg_target_address, 0, curr_rgcfg_value, true);
        }

        curr_rgcfg_buffer_pos += SEP_NEXT_OFS; // skip rgcfg_value

        curr_rgcfg_block_size -= 8;
    }

    MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "success.\n");
    return MEMX_STATUS_OK;
}

static memx_status memx_download_data_flow_program(MemxPcie *memx_dev, uint8_t base_chip_id, uint8_t * const buffer, uint32_t num_of_mpu, uint8_t type, uint8_t flag)
{
    memx_status status = MEMX_STATUS_OK;
    uint32_t curr_rgcfg_size = 0;
    uint8_t chip_id = 0;
    uint8_t *curr_dfp_buffer_pos = (uint8_t *)(buffer);

    MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "hw_version(%x)\n", *((uint32_t *)curr_dfp_buffer_pos));
    curr_dfp_buffer_pos += SEP_NEXT_OFS; // skip hw_version
    MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "datecode(%d)\n", *((uint32_t *)(curr_dfp_buffer_pos)));
    curr_dfp_buffer_pos += SEP_NEXT_OFS; // skip datecode
    curr_dfp_buffer_pos += SEP_NEXT_OFS; // skip reserve
    curr_dfp_buffer_pos += SEP_NEXT_OFS; // skip num of mpu

    if(type == DFP_FROM_SEPERATE_WTMEM){
        init_wtmem_and_fmem_group(memx_dev, base_chip_id, num_of_mpu);
    }

    if(type == DFP_FROM_SEPERATE_CONFIG){
        reset_mpu_group(memx_dev, base_chip_id, num_of_mpu);
    }

    for (uint8_t mpu_count = 0; mpu_count < num_of_mpu; mpu_count++) {
        curr_dfp_buffer_pos += curr_rgcfg_size; // skip to current rgcfg dfp buffer start

        chip_id = *((uint8_t *)curr_dfp_buffer_pos) + base_chip_id;
        curr_dfp_buffer_pos += SEP_NEXT_OFS; // skip chip id
        MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "chip_id(%d)\n", chip_id);


        curr_rgcfg_size = *((uint32_t *)curr_dfp_buffer_pos);
        curr_dfp_buffer_pos += SEP_NEXT_OFS; // skip curr_rgcfg_size
        switch (type) {
            case DFP_FROM_SEPERATE_WTMEM: {
                status = memx_parsing_weight_memory(memx_dev, curr_dfp_buffer_pos, chip_id);
                if (memx_status_error(status)) {
                    MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "chip %d init and parsing weight memory fail status 0x%x\n", chip_id, status);
                    return MEMX_STATUS_MPU_DOWNLOAD_MODEL_FAIL;
                }
            } break;
            case DFP_FROM_SEPERATE_CONFIG: {
                status = memx_download_rgcfg(memx_dev, curr_dfp_buffer_pos, chip_id, curr_rgcfg_size);
                if (memx_status_error(status)) {
                    MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "chip %d rgcfg fail status 0x%x\n", chip_id, status);
                    return MEMX_STATUS_MPU_DOWNLOAD_MODEL_FAIL;
                }
            } break;
            default:
                MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "non-support source type(%u)\n", type);
                return MEMX_STATUS_MPU_DOWNLOAD_MODEL_FAIL;
        }
        curr_dfp_buffer_pos += SEP_NEXT_OFS; // skip the tail 0xFFFFFFFF at the end of one chip's config
    }

    if(type == DFP_FROM_SEPERATE_WTMEM){
        //download weight memory here
        if(MEMX_TYPE_NOT_EXIST(flag, MEMX_MPUIO_DOWNLOAD_MODEL_TYPE_LEGACY)) {
            status = _download_dfp_memory_parallel(memx_dev, DFP_DOWNLOAD_WEIGHT_MEMORY, base_chip_id, num_of_mpu);
        } else {
            for (uint8_t mpu_count = 0; mpu_count < num_of_mpu; mpu_count++){
                chip_id = base_chip_id + mpu_count;
                status = _download_dfp_memory(memx_dev, DFP_DOWNLOAD_WEIGHT_MEMORY, chip_id);
                if (memx_status_error(status)) {
                    MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "chip %d download weight memory fail status 0x%x\n", chip_id, status);
                    return MEMX_STATUS_MPU_DOWNLOAD_MODEL_FAIL;
                }
            }
        }
    }

    if(type == DFP_FROM_SEPERATE_CONFIG){
        config_mpu_group_pmu(memx_dev, base_chip_id, num_of_mpu);
    }

    MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "success.\n");
    return MEMX_STATUS_OK;
}

memx_status memx_pcie_download_model(MemxMpuIo *mpuio, uint8_t chip_id, void * pDfpMeta, uint8_t model_idx, int32_t type, int32_t timeout)
{
    unused(timeout);
    unused(model_idx);
    memx_status status          = MEMX_STATUS_OK;
    platform_device_t* memx_dev = NULL;
    MemxPcie* pcie              = NULL;
    struct memx_bin memx_bin         = {0};

    do
    {
        if (!mpuio || !mpuio->context)
        {
            MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "invaild context\n");
            status = MEMX_STATUS_MPUIO_INVALID_CONTEXT;
            break;
        }
        memx_dev = (platform_device_t*)mpuio->context;

        if (!memx_dev->pdev.pcie)
        {
            MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "Invalid memx pcie device\n");
            status = MEMX_STATUS_MPUIO_INVALID_CONTEXT;
            break;
        }
        pcie = memx_dev->pdev.pcie;

        if (pcie->hw_info.chip.generation != MEMX_MPU_CHIP_GEN_CASCADE_PLUS)
        {
            MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "Chip generation(%u) non-support memx_dev interface.\n", pcie->hw_info.chip.generation);
            status = MEMX_STATUS_MPUIO_INVALID_CHIP_GEN;
            break;
        }

        pDfpContext pDfp = (pDfpContext)pDfpMeta;
        if (type & MEMX_MPUIO_DOWNLOAD_MODEL_TYPE_WEIGHT_MEMORY)
        {
            memx_bin.dfp_src = DFP_FROM_SEPERATE_WTMEM;
            memx_bin.buf = pDfp->pWeightBaseAdr;
            memx_bin.dfp_cnt = *(uint32_t *)(memx_bin.buf + 12);
            memx_bin.total_size = pDfp->weight_size;
        }
        else if (type & MEMX_MPUIO_DOWNLOAD_MODEL_TYPE_MPU_CONFIG)
        {
            memx_bin.dfp_src = DFP_FROM_SEPERATE_CONFIG;
            memx_bin.buf = pDfp->pRgCfgBaseAdr;
            memx_bin.dfp_cnt = *(uint32_t *)(memx_bin.buf + 12);
            memx_bin.total_size = pDfp->config_size;
        }
        else
        {
            status =  MEMX_STATUS_MPUIO_INVALID_CONTEXT;
            break;
        }

        if (memx_bin.dfp_cnt != (uint32_t)(pcie->hw_info.chip.curr_config_chip_count / pcie->hw_info.chip.group_count)) {
            status = MEMX_STATUS_MPUIO_INVALID_CHIP_GEN;
            break;
        }

        if (memx_status_no_error(status))
        {
            status = memx_download_data_flow_program(pcie, chip_id, memx_bin.buf, memx_bin.dfp_cnt, memx_bin.dfp_src, type);
        }
    } while (0);

    return status;
}
#else //rewrite
static memx_status memx_download_weight_memory(MemxPcie *memx_dev, uint8_t base_chip_id, platform_void_t *data, platform_uint32_t size, uint8_t flag)
{
    unused(size);
    memx_status status      = MEMX_STATUS_OK;
    struct memx_bin *pMemxbin    =  (struct memx_bin *) data;
    uint32_t    num_of_mpu  = pMemxbin->dfp_cnt;
    MemxPcie    *pcie       = memx_dev;
    platform_int32_t ret    = 0;
    struct memx_driver_info SQ   = {0};

    init_wtmem_and_fmem_group(memx_dev, base_chip_id, (uint8_t) num_of_mpu);


    SQ.CDW[10] = pMemxbin->dfp_src;
    SQ.CDW[11] = base_chip_id;
    SQ.CDW[12] = flag & MEMX_MPUIO_DOWNLOAD_MODEL_TYPE_LEGACY;
    ret = platform_ioctl(pcie->ctrl_event, &pcie->fd, MEMX_SET_DRIVER_INFO, &SQ, sizeof(struct memx_driver_info));
    if (ret != 0)
    {
        MEMX_LOG(MEMX_LOG_DOWNLOAD_DFP, "memx_download_weight_memory failed to ioctl MEMX_SET_DRIVER_INFO\r\n");
        status = MEMX_STATUS_PLATFORM_IOCTL_FAIL;
    } else {
        ret = platform_ioctl(pcie->ctrl_event, &pcie->fd, MEMX_RUNTIMEDWN_DFP, pMemxbin->buf, pMemxbin->total_size);
        if (ret != 0)
        {
            MEMX_LOG(MEMX_LOG_DOWNLOAD_DFP, "memx_download_weight_memory failed to ioctl MEMX_RUNTIMEDWN_DFP \r\n");
            status = MEMX_STATUS_PLATFORM_IOCTL_FAIL;
        }
    }

    return status;
}

static memx_status memx_download_rgcfg(MemxPcie *memx_dev, uint8_t base_chip_id, platform_void_t *data, platform_uint32_t size)
{
    unused(size);
    memx_status status      = MEMX_STATUS_OK;
    struct memx_bin *pMemxbin    =  (struct memx_bin *) data;
    uint32_t    num_of_mpu  = pMemxbin->dfp_cnt;
    MemxPcie    *pcie       = memx_dev;
    platform_int32_t ret    = 0;
    struct memx_driver_info SQ   = {0};

    reset_mpu_group(memx_dev, base_chip_id, (uint8_t) num_of_mpu);

    SQ.CDW[10] = pMemxbin->dfp_src;
    SQ.CDW[11] = base_chip_id;
    SQ.CDW[12] = 0;
    ret = platform_ioctl(pcie->ctrl_event, &pcie->fd, MEMX_SET_DRIVER_INFO, &SQ, sizeof(struct memx_driver_info));
    if (ret != 0)
    {
        MEMX_LOG(MEMX_LOG_DOWNLOAD_DFP, "memx_download_rgcfg failed to ioctl MEMX_SET_DRIVER_INFO\r\n");
        status = MEMX_STATUS_PLATFORM_IOCTL_FAIL;

    } else {
        ret = platform_ioctl(pcie->ctrl_event, &pcie->fd, MEMX_RUNTIMEDWN_DFP, pMemxbin->buf, pMemxbin->total_size);
        if (ret != 0)
        {
            MEMX_LOG(MEMX_LOG_DOWNLOAD_DFP, "memx_download_rgcfg failed to ioctl MEMX_RUNTIMEDWN_DFP\r\n");
            status = MEMX_STATUS_PLATFORM_IOCTL_FAIL;
        }
    }

    return status;
}

static memx_status memx_download_data_flow_program(MemxPcie *memx_dev, uint8_t base_chip_id, platform_void_t *data, platform_uint32_t size, uint8_t flag)
{
    unused(size);
    memx_status status      = MEMX_STATUS_OK;
    struct memx_bin *pMemxbin    =  (struct memx_bin *) data;
    uint8_t     type        = pMemxbin->dfp_src;
    uint32_t    *plBuf      = (uint32_t *)pMemxbin->buf;

    MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW,  "buf %p size %d type %d dfp_size %d\n", data, size, pMemxbin->dfp_src, pMemxbin->total_size);
    MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW,  "hw_version(%x) datecode(%d) reserve(%d) num of mpu(%d)\n", plBuf[0], plBuf[1], plBuf[2], plBuf[3]);

    switch (type)
    {
        case DFP_FROM_SEPERATE_WTMEM:
            status = memx_download_weight_memory(memx_dev, base_chip_id, data, size, flag);
            break;
        case DFP_FROM_SEPERATE_CONFIG:
            status = memx_download_rgcfg(memx_dev, base_chip_id, data, size);
            break;
        default:
            status = MEMX_STATUS_MPUIO_INVALID_CONTEXT;
            break;
    }

    MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "memx_download_data_flow_program %s.\n", (status == MEMX_STATUS_OK) ? "success" : "failed");
    return status;
}

memx_status memx_pcie_download_model(MemxMpuIo *mpuio, uint8_t chip_id,void * pDfpMeta, uint8_t model_idx, int32_t type, int32_t timeout)
{
    unused(timeout);
    memx_status status          = MEMX_STATUS_OK;
    platform_device_t* memx_dev = NULL;
    MemxPcie* pcie              = NULL;
    uint32_t dfp_fsize          = 0;
    struct memx_bin memx_bin         = {0};

    do
    {
        if (!mpuio || !mpuio->context)
        {
            MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "invaild context\n");
            status = MEMX_STATUS_MPUIO_INVALID_CONTEXT;
            break;
        }
        memx_dev = (platform_device_t*)mpuio->context;

        if (!memx_dev->pdev.pcie)
        {
            MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "Invalid memx pcie device\n");
            status = MEMX_STATUS_MPUIO_INVALID_CONTEXT;
            break;
        }
        pcie = memx_dev->pdev.pcie;

        if (pcie->hw_info.chip.generation != MEMX_MPU_CHIP_GEN_CASCADE_PLUS)
        {
            MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "Chip generation(%u) non-support memx_dev interface.\n", pcie->hw_info.chip.generation);
            status = MEMX_STATUS_MPUIO_INVALID_CHIP_GEN;
            break;
        }

        unused(model_idx);
        pDfpContext pDfp = (pDfpContext)pDfpMeta;
        if (type & MEMX_MPUIO_DOWNLOAD_MODEL_TYPE_WEIGHT_MEMORY)
        {
            memx_bin.dfp_src = DFP_FROM_SEPERATE_WTMEM;
            memx_bin.buf = pDfp->pWeightBaseAdr;
            dfp_fsize = pDfp->weight_size;
        }
        else if (type & MEMX_MPUIO_DOWNLOAD_MODEL_TYPE_MPU_CONFIG)
        {
            memx_bin.dfp_src = DFP_FROM_SEPERATE_CONFIG;
            memx_bin.buf = pDfp->pRgCfgBaseAdr;
            dfp_fsize = pDfp->config_size;
        }
        else
        {
            status =  MEMX_STATUS_MPUIO_INVALID_CONTEXT;
        }

        if (memx_status_no_error(status))
        {
            MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "Chip generation(%u) \n", pcie->hw_info.chip.generation);
            memx_bin.total_size = dfp_fsize;
            memx_bin.dfp_cnt = *(uint32_t *)(memx_bin.buf + 12);

            if (memx_bin.dfp_cnt != (uint32_t)(pcie->hw_info.chip.curr_config_chip_count / pcie->hw_info.chip.group_count)) {
                status = MEMX_STATUS_MPU_DOWNLOAD_MODEL_FAIL;
            }

            if (memx_status_no_error(status)) {
                status = memx_download_data_flow_program(pcie, chip_id, &memx_bin, sizeof(memx_bin), (uint8_t) type);
            }
        }
    } while (0);

    return status;
}
#endif

memx_status memx_pcie_set_ifmap_size(MemxMpuIo *mpuio, uint8_t chip_id, uint8_t flow_id, int32_t height, int32_t width, int32_t z, int32_t channel_number, int32_t format, int32_t timeout)
{
    unused(timeout);
    if (!mpuio || !mpuio->context) {
        MEMX_LOG(MEMX_LOG_MPUIO_OFMAP_FLOW, "invaild context\n");
        return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
    }
    platform_device_t *memx_dev = (platform_device_t *)mpuio->context;
    if (!memx_dev->pdev.pcie) { MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "Invalid memx pcie device\n"); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    MemxPcie *pcie = memx_dev->pdev.pcie;
    if (pcie->hw_info.chip.generation != MEMX_MPU_CHIP_GEN_CASCADE_PLUS) {
        MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "Chip generation(%u) non-support memx_dev interface.\n", pcie->hw_info.chip.generation);
        return MEMX_STATUS_MPUIO_INVALID_CHIP_GEN;
    }
    MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "height(%u) width(%u) z(%u) channel_number(%u) format(%u).\n", height, width, z, channel_number, format);
    pcie->in_size[chip_id][flow_id] = height * width * z * channel_number;
    MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "flow_id(%u) in_size(%u))\n", flow_id, pcie->in_size[chip_id][flow_id]);
    return MEMX_STATUS_OK;
}

memx_status memx_pcie_set_ofmap_size(MemxMpuIo *mpuio, uint8_t chip_id, uint8_t flow_id, int32_t height, int32_t width, int32_t z, int32_t channel_number, int32_t format, int32_t timeout)
{
    unused(timeout);
    if (!mpuio || !mpuio->context) {
        MEMX_LOG(MEMX_LOG_MPUIO_OFMAP_FLOW, "invaild context\n");
        return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
    }
    platform_device_t *memx_dev = (platform_device_t *)mpuio->context;
    if (!memx_dev->pdev.pcie) { MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "Invalid memx pcie device\n"); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    MemxPcie *pcie = memx_dev->pdev.pcie;
    if (pcie->hw_info.chip.generation != MEMX_MPU_CHIP_GEN_CASCADE_PLUS) {
        MEMX_LOG(MEMX_LOG_MPUIO_OFMAP_FLOW, "Chip generation(%u) non-support memx_dev interface.\n", pcie->hw_info.chip.generation);
        return MEMX_STATUS_MPUIO_INVALID_CHIP_GEN;
    }

    MEMX_LOG(MEMX_LOG_MPUIO_OFMAP_FLOW, "height(%u) width(%u) z(%u) channel_number(%u) format(%u) frame_pad(%u).\n", height, width, z, channel_number, format, 1);
    pcie->out_size[chip_id][flow_id] = mpuio_comm_cal_output_flow_size(height, width, z, channel_number, format, ((format == MEMX_MPUIO_OFMAP_FORMAT_GBF80_ROW_PAD)? 0 : 1));
    MEMX_LOG(MEMX_LOG_MPUIO_OFMAP_FLOW, "chip_id(%u) flow_id(%u) out_flow_size(%u).\n", chip_id, flow_id, pcie->out_size[chip_id][flow_id]);
    pcie->out_width_size[chip_id][flow_id] = width;
    pcie->out_height_size[chip_id][flow_id] = height;
    return MEMX_STATUS_OK;
}

memx_status memx_pcie_update_fmap_size(MemxMpuIo *mpuio, uint8_t chip_id, uint8_t in_flow_count, uint8_t out_flow_count, int32_t timeout)
{
    unused(timeout);

    if (!mpuio || !mpuio->context) {
        MEMX_LOG(MEMX_LOG_MPUIO_OFMAP_FLOW, "invaild context\n");
        return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
    }
    platform_device_t *memx_dev = (platform_device_t *)mpuio->context;
    if (!memx_dev->pdev.pcie) { MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "Invalid memx pcie device\n"); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    MemxPcie *pcie = memx_dev->pdev.pcie;
    if (pcie->hw_info.chip.generation != MEMX_MPU_CHIP_GEN_CASCADE_PLUS) {
        MEMX_LOG(MEMX_LOG_MPUIO_OFMAP_FLOW, "Invaild Chip Generation(%u) so can't calcullate output buffer size\n", pcie->hw_info.chip.generation);
        return MEMX_STATUS_MPUIO_INVALID_CHIP_GEN;
    }

    if ((in_flow_count == 0) && (out_flow_count == 0)) {
        MEMX_LOG(MEMX_LOG_MPUIO_OFMAP_FLOW, "no any active flow ports.\n");
        return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
    }

    uint32_t total_buffer_size = 0;
    uint8_t total_group_cnt = pcie->hw_info.chip.group_count;
    uint32_t ofmap_total_buffer_size = (pcie->hw_info.chip.roles[chip_id] == ROLE_SINGLE)? (MEMX_PCIE_OFMAP_DMA_COHERENT_MMAP_SIZE_256KB/2): (MEMX_PCIE_OFMAP_DMA_COHERENT_MMAP_SIZE_256KB/total_group_cnt)& ~0x3;
    uint32_t remaining_try_count = (ofmap_total_buffer_size/1024) - 1;
    uint32_t buffer_size[MEMX_TOTAL_FLOW_COUNT] = {0};

    do {
        total_buffer_size = 0;
        for (uint8_t flow_id = 0; flow_id < out_flow_count; flow_id++) {
            buffer_size[flow_id] = mpuio_comm_assign_suitable_buffer_size_for_flow(pcie->out_height_size[chip_id][flow_id], pcie->out_size[chip_id][flow_id], remaining_try_count);
            total_buffer_size += (buffer_size[flow_id] + MEMX_MPUIO_OFMAP_HEADER_SIZE);
        }

        remaining_try_count--;
        // worst case set buf size to 4 for all enabled flow
        if (remaining_try_count == 0) {
            for (uint8_t flow_id = 0; flow_id < out_flow_count; flow_id++) {
                printf("Warning: all output flow set to 4 bytes\n");
                buffer_size[flow_id] = 4;
            }
            break;
        }
    } while (total_buffer_size > ofmap_total_buffer_size);

    for (uint8_t flow_id = 0; flow_id < out_flow_count; flow_id++) {
        if (pcie->out_size[chip_id][flow_id] % buffer_size[flow_id] || buffer_size[flow_id] % 4) {
            return MEMX_STATUS_MPU_INVALID_BUF_SIZE;
        }
    }

    const uint32_t buf0_addr_reg = 0x30201080;
    const uint32_t buf0_size_reg = 0x30201084;
    const uint32_t buf1_addr_reg = 0x30201088;
    const uint32_t buf1_size_reg = 0x3020108C;
    uint32_t data_buffer_start_addr = pcie->hw_info.fw.egress_dcore_mapping_sram_base[chip_id];
    pcie->final_start_flows_bits[chip_id] = 0;

    for (uint8_t flow_id = 0; flow_id < out_flow_count; flow_id++) {
        uint32_t channel_offset = (0x100 * flow_id);
        uint32_t curr_flow_buffer0_sram_start_address = data_buffer_start_addr + MEMX_OFMAP_SRAM_COMMON_HEADER_SIZE - MEMX_PCIE_EGRESS_DCORE_COMMON_HEADER_SIZE;
        uint32_t curr_flow_buffer0_sram_size = buffer_size[flow_id] + MEMX_PCIE_EGRESS_DCORE_COMMON_HEADER_SIZE;

        memx_xflow_write(pcie, chip_id, buf0_addr_reg, channel_offset, curr_flow_buffer0_sram_start_address, true);
        memx_xflow_write(pcie, chip_id, buf0_size_reg, channel_offset, curr_flow_buffer0_sram_size, true);

        memx_xflow_write(pcie, chip_id, buf1_addr_reg, channel_offset, curr_flow_buffer0_sram_start_address + ofmap_total_buffer_size, true);
        memx_xflow_write(pcie, chip_id, buf1_size_reg, channel_offset, curr_flow_buffer0_sram_size, true);

        // move to next flow buffer start in sram
        data_buffer_start_addr = data_buffer_start_addr + buffer_size[flow_id] + MEMX_OFMAP_SRAM_COMMON_HEADER_SIZE;
        pcie->final_start_flows_bits[chip_id] |= (0x1 << flow_id);
    }

    // enable mpu buf0 and buf1 irq for all flows
    memx_xflow_write(pcie, chip_id, 0x30200000, 0x10, pcie->final_start_flows_bits[chip_id], true);
    memx_xflow_write(pcie, chip_id, 0x30200000, 0x14, pcie->final_start_flows_bits[chip_id], true);

    // enable output ping-pong
    uint32_t egr_conifg = memx_xflow_read(pcie, chip_id, 0x30200000, 0x4, true);
    memx_xflow_write(pcie, chip_id, 0x30200000, 0x4, egr_conifg | (0x1 << 6), true);

    memx_xflow_write(pcie, chip_id, 0x30200000, 0x100, pcie->final_start_flows_bits[chip_id], true);
    return MEMX_STATUS_OK;
}
