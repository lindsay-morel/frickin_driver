/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#ifndef MEMX_PCIE_H_
#define MEMX_PCIE_H_

#ifdef __cplusplus
extern "C" {
#endif
#include "memx_pcie_xflow.h"
#include "memx_platform.h"
#include "memx_mpuio.h"

#define MEMX_PCIE_READ_BUFFER_SIZE (MEMX_PCIE_OFMAP_DMA_COHERENT_MMAP_SIZE_512KB)
#define MEMX_PCIE_WRITE_BUFFER_SIZE (MEMX_PCIE_OFMAP_DMA_COHERENT_MMAP_SIZE_512KB)
#define MEMX_PCIE_READ_BUFFER_ALIGNMENT (4 * 1024)

#define MEMX_PCIE_BAR0_MMAP_SIZE_256MB (0x10000000)
#define MEMX_PCIE_BAR0_MMAP_SIZE_128MB (0x8000000)
#define MEMX_PCIE_BAR0_MMAP_SIZE_64MB  (0x4000000)
#define MEMX_PCIE_BAR0_MMAP_SIZE_16MB  (0x1000000)
#define MEMX_PCIE_BAR1_MMAP_SIZE_1MB   (0x100000)
#define MEMX_PCIE_BAR0_MMAP_SIZE_512KB (0x80000)
#define MEMX_PCIE_BAR0_MMAP_SIZE_256KB (0x40000)
#define MEMX_PCIE_BAR0_MMAP_SIZE_4KB   (0x1000)

#define MEMX_PCIE_DMA_BUFFER_SIZE    (0x200000)
#define MEMX_PCIE_OFMAP_DMA_COHERENT_MMAP_SIZE_512KB (0x80000)
#define MEMX_PCIE_OFMAP_DMA_COHERENT_MMAP_SIZE_256KB (0x40000)

// pcie ofmap rx sram common header format:
/*
+------------------+---------+----------+------------------+------------------------+----------+----------------------+----------+--------------+--------------+
| EGR pkt header   | [0:3]   |  [4:7]   | [8:11]           | [12:15]                | [16:19]  | [20:23]              | [24:55]  | [56:59]      | [60:63]      |
+------------------+---------+----------+------------------+------------------------+----------+----------------------+----------+--------------+--------------+
| Define           | flow_id | buf_size | output_flow_flag | output_flow_total_size | chip_id  | output_buf_count     | reserved | EGR buf hdr 0| EGR buf hdr 1|
+------------------+---------+----------+------------------+------------------------+----------+----------------------+----------+--------------+--------------+
*/
#define MEMX_OFMAP_SRAM_COMMON_HEADER_SIZE (64)
#define MEMX_OFMAP_SRAM_COMMON_HEADER_FLOW_ID_OFFSET (0)
#define MEMX_OFMAP_SRAM_COMMON_HEADER_BUFFER_SIZE_OFFSET (4)
#define MEMX_OFMAP_SRAM_COMMON_HEADER_TRIGGER_BITS_OFFSET (8)
#define MEMX_OFMAP_SRAM_COMMON_HEADER_TOTAL_LENGTH_OFFSET (12)
#define MEMX_OFMAP_SRAM_COMMON_HEADER_CHIP_ID_OFFSET (16)
#define MEMX_OFMAP_SRAM_COMMON_HEADER_OUTPUT_BUF_CNT_OFFSET (20)

#define MEMX_PCIE_EGRESS_DCORE_COMMON_HEADER_SIZE (8)
#define MEMX_PCIE_EGRESS_DCORE_HEADER0_OFFSET (56)
#define MEMX_PCIE_EGRESS_DCORE_HEADER0_FLOW_ID_MASK (0x1F)
#define MEMX_PCIE_EGRESS_DCORE_HEADER0_EON_OF_FRAME_AND_BUFFER_MASK (0x300)
#define MEMX_PCIE_EGRESS_DCORE_HEADER1_OFFSET (60)
#define MEMX_PCIE_EGRESS_DCORE_HEADER1_BUFFER_COUNT_MASK (0xFFFFFFFF)
#define SEP_NEXT_OFS (4)
#define DFP_WEIGHT_MEMORY_COMMON_HEADER_SIZE (8)

#define MEMX_PCIE_FIRMWARE_DOWNLOAD_SRAM_INIT_VAL (0x4D4D4D4D)

#define MEMX_PCIE_IRQ_OFFSET(C, I) ((((15 - (C)) * DEVICE_IRQ_COUNT) + (I)) * 4 + 0x200)

struct _MemxList;

typedef struct{
    uint32_t        chip_id;
    uint32_t        des_addr_base;
    void*           src_buf_base;
    uint32_t        total_size;
    uint32_t        written_size;
} WmemEntry, *pWmemEntry;

typedef struct{
    pWmemEntry      pContext;
    uint16_t        uwMaxEntryCnt;
    uint16_t        uwValidEntryCnt;
    uint16_t        uwCurEntryIdx;
} DevWmemContext, *pDevWmemContext;

/**
 * @brief Package of PCIe devices includes one master and one slave devices or one device
 * to perform TX and RX transfer.
 */
typedef struct _MemxPcie {
    uint32_t        role;         // device type, single or split
    platform_fd_t   fd;           // device file node
    platform_fd_t   fdi;          // reserved for multi-device support
    platform_fd_t   fdo;          // reserved for multi-device support
    platform_fd_t   fd_feature;   // device file node, used in main thread for admin command
    uint8_t         group_id;     // record current MPU IO structure belong to which device group
#if __linux__                           // Note: Only Linux platform can support mmap api.
    volatile uint8_t  *mmap_mpu_base; 	//  pcie default use bar0 to mmap whole MPU HW
                                        //  MPU : 0x3000_0000 ~ 0x3FFF_FFFF(256MB)
                                        //  - Data Intf.	0x3000_0000	~ 0x37FF_FFFF(128 MB)
                                        //  - Cnfg. Intf.	0x3800_0000	~ 0x3FFF_FFFF(128 MB)
    volatile uint8_t  *mmap_xflow_vbuf_base; // mmap xflow vbuf addr, only use in MEMXBAR_3BAR_BAR0VB_BAR2CI_64MB_BAR4SRAM and MEMXBAR_3BAR_BAR0VB_BAR2CI_16MB_BAR4SRAM mode
    volatile uint8_t  *mmap_device_irq_base;
#endif
	struct _MemxList *flow_ringbuffer[MEMX_MPUIO_MAX_HW_MPU_COUNT][MEMX_MPUIO_FLOW_RING_BUFFER_NUMBER];

	platform_thread_t background_crawler; // background worker who keeps pulling out data from device
	uint8_t *rx_buffer; // mpuio-pcie use kdriver-pcie dma_alloc_coherent's buffer which sharing with our firmware.
						// It's a host && chip DMA Buffer : dma_coherent_physical_address ~ (1MB)
                        //  - rx_buffer: MEMX_PCIE_READ_BUFFER_SIZE (512 KB)
    uint8_t *tx_buffer; //  - tx_buffer: MEMX_PCIE_WRITE_BUFFER_SIZE (512 KB)

    uint8_t is_ready_exit; // use to force break the rx background caller task
    uint8_t is_read_abort; // use to force release read data in rx background caller task

    platform_handle_t rx_event;	    // windows rx
    platform_handle_t tx_event;	    // windows tx
    platform_handle_t ctrl_event;	// windows ioctl

    uint32_t in_size[MEMX_MPUIO_MAX_HW_MPU_COUNT][MEMX_MPUIO_FLOW_RING_BUFFER_NUMBER]; // record mpu flow size of input fmap size (to mpu)
    uint32_t out_size[MEMX_MPUIO_MAX_HW_MPU_COUNT][MEMX_MPUIO_FLOW_RING_BUFFER_NUMBER]; // record mpu flow size of output fmap size (to host)
    uint32_t out_width_size[MEMX_MPUIO_MAX_HW_MPU_COUNT][MEMX_MPUIO_FLOW_RING_BUFFER_NUMBER]; // record mpu flow size of output fmap width size
    uint32_t out_height_size[MEMX_MPUIO_MAX_HW_MPU_COUNT][MEMX_MPUIO_FLOW_RING_BUFFER_NUMBER]; // record mpu flow size of output fmap height size

    uint32_t final_start_flows_bits[MEMX_MPUIO_MAX_HW_MPU_COUNT]; // record active ofmap flow bits
    struct hw_info hw_info;
    platform_mutex_t xflow_write_guard[MEMX_MPUIO_MAX_HW_MPU_COUNT]; // lock for xflow write to prevent xflow addr racing
    platform_mutex_t xflow_access_guard_first_chip_control;
    platform_mutex_t write_guard; // lock for write to prevent racing
    platform_mutex_t context_guard; // lock for context changed to prevent racing
    DevWmemContext   download_wmem_context[MEMX_MPUIO_MAX_HW_MPU_COUNT];
} MemxPcie;

/**
 * @brief creeate pcie context for mpuio.
 *
 * @param memx_dev_ptr_src  pointer which we allocated memory
 * @param group_id		   target group id
 * @param chip_gen         target MPU chip generation (MEMX_MPU_CHIP_GEN_CASCADE_PLUS)
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_pcie_create_context(platform_device_t **memx_dev_ptr_src, uint8_t group_id, uint8_t chip_generation);

/**
 * @brief Set up read abort flag to the given MPUIO context.
 *
 * @param mpuio               MPUIO context
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_pcie_set_read_abort_start(MemxMpuIo *mpuio);

/**
 * @brief Destroy the MPUIO context created by cleaning-up all resources
 * allocated within 'create' function.
 *
 * @param mpuio               MPUIO context
 *
 * @return none
 */
void memx_pcie_destroy(MemxMpuIo *mpuio);

/**
 * @brief Gerneral purpose PCIE operation. The command available could be
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
memx_status memx_pcie_operation(MemxMpuIo *mpuio, uint8_t chip_id, int32_t cmd_id, void *data, uint32_t size, int32_t timeout);

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
memx_status memx_pcie_control_write(MemxMpuIo *mpuio, uint8_t chip_id, uint32_t address, uint8_t *data,
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
memx_status memx_pcie_control_read(MemxMpuIo *mpuio, uint8_t chip_id, uint32_t address, uint8_t *data,
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
memx_status memx_pcie_stream_write(MemxMpuIo *mpuio, uint8_t chip_id, uint8_t flow_id, uint8_t *data,
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
memx_status memx_pcie_stream_read(MemxMpuIo *mpuio, uint8_t chip_id, uint8_t flow_id, uint8_t *data,
                                            int32_t length, int32_t *transferred, int32_t timeout);

/**
 * @brief Download model data flow program to device. Data flow program should
 * be generated using MIX compiler. Currently PCIE version is strongly based on
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
memx_status memx_pcie_download_model(MemxMpuIo *mpuio, uint8_t chip_id, void * pDfpMeta,
                                                uint8_t model_idx, int32_t type, int32_t timeout);

/**
 * @brief Download firmware to device flash. .
 *
 * @param mpuio               IO handle context
 * @param file_path          firmware file, which by default should be named cascade.bin
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_pcie_download_firmware(MemxMpuIo *mpuio, const char *file_path);

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
memx_status memx_pcie_set_ifmap_size(MemxMpuIo *mpuio, uint8_t chip_id, uint8_t flow_id, int32_t height,
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
memx_status memx_pcie_set_ofmap_size(MemxMpuIo *mpuio, uint8_t chip_id, uint8_t flow_id, int32_t height,
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
memx_status memx_pcie_update_fmap_size(MemxMpuIo *mpuio, uint8_t chip_id, uint8_t in_flow_count, uint8_t out_flow_count, int32_t timeout);

void memx_pcie_trigger_device_irq(struct _MemxPcie *memx_dev, uint8_t chip_id, xflow_mpu_sw_irq_idx_t sw_irq_idx);

#ifdef __cplusplus
}
#endif

#endif /* MEMX_PCIE_H_ */
