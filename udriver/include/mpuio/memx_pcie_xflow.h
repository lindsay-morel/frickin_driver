#ifndef _MEMX_PCIE_XFLOW_H_
#define _MEMX_PCIE_XFLOW_H_
#include "memx_common.h"

#define XFLOW_BASE_ADDRESS_REGISTER_OFFSET      (0x0)
#define XFLOW_CONTROL_REGISTER_OFFSET           (0x4)

#define XFLOW_CONFIG_REG_PREFIX                 (0xC000000)
#define XFLOW_VIRTUAL_BUFFER_PREFIX             (0x8000000)

#define XFLOW_CHIP_ID_WIDTH                     (0xF)
#define XFLOW_CHIP_ID_SHIFT                     (22)
#define XFLOW_MAX_BURST_WRITE_SIZE              (256*256*1024)
#define XFLOW_BURST_WRITE_BATCH_SIZE            (64)

#define MXCNST_RP_XFLOW_ADDR (0x60000000)

#define GET_XFLOW_OFFSET(chip_id, is_config) \
    (((is_config) ? XFLOW_CONFIG_REG_PREFIX : XFLOW_VIRTUAL_BUFFER_PREFIX) | \
     (((chip_id) & XFLOW_CHIP_ID_WIDTH) << XFLOW_CHIP_ID_SHIFT))

#define GET_XFLOW_DATAFLOW_FIFO_OFFSET(chip_id, flow_id) (((flow_id & 0x1f) << 22) | ((chip_id & 0xf) << 18))

typedef enum xflow_mpu_sw_irq_idx {
    egress_dcore_done_idx_0 = 0,    //  should only be used in our fw.
    dump_xflow_err_status_idx_1,    //  only for debug fw xflow purpose.
    enable_mpu_nvic_idx_2,
    reset_device_idx_3,
    fw_cmd_idx_4,
    move_sram_data_to_di_port_idx_5,
    init_wtmem_and_fmem_idx_6,
    reset_mpu_idx_7,
    max_support_mpu_sw_irq_num
} xflow_mpu_sw_irq_idx_t;

typedef enum xflow_operation_type {
    XFLOW_OPERATION_TYPE_SET_MODE = 0,
    XFLOW_OPERATION_TYPE_SET_ADDRESS = 1,
    XFLOW_OPERATION_TYPE_WRITE_VIRTUAL_BUFFER = 2,
    XFLOW_OPERATION_TYPE_COUNT
} xflow_operation_type_t;

#define DEVICE_IRQ_COUNT ((reset_mpu_idx_7) - (enable_mpu_nvic_idx_2) + 1)

struct _MemxPcie;
int32_t memx_xflow_basic_check(struct _MemxPcie *memx_dev, uint8_t chip_id);
#ifdef __linux__
int32_t memx_xflow_burst_write_wtmem(struct _MemxPcie *memx_dev, uint8_t chip_id, uint32_t base_addr, uint8_t *data, uint32_t write_size, bool access_mpu);
#endif
int32_t memx_xflow_write(struct _MemxPcie *memx_dev, uint8_t chip_id, uint32_t base_addr, uint32_t base_addr_offset, uint32_t value, bool access_mpu);
int32_t memx_xflow_read(struct _MemxPcie *memx_dev, uint8_t chip_id, uint32_t base_addr, uint32_t base_addr_offset, bool access_mpu);
#endif