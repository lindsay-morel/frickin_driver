/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#include "memx_usb.h"
#include "memx_mpu_comm.h"
#include "memx_log.h"
#include "memx_ioctl.h"
#include "memx_list.h"
#include "memx_device_manager.h"
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

#if __linux__
#include <time.h>
unsigned int usb_write_size = 0;
unsigned int usb_read_size = 0;
unsigned int usb_write_time = 0;
unsigned int usb_write_time_idle = 0;
unsigned int usb_read_time = 0;
unsigned int usb_read_time_idle = 0;
static clockid_t clock_id = CLOCK_REALTIME;
struct timespec usb_prev_time_write;
struct timespec usb_prev_time_read;
#endif

static void* force_buffer_allocate(uint32_t size);
static MemxRingBufferData* output_data_buffer_allocate(uint32_t real_buffer_length);
static void memx_usb_dynamic_allocate_buffer(struct memx_mpu_size* mpu_size, MemxUsb *usb, uint8_t out_flow_count, uint8_t chip_id);
static uint32_t memx_usb_choose_buffer_type(uint32_t* buffer_size_single, uint32_t* buffer_size_pingpong, uint32_t* total_buffer_size, uint32_t* transfer_count_max, uint32_t* flow_size, uint8_t out_flow_count);

/***************************************************************************//**
* implementation
******************************************************************************/
static uint8_t _check_usb_abort_status(MemxUsb* usb);
/**
* @brief The main process of background worker 'crawler'. Crawler is designed
* to keep pulling out data from device and write data into the 'shared' ring
* buffer waiting to be decoded.
*/
void* _memx_usb_background_crawler_main_loop(void *user_data)
{
    if (!user_data) { return NULL; }
    platform_device_t *memx_dev = (platform_device_t *)user_data;
    if (!memx_dev->pdev.usb) { return NULL; }
    MemxUsb *usb = memx_dev->pdev.usb;
    uint8_t* remain_output_buffer = (uint8_t*)force_buffer_allocate(MEMX_USB_OUTPUT_BUFFER_BACKUP_SIZE);
    uint32_t remain_data_size = 0;
    uint8_t read_abort = 0;

    while (1) {
        platform_int32_t total_len = 0;
        if (_check_usb_abort_status(usb)) {
            break;
        } else {
            total_len = platform_read(usb->rx_event, (usb->role == MEMX_MPU_TYPE_SPLIT) ? &usb->fdo : &usb->fd, usb->rx_buffer, MEMX_USB_READ_BUFFER_SIZE);
        }
        if (total_len < 0) { continue;}
        if (total_len > 0) {
            uint8_t *curr_buffer_pos = usb->rx_buffer;
            uint32_t output_chip_id = 0;

            if (remain_data_size) {
                memcpy(remain_output_buffer + remain_data_size, curr_buffer_pos, total_len);
                curr_buffer_pos = remain_output_buffer;
                total_len += remain_data_size;
            }

            // read output_chip_id from first trigger flow EGR pkt hdr for cascasde plus
            if  (usb->hw_info.chip.generation == MEMX_MPU_CHIP_GEN_CASCADE_PLUS) {
                output_chip_id = *(uint32_t *)(curr_buffer_pos + 16);
                MEMX_LOG(MEMX_LOG_MPUIO_OFMAP_FLOW, "output_chip_id: %d\n", output_chip_id);
            }

            while (total_len > 0) {
                if (total_len < (platform_int32_t)MEMX_MPUIO_OFMAP_HEADER_SIZE) {
                    memcpy(remain_output_buffer, curr_buffer_pos, total_len);
                    remain_data_size = total_len;
                    break;
                }

                uint32_t flow_id = *(uint32_t *)curr_buffer_pos;
                uint8_t *real_data_buffer_pos = curr_buffer_pos + MEMX_MPUIO_OFMAP_HEADER_SIZE;
                uint32_t real_buffer_length = *(uint32_t *)(curr_buffer_pos + 4);
                MemxRingBufferData *data_node = null;

                // read flow id and buffer length from EGR buf hdr for cascasde plus
                if  (usb->hw_info.chip.generation == MEMX_MPU_CHIP_GEN_CASCADE_PLUS) {
                    flow_id = *(uint32_t *)(curr_buffer_pos + MEMX_MPUIO_OFMAP_HEADER_SIZE - 8) & 0x0000001f;
                    real_buffer_length = *(uint32_t *)(curr_buffer_pos + MEMX_MPUIO_OFMAP_HEADER_SIZE - 4);
                }

                if (total_len < (platform_int32_t)(real_buffer_length + MEMX_MPUIO_OFMAP_HEADER_SIZE)) {
                    MEMX_LOG(MEMX_LOG_MPUIO_OFMAP_FLOW, "Seperate flow[%d] nead %d data form chip, remain %d byte\n", flow_id, real_buffer_length, total_len);
                    memcpy(remain_output_buffer, curr_buffer_pos, total_len);
                    remain_data_size = total_len;
                    break;
                }

                MEMX_LOG(MEMX_LOG_MPUIO_OFMAP_FLOW, "flow[%d] read %d data form chip. Total %d \n", flow_id, real_buffer_length, total_len);

                while (1) {
                    data_node = output_data_buffer_allocate(real_buffer_length);
                    if (!data_node) {
                        printf("Memory allocate for MemxRingBufferData fail\n");
                        continue;
                    }

                    data_node->length = real_buffer_length;
                    memcpy(data_node->buffer, real_data_buffer_pos, real_buffer_length);

                    // wait untill list empty
                    while (memx_list_count(usb->flow_ringbuffer[output_chip_id][flow_id]) >= MEMX_MPUIO_MAX_PENDDING_LIST_SIZE) {
                        if (read_abort || usb->is_read_abort) {
                            free(data_node->buffer);
                            data_node->buffer = NULL;
                            free(data_node);
                            data_node = NULL;
                            read_abort = 1;

                            break;
                        }
                        platform_usleep(100);
                    }

                    if (read_abort == 1) break;
                    // push until succes
                    while (memx_list_push(usb->flow_ringbuffer[output_chip_id][flow_id], data_node));
                    break;
                }
                total_len -= ((real_buffer_length) + MEMX_MPUIO_OFMAP_HEADER_SIZE);
                curr_buffer_pos = (curr_buffer_pos + (real_buffer_length) + MEMX_MPUIO_OFMAP_HEADER_SIZE);
            }

            if (total_len == 0) {
                remain_data_size = 0;
            }
        }
    }

    free(remain_output_buffer);
    remain_output_buffer = NULL;

    return NULL;
}

static void* force_buffer_allocate(uint32_t size)
{
    void* buffer = null;

    while (!buffer) {
        buffer = malloc(size);

        if (!buffer) {
            printf("Memory allocate for force_buffer_allocate fail, %d byte\n", size);
            platform_usleep(100);
        }
    }

    return buffer;
}

static MemxRingBufferData* output_data_buffer_allocate(uint32_t real_buffer_length)
{
    MemxRingBufferData *data_node = (MemxRingBufferData *)malloc(sizeof(MemxRingBufferData));

    if (!data_node) {
        printf("Memory allocate for MemxRingBufferData fail\n");
    }

    data_node->buffer = malloc(real_buffer_length);
    if (!data_node->buffer) {
        printf("Memory allocate for data_node->buffer fail\n");
        free(data_node);
        data_node = NULL;
    }

    return data_node;
}

static uint8_t _check_usb_abort_status(MemxUsb* usb)
{
    platform_mutex_lock(&usb->context_guard);
    uint8_t status = usb->is_ready_exit;
    platform_mutex_unlock(&usb->context_guard);
    return  status;
}

static void _set_usb_abort_status(MemxUsb* usb)
{
    // stop memx_dev rx background workers so set exit flag first
    platform_mutex_lock(&usb->context_guard);
    usb->is_ready_exit = 1;

    if (usb->role == MEMX_MPU_TYPE_SPLIT) {
        if (platform_ioctl(usb->ctrl_event, &usb->fdo, MEMX_ABORT_TRANSFER, NULL, 0)) { printf("failed to ioctl MEMX_ABORT_TRANSFER!\n"); }
        if (platform_ioctl(usb->ctrl_event, &usb->fdi, MEMX_ABORT_TRANSFER, NULL, 0)) { printf("failed to ioctl MEMX_ABORT_TRANSFER!\n"); }
    }
    else {
        if (platform_ioctl(usb->ctrl_event, &usb->fd, MEMX_ABORT_TRANSFER, NULL, 0)) { printf("failed to ioctl MEMX_ABORT_TRANSFER!\n"); }
    }
    platform_mutex_unlock(&usb->context_guard);
}

static void _set_usb_read_abort_status(MemxUsb *usb)
{
    usb->is_read_abort = 1;
}

static memx_status memx_usb_get_chip_info(MemxUsb *usb)
{
    if (usb->hw_info.chip.generation != MEMX_MPU_CHIP_GEN_CASCADE_PLUS) {
        return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
    }

    if (platform_ioctl(usb->ctrl_event, &usb->fd, MEMX_GET_HW_INFO, &usb->hw_info, sizeof(struct hw_info))) {
        printf("failed to ioctl MEMX_GET_HW_INFO!\n");
        return MEMX_STATUS_PLATFORM_IOCTL_FAIL;
    }

    return MEMX_STATUS_OK;
}

static platform_int32_t _check_usb_open_result(MemxUsb *pDev, platform_uint8_t group_id, uint8_t chip_gen)
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
        } else if (pDev->fd < 0){
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

static platform_int32_t _memx_usb_open(MemxUsb *pDev, platform_uint8_t group_id, uint8_t chip_gen)
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
    } else {
        return -5;
    }

    return _check_usb_open_result(pDev, group_id, chip_gen);
}

memx_status memx_usb_create_context(platform_device_t **memx_dev_ptr_src, uint8_t group_id, uint8_t chip_gen)
{
    // create usb data
    *memx_dev_ptr_src = (platform_device_t *)malloc(sizeof(platform_device_t));
    platform_device_t *memx_dev = *memx_dev_ptr_src;
    if (!memx_dev) { printf("malloc for memx_dev failed\n"); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    memx_dev->hif = MEMX_MPUIO_INTERFACE_USB;
    memx_dev->pdev.usb = (MemxUsb *)malloc(sizeof(MemxUsb));
    if (!memx_dev->pdev.usb) { printf("malloc for memx_usb device failed\n"); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    MemxUsb *usb = memx_dev->pdev.usb;
    memset(usb, 0, sizeof(MemxUsb)); // init. all pointers to nullptr

    usb->pipeline_flag = true;
    usb->hw_info.chip.generation = chip_gen;
    platform_int32_t result = _memx_usb_open(usb, group_id, chip_gen);
    if (result != 0) {
        printf("memx_usb_open failed %d\n", result);
        return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
    }
    if (memx_usb_get_chip_info(usb)) {
        printf("Get USB HW info failed\n");
        return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
    }
    for (uint8_t mpu_group_id = 0; mpu_group_id < usb->hw_info.chip.group_count; mpu_group_id++) {
        uint8_t chip_id = usb->hw_info.chip.groups[mpu_group_id].output_chip_id;
        // allocate internal flow-ringbuffer
        for (uint8_t flow_id = 0; flow_id < MEMX_MPUIO_FLOW_RING_BUFFER_NUMBER; ++flow_id) {
            // To increase the windows performance, we need to at least 4 * MEMX_USB_FLOW_RINGBUFFER_SIZE
            // it's too wast memory so change to use link-list for buffering
            usb->flow_ringbuffer[chip_id][flow_id] = memx_list_create();

            if (!usb->flow_ringbuffer[chip_id][flow_id]) { printf("usb->flow_ringbuffer[%d][%d] create failed\n", chip_id, flow_id); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
        }
    }
    usb->rx_buffer = (uint8_t *)malloc(MEMX_USB_READ_BUFFER_SIZE);
    if (!usb->rx_buffer) { printf("usb->rx_buffer create failed\n"); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    memset(usb->rx_buffer, 0x0, MEMX_USB_READ_BUFFER_SIZE);

    usb->tx_buffer = (uint8_t *)malloc(MEMX_USB_COMMON_BUFFER_SIZE);
    if (!usb->tx_buffer) { printf("usb->tx_buffer create failed\n"); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    memset(usb->tx_buffer, 0x0, MEMX_USB_COMMON_BUFFER_SIZE);

    if (platform_mutex_create(&usb->context_guard, NULL)) {
        printf("usb context_guard create failed\n");
        return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
    }

    // start background workers
    if (platform_thread_create(&usb->background_crawler, NULL, &_memx_usb_background_crawler_main_loop, memx_dev)) {
        printf("usb->background_crawler create failed\n");
        return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
    }

#if _WIN32
    SetThreadPriority(&usb->background_crawler, THREAD_PRIORITY_TIME_CRITICAL); // THREAD_PRIORITY_TIME_CRITICAL
#endif
    return MEMX_STATUS_OK;
}

#if __linux__
void memx_usb_throughput_add(unsigned int *current_size, unsigned int additional_size)
{
    if (*current_size > (0xffffffff - additional_size)) {
        usb_write_size = 0;
        usb_read_size = 0;
        usb_write_time = 0;
        usb_write_time_idle = 0;
        usb_read_time = 0;
        usb_read_time_idle = 0;
    } else {
        *current_size += additional_size;
    }
}

void memx_usb_set_throughput_info(MemxUsb* usb)
{
    struct memx_throughput_info memx_throughput_info;
    memx_throughput_info.stream_write_us = usb_write_time + usb_write_time_idle;
    memx_throughput_info.stream_write_kb = usb_write_size / KBYTE;
    memx_throughput_info.stream_read_us = usb_read_time + usb_read_time_idle;
    memx_throughput_info.stream_read_kb =  usb_read_size / KBYTE;

    // stop memx_dev rx background workers so set exit flag first
    platform_mutex_lock(&usb->context_guard);
    if (platform_ioctl(usb->ctrl_event, (usb->role == MEMX_MPU_TYPE_SPLIT) ? &usb->fdi : &usb->fd, MEMX_SET_THROUGHPUT_INFO, &memx_throughput_info, sizeof(struct memx_throughput_info))) {
        printf("failed to ioctl MEMX_SET_THROUGHPUT_INFO!\n");
    }
    platform_mutex_unlock(&usb->context_guard);
    usb_write_size = 0;
    usb_read_size = 0;
    usb_write_time = 0;
    usb_write_time_idle = 0;
    usb_read_time = 0;
    usb_read_time_idle = 0;
}

void memx_usb_clear_dummy_read(MemxUsb *usb)
{
    platform_int32_t total_len = 0;

    do {
        total_len = platform_ioctl(usb->ctrl_event, (usb->role == MEMX_MPU_TYPE_SPLIT) ? (&usb->fdo) : (&usb->fd), MEMX_DUMMY_READ, NULL, 0);
    } while (total_len);
}
#endif

memx_status memx_usb_set_read_abort_start(MemxMpuIo *mpuio)
{
    if (!mpuio || !mpuio->context) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }

    platform_device_t *memx_dev = (platform_device_t *)mpuio->context;

    if (memx_dev->pdev.usb) {
        _set_usb_read_abort_status(memx_dev->pdev.usb);
        return MEMX_STATUS_OK;
    } else {
        return MEMX_STATUS_MPUIO_INVALID_CONTEXT;
    }
}

void memx_usb_destroy(MemxMpuIo *mpuio)
{
    if (!mpuio || !mpuio->context) { return; }
    platform_device_t *memx_dev = (platform_device_t *)mpuio->context;
    if (!memx_dev->pdev.usb) {
        free(memx_dev);
        memx_dev = NULL;
        return;
    }
    MemxUsb *usb = memx_dev->pdev.usb;

#if __linux__
    memx_usb_set_throughput_info(usb);
#endif

    // free resource for read/write file
    // we need abort transfer first to avoid blocking wait for current urb complete in windows platform.
    _set_usb_abort_status(usb);

    // stop usb rx background workers
    platform_thread_join(&usb->background_crawler, NULL);

#if __linux__
    if (usb->is_read_abort) {
        memx_usb_clear_dummy_read(usb);
    }
#endif

    // release internal ringbuffer
    for (uint8_t mpu_group_id = 0; mpu_group_id < usb->hw_info.chip.group_count; ++mpu_group_id) {
        uint8_t chip_id = usb->hw_info.chip.groups[mpu_group_id].output_chip_id;
        for (uint8_t flow_id = 0; flow_id < MEMX_MPUIO_FLOW_RING_BUFFER_NUMBER; ++flow_id) {
            uint32_t count = 1;
            while (memx_list_count(usb->flow_ringbuffer[chip_id][flow_id])) {
                MemxRingBufferData *data_node = (MemxRingBufferData *)memx_list_pop(usb->flow_ringbuffer[chip_id][flow_id]);
                MEMX_LOG(MEMX_LOG_GENERAL, "flow %d remain data count %d size %d\n", flow_id, count, data_node->length);
                count++;
                free(data_node->buffer);
                data_node->buffer = NULL;
                free(data_node);
                data_node = NULL;
            }
            memx_list_destroy(usb->flow_ringbuffer[chip_id][flow_id]);
        }
    }
    free(usb->rx_buffer); usb->rx_buffer = NULL;
    free(usb->tx_buffer); usb->tx_buffer = NULL;
#if __linux__
    if (usb->role == MEMX_MPU_TYPE_SPLIT) {
        if (usb->fdi > 0) { platform_close(&usb->fdi); }
        if (usb->fdo > 0) { platform_close(&usb->fdo); }
    } else {
        if (usb->fd > 0) { platform_close(&usb->fd);}
    }
#elif _WIN32
    if (usb->tx_event) { platform_close(&usb->tx_event); }
    if (usb->rx_event) { platform_close(&usb->rx_event); }
    if (usb->ctrl_event) { platform_close(&usb->ctrl_event); }
    if (usb->role == MEMX_MPU_TYPE_SPLIT) {
        platform_close(&usb->fdi);
        usb->fdi = NULL;
        platform_close(&usb->fdo);
        usb->fdo = NULL;
    } else {
        platform_close(&usb->fd);
        usb->fd = NULL;
    }
#endif
    platform_mutex_unlock(&usb->context_guard);
    platform_mutex_destory(&usb->context_guard);
    free(usb);
    usb = NULL;
    free(memx_dev);
    memx_dev = NULL;
}

memx_status memx_usb_operation(MemxMpuIo *mpuio, uint8_t chip_id, int32_t cmd_id, void *data, uint32_t size, int32_t timeout)
{
    unused(chip_id);
    unused(size);
    unused(timeout);
    memx_status status = MEMX_STATUS_OK;
    if (!mpuio || !mpuio->context || !data) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    platform_device_t *memx_dev = (platform_device_t *)mpuio->context;
    if (!memx_dev->pdev.usb) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    MemxUsb *usb = memx_dev->pdev.usb;
    int ret = -1;

    switch (cmd_id) {

        case MEMX_CMD_READ_TOTAL_CHIP_COUNT: {
            memcpy(data, &usb->hw_info.chip.total_chip_cnt, 1);
        } break;

        case MEMX_CMD_GET_FW_DOWNLOAD_STATUS: {
            // read fwupdate status
            struct memx_reg memx_rreg = {NULL, 0, 0};
            uint32_t fwupdate_status = 0;

            memx_rreg.buf_addr = (uint8_t *)&fwupdate_status;
            ret = platform_ioctl(usb->ctrl_event, (usb->role == MEMX_MPU_TYPE_SPLIT) ? &usb->fdi : &usb->fd, MEMX_GET_FWUPDATE_STATUS, &memx_rreg, sizeof(struct memx_reg));
            if (ret) {
                printf("failed to ioctl MEMX_GET_FWUPDATE_STATUS!\n");
                return MEMX_STATUS_PLATFORM_IOCTL_FAIL;
            }

            // FW_UPDATE_STATUS_SUCCESS: 0x00ca5ade
            if (fwupdate_status != 0x00ca5ade) {
                return MEMX_STATUS_MPU_DOWNLOAD_FIRMWARE_FAIL;
            }
        } break;

        case MEMX_CMD_CONFIG_MPU_GROUP: {
            if (usb->hw_info.chip.generation != MEMX_MPU_CHIP_GEN_CASCADE_PLUS) {
                return MEMX_STATUS_MPUIO_INVALID_CHIP_GEN;
            }
            uint8_t mpu_group_config = *(uint8_t*)data;

            // 1. update chip role base on differnt mpu group config
            status = mpuio_comm_config_mpu_group(mpu_group_config, &usb->hw_info);
            if (memx_status_error(status)) {
                return status;
            }

            // 2. send updated chip role to firmware
            ret = platform_ioctl(usb->ctrl_event, (usb->role == MEMX_MPU_TYPE_SPLIT) ? &usb->fdi : &usb->fd, MEMX_CONFIG_MPU_GROUP, &usb->hw_info, sizeof(struct hw_info));
            if (ret) {
                printf("failed to ioctl MEMX_CONFIG_MPU_GROUP!\n");
                return MEMX_STATUS_PLATFORM_IOCTL_FAIL;
            }

            // 3. update and allocate internal flow-ringbuffer for each group if new group detected
            for (uint8_t mpu_group_id = 0; mpu_group_id < usb->hw_info.chip.group_count; mpu_group_id++) {
                uint8_t output_chip_id = usb->hw_info.chip.groups[mpu_group_id].output_chip_id;
                // allocate internal flow-ringbuffer
                for (uint8_t flow_id = 0; flow_id < MEMX_MPUIO_FLOW_RING_BUFFER_NUMBER; flow_id++) {
                    if (!usb->flow_ringbuffer[output_chip_id][flow_id]) {
                        usb->flow_ringbuffer[output_chip_id][flow_id] = memx_list_create();
                    }
                    if (!usb->flow_ringbuffer[output_chip_id][flow_id]) { printf("usb->flow_ringbuffer[%d][%d] create failed\n", output_chip_id, flow_id); return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
                }
            }

            // 5. update mpuio hw_info for mpu layer mpu_group info
            memcpy((void*)&mpuio->hw_info, (void *)&usb->hw_info, sizeof(struct hw_info));
        } break;
        case MEMX_CMD_RESET_DEVICE: {
            if (usb->hw_info.chip.generation != MEMX_MPU_CHIP_GEN_CASCADE_PLUS) {
                return MEMX_STATUS_MPUIO_INVALID_CHIP_GEN;
            }
            if (platform_ioctl(usb->ctrl_event, (usb->role == MEMX_MPU_TYPE_SPLIT) ? &usb->fdi : &usb->fd, MEMX_RESET_DEVICE, NULL, 0)) {
                printf("failed to ioctl MEMX_RESET_DEVICE!\n");
                return MEMX_STATUS_PLATFORM_IOCTL_FAIL;
            }
        } break;
        case MEMX_CMD_SET_QSPI_RESET_RELEASE: {
            uint32_t reset_register_value = 0;
            struct memx_reg memx_reg = {NULL, 0, 0};

            memx_reg.buf_addr = (uint8_t *)&reset_register_value;
            memx_reg.reg_start = 0x20000208;
            memx_reg.size = sizeof(uint32_t);

            ret = platform_ioctl(usb->ctrl_event, (usb->role == MEMX_MPU_TYPE_SPLIT) ? &usb->fdi : &usb->fd, MEMX_READ_REG, &memx_reg, sizeof(struct memx_reg));
            if (ret == 0) {
                uint32_t local_buffer[2] = {0}; // combine address and data
                if (*(uint16_t*)data) {
                    reset_register_value |= (MEMX_MPU_RESET_QSPI_M_BIT | MEMX_MPU_RESET_QSPI_S_BIT);
                } else {
                    reset_register_value &= (~(MEMX_MPU_RESET_QSPI_M_BIT | MEMX_MPU_RESET_QSPI_S_BIT));
                }

                local_buffer[0] = 0x20000208;
                local_buffer[1] = reset_register_value;
                memx_reg.buf_addr = (uint8_t *)local_buffer;
                memx_reg.size = sizeof(uint32_t) * 2;

                ret = platform_ioctl(usb->ctrl_event, (usb->role == MEMX_MPU_TYPE_SPLIT) ? &usb->fdi : &usb->fd, MEMX_WRITE_REG, &memx_reg, sizeof(struct memx_reg));
            }

            if (ret != 0) {
                printf("failed to MEMX_CMD_SET_QSPI_RESET_RELEASE!\n");
                status = MEMX_STATUS_PLATFORM_IOCTL_FAIL;
            }
        } break;
        case MEMX_CMD_GET_QSPI_RESET_RELEASE: {
            struct memx_reg memx_reg = {NULL, 0, 0};

            memx_reg.buf_addr = (uint8_t *)data;
            memx_reg.reg_start = 0x20000208;
            memx_reg.size = sizeof(uint32_t);

            ret = platform_ioctl(usb->ctrl_event, (usb->role == MEMX_MPU_TYPE_SPLIT) ? &usb->fdi : &usb->fd, MEMX_READ_REG, &memx_reg, sizeof(struct memx_reg));
            if (ret != 0) {
                printf("failed to MEMX_CMD_GET_QSPI_RESET_RELEASE!\n");
                status = MEMX_STATUS_PLATFORM_IOCTL_FAIL;
            }

            *((uint32_t *)data) = ((*((uint32_t *)data) & (MEMX_MPU_RESET_QSPI_M_BIT | MEMX_MPU_RESET_QSPI_S_BIT)) >> MEMX_MPU_RESET_QSPI_M_BIT_POSITION);
        } break;

        default:
            status = MEMX_STATUS_INVALID_OPCODE;
    }

    return status;
}

memx_status memx_usb_control_write(MemxMpuIo *mpuio, uint8_t chip_id, uint32_t address, uint8_t *data,
                            int32_t length, int32_t *transferred, int32_t increment, int32_t timeout)
{
    unused(chip_id);
    unused(timeout);

    if (!mpuio || !mpuio->context) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    platform_device_t *memx_dev = (platform_device_t *)mpuio->context;
    if (!memx_dev->pdev.usb) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    MemxUsb *usb = memx_dev->pdev.usb;
    if (!data) { return MEMX_STATUS_MPUIO_INVALID_DATA; }
    if (length <= 0) { return MEMX_STATUS_MPUIO_INVALID_DATALEN; }

    uint8_t *local_buffer = (uint8_t *)malloc((uint64_t)2 * MEMX_USB_CONTROLWRITE_SIZE); // combine address and data
    if (!local_buffer) { return MEMX_STATUS_OTHERS; }

    // always reset actual transferred size
    if (transferred) { *transferred = 0; }

    uint32_t temp_address = address;
    uint8_t *temp_buf = data;
    int32_t actual_size = 0;
    int32_t transfer_size = 0;
    // spilt payload into multiple transfers
    for (int32_t transfer_offset = 0; transfer_offset < length;) {
        /*compose register write buffer*/
        transfer_size = (transfer_offset + MEMX_USB_CONTROLWRITE_SIZE < length) ?
                            MEMX_USB_CONTROLWRITE_SIZE : length - transfer_offset; // clip transfer size to chunk size
        for (int32_t j = 0; j < transfer_size; j += 4) {
            *(uint32_t*)(local_buffer + (int64_t)j * 2) = temp_address + (increment ? j : 0);
            *(uint32_t*)(local_buffer + (int64_t)j * 2 + 4) = *(uint32_t *)(temp_buf + j);
        }
        struct memx_reg memx_wreg = {NULL, 0, 0};
        memx_wreg.buf_addr = local_buffer;
        memx_wreg.size = transfer_size * 2;
        int32_t ret = platform_ioctl(usb->ctrl_event, (usb->role == MEMX_MPU_TYPE_SPLIT) ? &usb->fdi : &usb->fd, MEMX_WRITE_REG, &memx_wreg, sizeof(struct memx_reg));
        if (ret) {
            free(local_buffer);
            local_buffer = NULL;
            printf("failed to ioctl MEMX_WRITE_REG!\n");
            return MEMX_STATUS_PLATFORM_IOCTL_FAIL;
        }
        temp_address += (increment ? transfer_size : 0);
        temp_buf += transfer_size;
        actual_size += transfer_size; // here actually is the size submitted to be transferred
        transfer_offset += transfer_size; // update transfer offset
    }

    // report actual transferred size
    if (transferred) { *transferred = actual_size; }

    free(local_buffer);
    local_buffer = NULL;
    return MEMX_STATUS_OK;
}

memx_status memx_usb_control_read(MemxMpuIo *mpuio, uint8_t chip_id, uint32_t address, uint8_t *data,
                            int32_t length, int32_t* transferred, int32_t increment, int32_t timeout)
{
    unused(increment);
    unused(length);
    unused(timeout);
    unused(chip_id);

    if (!mpuio || !mpuio->context) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    platform_device_t *memx_dev = (platform_device_t *)mpuio->context;
    if (!memx_dev->pdev.usb) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    MemxUsb *usb = memx_dev->pdev.usb;

    if (!data) { return MEMX_STATUS_MPUIO_INVALID_DATA; }
    if (length <= 0) { return MEMX_STATUS_MPUIO_INVALID_DATALEN; }

    // always reset actual transferred size
    if (transferred) { *transferred = 0; }
    struct memx_reg memx_rreg = {NULL, 0, 0};
    // write READ command to device
    memx_rreg.buf_addr = data;
    memx_rreg.reg_start = address;
    memx_rreg.size = length;
    int32_t ret = platform_ioctl(usb->ctrl_event, (usb->role == MEMX_MPU_TYPE_SPLIT) ? &usb->fdi : &usb->fd, MEMX_READ_REG, &memx_rreg, sizeof(struct memx_reg));
    if (transferred) { *transferred = (ret) ? 0 : length; }
    if (ret) {
        printf("failed to ioctl MEMX_READ_REG!\n");
        return MEMX_STATUS_PLATFORM_IOCTL_FAIL;
    }
    return MEMX_STATUS_OK;
}

memx_status memx_usb_stream_write(MemxMpuIo *mpuio, uint8_t chip_id, uint8_t flow_id, uint8_t *data,
                                            int32_t length, int32_t *transferred, int32_t timeout)
{
    unused(timeout);

    if (!mpuio || !mpuio->context) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    platform_device_t *memx_dev = (platform_device_t *)mpuio->context;
    if (!memx_dev->pdev.usb) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    MemxUsb *usb = memx_dev->pdev.usb;
    if (flow_id >= MEMX_MPUIO_FLOW_RING_BUFFER_NUMBER) { return MEMX_STATUS_MPUIO_INVALID_FLOW_ID; }
    if (!data) { return MEMX_STATUS_MPUIO_INVALID_DATA; }
    if (length <= 0) { return MEMX_STATUS_MPUIO_INVALID_DATALEN; }

#if __linux__
    struct timespec start_time;
    struct timespec end_time;
    clock_gettime(clock_id, &start_time);
    if (usb_prev_time_write.tv_sec != 0) {
        memx_usb_throughput_add(&usb_write_time_idle, (start_time.tv_sec - usb_prev_time_write.tv_sec) * 1000000 + (start_time.tv_nsec - usb_prev_time_write.tv_nsec) / 1000);
    }
#endif
    // always reset actual transferred size
    if (transferred) { *transferred = 0; }

    uint32_t total_tx_length = MEMX_MPUIO_IFMAP_HEADER_SIZE + length;
    uint32_t *tx_buffer = (uint32_t *)calloc(total_tx_length, sizeof(uint8_t));
    if (!tx_buffer) { printf("usb write : calloc fail\n"); return MEMX_STATUS_MPUIO_INVALID_DATALEN; }
    tx_buffer[0] = flow_id;
    tx_buffer[1] = length;
    tx_buffer[2] = chip_id;
    memcpy(&tx_buffer[16], data, length);
    platform_int32_t actual_platform_write = platform_write(usb->tx_event, (usb->role == MEMX_MPU_TYPE_SPLIT) ? &usb->fdi : &usb->fd, tx_buffer, total_tx_length);
    free(tx_buffer);
    memx_status status = MEMX_STATUS_OK;
    if (actual_platform_write > 0)
    {
        if (transferred) { *transferred = actual_platform_write - MEMX_MPUIO_IFMAP_HEADER_SIZE; }
    }
    else
    {
        printf("write error flow_id %d chip_id %d length %d\r\n", flow_id, chip_id, total_tx_length);
        status = MEMX_STATUS_PLATFORM_WRITE_FAIL;
    }

    MEMX_LOG(MEMX_LOG_MPUIO_IFMAP_FLOW, "flow[%d] write %d data to chip\n", flow_id, actual_platform_write);
#if __linux__
    clock_gettime(clock_id, &end_time);
    memx_usb_throughput_add(&usb_write_size, length);
    memx_usb_throughput_add(&usb_write_time, (end_time.tv_sec - start_time.tv_sec) * 1000000 + (end_time.tv_nsec - start_time.tv_nsec) / 1000);
    usb_prev_time_write.tv_sec = end_time.tv_sec;
    usb_prev_time_write.tv_nsec = end_time.tv_nsec;
#endif
    return status;
}

memx_status memx_usb_stream_read(MemxMpuIo *mpuio, uint8_t chip_id, uint8_t flow_id, uint8_t *data,
                                            int32_t length, int32_t *transferred, int32_t timeout)
{
    unused(timeout);

    if (!mpuio || !mpuio->context) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    platform_device_t *memx_dev = (platform_device_t *)mpuio->context;
    if (!memx_dev->pdev.usb) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    MemxUsb *usb = memx_dev->pdev.usb;

    if (flow_id >= MEMX_MPUIO_FLOW_RING_BUFFER_NUMBER) { return MEMX_STATUS_MPUIO_INVALID_FLOW_ID; }
    if (!data) { return MEMX_STATUS_MPUIO_INVALID_DATA; }
    if (length <= 0) { return MEMX_STATUS_MPUIO_INVALID_DATALEN; }

    if (transferred) { *transferred = 0; }

#if __linux__
    struct timespec start_time;
    struct timespec end_time;
    clock_gettime(clock_id, &start_time);
    if (usb_prev_time_read.tv_sec != 0) {
        memx_usb_throughput_add(&usb_read_time_idle, (start_time.tv_sec - usb_prev_time_read.tv_sec) * 1000000 + (start_time.tv_nsec - usb_prev_time_read.tv_nsec) / 1000);
    }
#endif
    // try to read data from control-ringbuffer
    // block to avoid memory violation
    int32_t actual_size = 0;
    // actual_size = (int)memx_ringbuffer_get(usb->flow_ringbuffer[flow_id], data, length);
    // TODO cache list len for currently list and pop untill empty?
    MemxRingBufferData *data_node = (MemxRingBufferData *)memx_list_pop(usb->flow_ringbuffer[chip_id][flow_id]);
    if (data_node) {
        memcpy(data, data_node->buffer, data_node->length);
        free(data_node->buffer);
        data_node->buffer = NULL;
        actual_size = data_node->length;
        free(data_node);
        data_node = NULL;
    } else {
        actual_size = 0;
    }

    // report actual transferred size if needed
    if (transferred) { *transferred = actual_size; }
#if __linux__
    clock_gettime(clock_id, &end_time);
    memx_usb_throughput_add(&usb_read_size, actual_size);
    memx_usb_throughput_add(&usb_read_time, (end_time.tv_sec - start_time.tv_sec) * 1000000 + (end_time.tv_nsec - start_time.tv_nsec) / 1000);
    usb_prev_time_read.tv_sec = end_time.tv_sec;
    usb_prev_time_read.tv_nsec = end_time.tv_nsec;
#endif
    return MEMX_STATUS_OK;
}

memx_status memx_usb_download_firmware(MemxMpuIo *mpuio, const char *file_path)
{
    if (!mpuio || !mpuio->context) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    platform_device_t *memx_dev = (platform_device_t *)mpuio->context;
    if (!memx_dev->pdev.usb) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    MemxUsb *usb = memx_dev->pdev.usb;

    if (!file_path) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    char *file_name = mpuio_comm_find_file_name(file_path);
    if (!file_name) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }

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
    int32_t ret = platform_ioctl(usb->ctrl_event, (usb->role == MEMX_MPU_TYPE_SPLIT) ? &usb->fdi : &usb->fd, MEMX_DOWNLOAD_FIRMWARE, &memx_firmware_bin, sizeof(memx_firmware_bin));
    if (ret) {
        printf("failed to ioctl MEMX_DOWNLOAD_FIRMWARE!\n");
        return MEMX_STATUS_PLATFORM_IOCTL_FAIL;
    }
    return MEMX_STATUS_OK;
}

memx_status memx_usb_download_model(MemxMpuIo *mpuio, uint8_t chip_id, void * pDfpMeta,
                                            uint8_t model_idx, int32_t type, int32_t timeout)
{
    unused(chip_id);
    unused(timeout);
    unused(model_idx);
    if (!mpuio || !mpuio->context) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    platform_device_t *memx_dev = (platform_device_t *)mpuio->context;
    if (!memx_dev->pdev.usb) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    MemxUsb *usb = memx_dev->pdev.usb;
    struct memx_bin memx_bin = {0};
    uint32_t dfp_fsize = 0;
    memx_status status = MEMX_STATUS_OK;
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

    memx_bin.total_size = dfp_fsize;
    memx_bin.dfp_cnt = *(uint32_t *)(memx_bin.buf + 12);

    if (memx_bin.dfp_cnt != (uint32_t)(usb->hw_info.chip.curr_config_chip_count / usb->hw_info.chip.group_count)) {
        return MEMX_STATUS_MPU_DOWNLOAD_MODEL_FAIL;
    }

    int32_t ret = platform_ioctl(usb->ctrl_event, (usb->role == MEMX_MPU_TYPE_SPLIT) ? &usb->fdi : &usb->fd, MEMX_RUNTIMEDWN_DFP, &memx_bin, sizeof(memx_bin));

    if (ret) {
        printf("failed to ioctl MEMX_RUNTIMEDWN_DFP!\n");
        return MEMX_STATUS_PLATFORM_IOCTL_FAIL;
    }
    return status;
}

memx_status memx_usb_set_ifmap_size(MemxMpuIo *mpuio, uint8_t chip_id, uint8_t flow_id,
    int32_t height, int32_t width, int32_t z, int32_t channel_number, int32_t format, int32_t timeout)
{
    unused(format);
    unused(timeout);
    if (!mpuio || !mpuio->context) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    platform_device_t *memx_dev = (platform_device_t *)mpuio->context;
    if (!memx_dev->pdev.usb) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    MemxUsb *usb = memx_dev->pdev.usb;

    usb->in_size[chip_id][flow_id] = height * width * z * channel_number;

    return MEMX_STATUS_OK; // do nothing
}

memx_status memx_usb_set_ofmap_size(MemxMpuIo *mpuio, uint8_t chip_id, uint8_t flow_id,
    int32_t height, int32_t width, int32_t z, int32_t channel_number, int32_t format, int32_t timeout)
{
    unused(timeout);
    // cascade plus using frame padding, so default set it enabled
    uint8_t isUsingFramePadding = (format == MEMX_MPUIO_OFMAP_FORMAT_GBF80_ROW_PAD) ? 0 : 1;
    if (!mpuio || !mpuio->context) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    platform_device_t *memx_dev = (platform_device_t *)mpuio->context;
    if (!memx_dev->pdev.usb) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    MemxUsb *usb = memx_dev->pdev.usb;
    usb->out_size[chip_id][flow_id] = mpuio_comm_cal_output_flow_size(height, width, z, channel_number, format, isUsingFramePadding);
    usb->out_width_size[chip_id][flow_id] = width;
    usb->out_height_size[chip_id][flow_id] = height;

    return MEMX_STATUS_OK;
}

memx_status memx_usb_update_fmap_size(MemxMpuIo *mpuio, uint8_t chip_id,
        uint8_t in_flow_count, uint8_t out_flow_count, int32_t timeout)
{
    unused(in_flow_count);
    unused(timeout);

    if (!mpuio || !mpuio->context) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    platform_device_t *memx_dev = (platform_device_t *)mpuio->context;
    if (!memx_dev->pdev.usb) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }
    MemxUsb *usb = memx_dev->pdev.usb;

    // FIXME: why we need to care take both count == 0 ?
    if ((in_flow_count == 0) && (out_flow_count == 0)) { return MEMX_STATUS_MPUIO_INVALID_CONTEXT; }

    struct memx_mpu_size mpu_size;

    for (uint8_t flow_id = 0; flow_id < MEMX_TOTAL_FLOW_COUNT; flow_id++) {
        mpu_size.flow_size[flow_id] = (flow_id >= out_flow_count) ? 0 : usb->out_size[chip_id][flow_id];
    }

    int32_t ret = -1;

    memx_usb_dynamic_allocate_buffer(&mpu_size, usb, out_flow_count, chip_id);

    for (uint8_t flow_id = 0; flow_id < MEMX_TOTAL_FLOW_COUNT; flow_id++) {
        if (mpu_size.buffer_size[flow_id]) {
            MEMX_LOG(MEMX_LOG_MPUIO_OFMAP_FLOW, "flow[%d] flow size: %d, buf size: %d\n", flow_id, mpu_size.flow_size[flow_id], mpu_size.buffer_size[flow_id]);
            if (mpu_size.flow_size[flow_id] % mpu_size.buffer_size[flow_id] || mpu_size.buffer_size[flow_id] % 4) {
                return MEMX_STATUS_MPU_INVALID_BUF_SIZE;
            }
        }
    }

    if (usb->hw_info.chip.generation == MEMX_MPU_CHIP_GEN_CASCADE_PLUS) {
        struct memx_chip_id memx_chip_id = {.chip_id = chip_id};
        // use command ID to lookup operation
        ret = platform_ioctl(usb->ctrl_event, &usb->fd, MEMX_SET_CHIP_ID, &memx_chip_id, sizeof(struct memx_chip_id));
        if (ret) {
            printf("failed to ioctl MEMX_SET_CHIP_ID!\n");
            return MEMX_STATUS_PLATFORM_IOCTL_FAIL;
        }
    }
    ret = platform_ioctl(usb->ctrl_event, (usb->role == MEMX_MPU_TYPE_SPLIT) ? &usb->fdo : &usb->fd, MEMX_DRIVER_MPU_IN_SIZE, &mpu_size, sizeof(struct memx_mpu_size));
    if (ret) { printf("failed to ioctl MEMX_DRIVER_MPU_IN_SIZE!\n"); return MEMX_STATUS_PLATFORM_IOCTL_FAIL; }
    return MEMX_STATUS_OK; // do nothing
}

static void memx_usb_dynamic_allocate_buffer(struct memx_mpu_size* mpu_size, MemxUsb *usb, uint8_t out_flow_count, uint8_t chip_id)
{
    uint32_t buffer_divider, buffer_type, ofmap_total_buffer_size, remaining_try_count;
    uint32_t total_buffer_size[MEMX_USB_OUTPUT_BUFFER_TYPE_COUNT] = {0};
    uint32_t buffer_size[MEMX_USB_OUTPUT_BUFFER_TYPE_COUNT][MEMX_TOTAL_FLOW_COUNT] = {0};
    uint32_t transfer_count_max[MEMX_USB_OUTPUT_BUFFER_TYPE_COUNT] = {0};

    for (buffer_type = 0; buffer_type < MEMX_USB_OUTPUT_BUFFER_TYPE_COUNT; buffer_type++) {
        buffer_divider = (buffer_type == MEMX_USB_OUTPUT_BUFFER_TYPE_SINGLE) ? MEMX_USB_OUTPUT_MPU_BUFFER_COUNT : (MEMX_USB_OUTPUT_DEVICE_BUFFER_COUNT * MEMX_USB_OUTPUT_MPU_BUFFER_COUNT);
        ofmap_total_buffer_size = (usb->hw_info.chip.generation == MEMX_MPU_CHIP_GEN_CASCADE_PLUS) ? (MEMX_USB_OUTPUT_BUFFER_SIZE / buffer_divider) : MEMX_USB_OUTPUT_BUFFER_SIZE;
        remaining_try_count = (usb->hw_info.chip.generation == MEMX_MPU_CHIP_GEN_CASCADE_PLUS) ? ((MEMX_MPUIO_MAX_ASSIGN_BUFFER_SIZE_TRY_COUNT - 1) / buffer_divider) : (MEMX_MPUIO_MAX_ASSIGN_BUFFER_SIZE_TRY_COUNT - 1);

        do {
            total_buffer_size[buffer_type] = 0;
            for (uint8_t flow_id = 0; flow_id < MEMX_TOTAL_FLOW_COUNT; flow_id++) {
                uint32_t flow_size = mpu_size->flow_size[flow_id];
                buffer_size[buffer_type][flow_id] = (flow_id < out_flow_count) ?
                    mpuio_comm_assign_suitable_buffer_size_for_flow(usb->out_height_size[chip_id][flow_id], flow_size, remaining_try_count) : 0;
                total_buffer_size[buffer_type] += (buffer_size[buffer_type][flow_id] + MEMX_MPUIO_OFMAP_HEADER_SIZE);
                if ((flow_id < out_flow_count) && ((mpu_size->flow_size[flow_id] / buffer_size[buffer_type][flow_id]) > transfer_count_max[buffer_type])) {
                    transfer_count_max[buffer_type] = (mpu_size->flow_size[flow_id] / buffer_size[buffer_type][flow_id]);
                }
            }
            // prevent infinite loop
            remaining_try_count--;
            // worst case set buf size to 4 for all enabled flow
            if (remaining_try_count == 0) {
                for (uint8_t flow_id = 0; flow_id < out_flow_count; flow_id++) {
                    printf("Warning: all output flow set to 4 bytes\n");
                    buffer_size[buffer_type][flow_id] = 4;
                    transfer_count_max[buffer_type] = ((mpu_size->flow_size[flow_id] / buffer_size[buffer_type][flow_id]) > transfer_count_max[buffer_type]) ? (mpu_size->flow_size[flow_id] / buffer_size[buffer_type][flow_id]) : transfer_count_max[buffer_type];
                }
                break;
            }
        } while (total_buffer_size[buffer_type] > ofmap_total_buffer_size);
    }

    buffer_type = memx_usb_choose_buffer_type(buffer_size[MEMX_USB_OUTPUT_BUFFER_TYPE_SINGLE], buffer_size[MEMX_USB_OUTPUT_BUFFER_TYPE_PINGPONG], total_buffer_size, transfer_count_max, mpu_size->flow_size, out_flow_count);

    for (uint8_t flow_id = 0; flow_id < MEMX_TOTAL_FLOW_COUNT; flow_id++) {
        mpu_size->buffer_size[flow_id] = buffer_size[buffer_type][flow_id];
    }

    usb->pipeline_flag = (buffer_type == MEMX_USB_OUTPUT_BUFFER_TYPE_PINGPONG) ? true : false;
    mpu_size->usb_first_chip_pipeline_flag = usb->pipeline_flag;
    mpu_size->usb_last_chip_pingpong_flag = 1; // Ping-pong on for normal case. Corner case can be zero in the future.
}

static uint32_t memx_usb_choose_buffer_type(uint32_t* buffer_size_single, uint32_t* buffer_size_pingpong, uint32_t* total_buffer_size, uint32_t* transfer_count_max, uint32_t* flow_size, uint8_t out_flow_count)
{
    uint32_t single_flow_count_total = transfer_count_max[MEMX_USB_OUTPUT_BUFFER_TYPE_SINGLE];
    uint32_t buffer_type = MEMX_USB_OUTPUT_BUFFER_TYPE_PINGPONG;

    if (total_buffer_size[MEMX_USB_OUTPUT_BUFFER_TYPE_SINGLE] > MEMX_USB_COMMON_BUFFER_SIZE) {
        uint32_t min_flow_count = 0;
        uint32_t buffer_size_with_min_flow_count = 0;
        transfer_count_max[MEMX_USB_OUTPUT_BUFFER_TYPE_SINGLE] = 0;

        for (;min_flow_count < single_flow_count_total; min_flow_count++) {
            for (uint8_t flow_id = 0; flow_id < out_flow_count; flow_id++) {
                if ((flow_size[flow_id] / buffer_size_single[flow_id]) > min_flow_count) {
                    buffer_size_with_min_flow_count += buffer_size_single[flow_id];
                }

                if (buffer_size_with_min_flow_count > MEMX_USB_COMMON_BUFFER_SIZE) {
                    transfer_count_max[MEMX_USB_OUTPUT_BUFFER_TYPE_SINGLE] += 2;
                    break;
                } else if (flow_id == (out_flow_count - 1)) transfer_count_max[MEMX_USB_OUTPUT_BUFFER_TYPE_SINGLE] += 1;
            }
            buffer_size_with_min_flow_count = 0;
        }
    }

    if (MEMX_USB_DYNAMIC_BUFFER_RULE_1) {
        for (uint8_t flow_id = 0; flow_id < out_flow_count; flow_id++) {
            if (MEMX_USB_DYNAMIC_BUFFER_RULE_2 || MEMX_USB_DYNAMIC_BUFFER_RULE_3) {
                buffer_type = MEMX_USB_OUTPUT_BUFFER_TYPE_SINGLE;
                break;
            }
        }
    }

    return buffer_type;
}
