/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#ifndef MEMX_USB_H_
#define MEMX_USB_H_

#ifdef __cplusplus
extern "C" {
#endif
#include "memx_platform.h"
#include "memx_mpuio.h"

#define MEMX_USB_COMMON_BUFFER_SIZE         (64 * 1024) /*USB packet size*/
#define MEMX_USB_READ_BUFFER_SIZE           (128 * 1024) /*USB read packet size*/
#define MEMX_USB_CONTROLWRITE_SIZE          (16 * 1024) /*USB packet size*/
#define MEMX_USB_OUTPUT_BUFFER_SIZE         (256 * 1024)
#define MEMX_USB_OUTPUT_BUFFER_BACKUP_COUNT (2)
#define MEMX_USB_OUTPUT_BUFFER_BACKUP_SIZE  (MEMX_USB_OUTPUT_BUFFER_SIZE * MEMX_USB_OUTPUT_BUFFER_BACKUP_COUNT)

#define MEMX_USB_OUTPUT_MPU_BUFFER_COUNT (2)
#define MEMX_USB_OUTPUT_DEVICE_BUFFER_COUNT (2)

#define MEMX_USB_DYNAMIC_BUFFER_RULE_1 (transfer_count_max[MEMX_USB_OUTPUT_BUFFER_TYPE_SINGLE] * 2 <= transfer_count_max[MEMX_USB_OUTPUT_BUFFER_TYPE_PINGPONG])
#define MEMX_USB_DYNAMIC_BUFFER_RULE_2 (buffer_size_pingpong[flow_id] * 15 < buffer_size_single[flow_id])
#define MEMX_USB_DYNAMIC_BUFFER_RULE_3 ((total_buffer_size[MEMX_USB_OUTPUT_BUFFER_TYPE_PINGPONG] * 2 < total_buffer_size[MEMX_USB_OUTPUT_BUFFER_TYPE_SINGLE]) && (buffer_size_pingpong[flow_id] * 3 <= buffer_size_single[flow_id]))

// TODO: What limit for cascade plus? dynamic assign?
#define DEFAULT_MPU_OUT_SIZE            54000       /*Set 54KB size on chip for one time bulk out size*/
#define DEFAULT_FW_OUT_SIZE             18000       /*Set 18KB size on chip for one time ingress dcode program size*/

typedef enum MEMX_USB_OUTPUT_BUFFER_TYPE {
    MEMX_USB_OUTPUT_BUFFER_TYPE_PINGPONG = 0,
    MEMX_USB_OUTPUT_BUFFER_TYPE_SINGLE,
    MEMX_USB_OUTPUT_BUFFER_TYPE_COUNT
} MEMX_USB_OUTPUT_BUFFER_TYPE;

struct _MemxList;
/**
 * @brief Package of USB devices includes one master and one slave devices or one device
 * to perform TX and RX transfer.
 */
typedef struct _MemxUsb {
    uint32_t role; // device type, single or split
    platform_fd_t fd;   // device file node
    platform_fd_t fdi;  // reserved for multi-device support
    platform_fd_t fdo;  // reserved for multi-device support
    platform_thread_t background_crawler; // background worker who keeps pulling out data from device

    struct _MemxList *flow_ringbuffer[MEMX_MPUIO_MAX_HW_MPU_COUNT][MEMX_MPUIO_FLOW_RING_BUFFER_NUMBER];
    uint8_t *rx_buffer; // read buffer pointer
    uint8_t *tx_buffer; // write buffer pointer
    uint8_t is_ready_exit;
    uint8_t is_read_abort; // use to force release read data in rx background caller task

    platform_handle_t rx_event;	    // windows rx
    platform_handle_t tx_event;	    // windows tx
    platform_handle_t ctrl_event;	// windows ioctl

    uint32_t in_size[MEMX_MPUIO_MAX_HW_MPU_COUNT][MEMX_MPUIO_FLOW_RING_BUFFER_NUMBER]; // record mpu flow size of input fmap size (bulk-out)
    uint32_t out_size[MEMX_MPUIO_MAX_HW_MPU_COUNT][MEMX_MPUIO_FLOW_RING_BUFFER_NUMBER]; // record mpu flow size of output fmap size (bulk-in)
    uint32_t out_width_size[MEMX_MPUIO_MAX_HW_MPU_COUNT][MEMX_MPUIO_FLOW_RING_BUFFER_NUMBER]; // record mpu flow size of output fmap width size
    uint32_t out_height_size[MEMX_MPUIO_MAX_HW_MPU_COUNT][MEMX_MPUIO_FLOW_RING_BUFFER_NUMBER]; // record mpu flow size of output fmap height size
    struct hw_info hw_info; // record hardware info from device
    platform_mutex_t context_guard; // lock for context changed to prevent racing

    uint8_t pipeline_flag;
} MemxUsb;

/**
 * @brief creeate usb context for mpuio.
 *
 * @param memx_dev_ptr_src  pointer which we allocated memory
 * @param group_id		   target group id
 * @param chip_gen         target MPU chip generation (MEMX_MPU_CHIP_GEN_CASCADE_PLUS)
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_usb_create_context(platform_device_t **memx_dev_ptr_src, uint8_t group_id, uint8_t chip_gen);

/**
 * @brief Set up read abort flag to the given MPUIO context.
 *
 * @param mpuio               MPUIO context
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_usb_set_read_abort_start(MemxMpuIo *mpuio);

/**
 * @brief Destroy the MPUIO context created by cleaning-up all resources
 * allocated within 'create' function.
 *
 * @param mpuio               MPUIO context
 *
 * @return none
 */
void memx_usb_destroy(MemxMpuIo *mpuio);

/**
 * @brief Gerneral purpose USB operation. The command available could be
 * found within 'memx_command.h'.
 *
 * @param mpuio               IO handle context
 * @param chip_id             target chip ID
 * @param cmd_id              command ID
 * @param data                command data
 * @param size                command data size
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_usb_operation(MemxMpuIo *mpuio, uint8_t chip_id, int32_t cmd_id, void *data, uint32_t size, int32_t timeout);

/**
 * @brief Control channel write multiple bytes to specific bus address. The
 * address given can be either a register address or a SRAM address.
 *
 * @param mpuio               MPUIO context
 * @param chip_id             target chip ID
 * @param address             bus address
 * @param data                byte buffer to write
 * @param length              byte length to write
 * @param transferred         actual transferred byte length
 * @param increment           post-write-address-increase enable
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_usb_control_write(MemxMpuIo *mpuio, uint8_t chip_id, uint32_t address, uint8_t *data,
							int32_t length, int32_t *transferred, int32_t increment, int32_t timeout);

/**
 * @brief Control channel read multiple bytes from specific bus address. The
 * address given can be either a register address or a SRAM address.
 *
 * @param mpuio               MPUIO context
 * @param chip_id             target chip ID
 * @param address             bus address
 * @param data                byte buffer to store data read
 * @param length              byte length to read
 * @param transferred         actual transferred byte length
 * @param increment           post-read-address-increase enable
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_usb_control_read(MemxMpuIo *mpuio, uint8_t chip_id, uint32_t address, uint8_t *data,
							int32_t length, int32_t *transferred, int32_t increment, int32_t timeout);

/**
 * @brief Data channel write multiple bytes to specific data flow.
 *
 * @param mpuio               MPUIO context
 * @param chip_id             target chip ID
 * @param flow_id             target flow ID
 * @param data                byte buffer to write
 * @param length              byte length to write
 * @param transferred         actual transferred byte length
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_usb_stream_write(MemxMpuIo *mpuio, uint8_t chip_id, uint8_t flow_id, uint8_t *data,
											int32_t length, int32_t *transferred, int32_t timeout);

/**
 * @brief Data channel read multiple bytes from specific data flow.
 *
 * @param mpuio               MPUIO context
 * @param chip_id             target chip ID
 * @param flow_id             target flow ID
 * @param data                byte buffer to store data read
 * @param length              byte length to read
 * @param transferred         actual transferred byte length
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_usb_stream_read(MemxMpuIo *mpuio, uint8_t chip_id, uint8_t flow_id, uint8_t *data,
											int32_t length, int32_t *transferred, int32_t timeout);

/**
 * @brief Download model data flow program to device. Data flow program should
 * be generated using MIX compiler. Currently USB version is strongly based on
 * Cascade design which will not be able to apply to other device.
 *
 * @param mpuio               IO handle context
 * @param chip_id             target chip ID
 * @param file_path           model dfp file, which by default should be named as '<device_name>.dfp'
 * @param model_idx           which rgcfg to use (used for model swapping). Set to 0 if only 1 model.
 * @param type                fp type, 0: wtmem, 1: config
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_usb_download_model(MemxMpuIo *mpuio, uint8_t chip_id, void * pDfpMeta,
												uint8_t model_idx, int32_t type, int32_t timeout);

/**
 * @brief Download firmware to device flash. .
 *
 * @param mpuio               IO handle context
 * @param file_path          firmware file, which by default should be named cascade.bin
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_usb_download_firmware(MemxMpuIo *mpuio, const char *file_path);

/**
 * @brief Configure input feature map size for specific data flow
 *
 * @param mpuio               MPUIO context
 * @param chip_id             target chip ID
 * @param flow_id             target flow ID
 * @param height              input feature map size height
 * @param width               input feature map size width
 * @param z                   input feature map size z
 * @param channel_number      input feature map size channel number
 * @param format              input feature map format
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_usb_set_ifmap_size(MemxMpuIo *mpuio, uint8_t chip_id, uint8_t flow_id, int32_t height,
								int32_t width, int32_t z, int32_t channel_number, int32_t format, int32_t timeout);

/**
 * @brief Configure output feature map size for specific data flow
 *
 * @param mpuio               MPUIO context
 * @param chip_id             target chip ID
 * @param flow_id             target flow ID
 * @param height              output feature map size height
 * @param width               output feature map size width
 * @param z                   output feature map size z
 * @param channel_number      output feature map size channel number
 * @param format              output feature map format
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_usb_set_ofmap_size(MemxMpuIo *mpuio, uint8_t chip_id, uint8_t flow_id, int32_t height,
								int32_t width, int32_t z, int32_t channel_number, int32_t format, int32_t timeout);

/**
 * @brief Update input/output feature map size to chip.
 *
 * @param mpuio               MPUIO context
 * @param chip_id             target chip ID
 * @param in_flow_count       total number of input feature map
 * @param out_flow_count      total number of output feature map
 * @param timeout             milliseconds timeout, '0' indicates infinite
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_usb_update_fmap_size(MemxMpuIo *mpuio, uint8_t chip_id, uint8_t in_flow_count, uint8_t out_flow_count, int32_t timeout);

#ifdef __cplusplus
}
#endif

#endif /* MEMX_USB_H_ */
