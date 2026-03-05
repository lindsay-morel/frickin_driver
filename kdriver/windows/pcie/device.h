/**
 * @file device.h
 * @author Gary Chang (gary.chang@memryx.com)
 * @brief This file contains the device definitions.
 * @version 0.1
 * @date 2023
 *
 * @copyright Copyright (c) 2023 MemryX. Inc. All Rights Reserved.
 *
 */
#pragma once
#include "public.h"

EXTERN_C_START

#define PCI_CONFIG_SPACE_SIZE           (DEF_KB(4))

#define MEMX_HOST_BUFFER_TOTAL_SIZE     (DEF_MB(2))
#define MEMX_HOST_BUFFER_BLOCK_SIZE     (DEF_KB(512))

#define MAX_MSIX                        (96)
#define FW_DEFAULT_VALUE                (0x4D4D4D4D)
#define DEV_CODE_SRAM_BASE              (0x40040000)
#define DEV_CODE_SRAM_SIZE              (DEF_KB(768))

#define DEV_CODE_SRAM_MPUUTIL_BASE      (0x40046d00)

#define PHYSICAL_MPU_BASE               (0x30000000)
#define PHYSICAL_SRAM_BASE              (0x40000000)
#define PHYSICAL_FW_DOWNLOAD_SRAM_BASE  (0x40040000)

#define AHB_HUB_IRQ_EN_BASE             (0x30400008)

#define MEMX_DBGLOG_CHIP_LOG_64ID                   (4)
#define MEMX_DBGLOG_STORE_ALL_FIFO_INFO_SIZE_64KB   (0x10000)
#define MEMX_DBGLOG_CHIP_BUFFER_SIZE_64KB           (0x10000)
#define MEMX_DBGLOG_CHIP_BUFFER_SIZE_32KB           (0x08000)
#define MEMX_DBGLOG_CHIP_BUFFER_SIZE(chip_id)       (((chip_id) >= MEMX_DBGLOG_CHIP_LOG_64ID)?MEMX_DBGLOG_CHIP_BUFFER_SIZE_32KB:MEMX_DBGLOG_CHIP_BUFFER_SIZE_64KB)


#define MEMX_DBGLOG_DMA_HOST_BUFFER_OFFSET  (0x100000)
    #define MEMX_DBGLOG_RWPTR_OFFSET            (0xFF000+MEMX_DBGLOG_DMA_HOST_BUFFER_OFFSET)
    #define MEMX_RMTCMD_CONTROLBASE_OFFSET      (0xFE000+MEMX_DBGLOG_DMA_HOST_BUFFER_OFFSET)
    #define MEMX_RMTCMD_PARAM2BASE_OFFSET       (0xFE200+MEMX_DBGLOG_DMA_HOST_BUFFER_OFFSET)
    #define MEMX_DVFS_UTILBASE_OFFSET           (0xFD000+MEMX_DBGLOG_DMA_HOST_BUFFER_OFFSET)

/* This part is host buffer use from end of debuglog in front direction, Front direction have (1024 - 32 - debuglog_used) allowed */
#define MEMX_DBGLOG_END_OFFSET                (MEMX_DBGLOG_DMA_HOST_BUFFER_OFFSET+(MEMX_DBGLOG_CHIP_LOG_64ID*MEMX_DBGLOG_CHIP_BUFFER_SIZE_64KB)+((16-MEMX_DBGLOG_CHIP_LOG_64ID)*MEMX_DBGLOG_CHIP_BUFFER_SIZE_32KB))
#define MEMX_DMA_DESC_START_OFFSET            MEMX_DBGLOG_END_OFFSET //0xA0000 for case4
#define MEMX_DMA_DESC_DESC_COUNT_PER_CHIP     (2)
#define MEXM_DMA_DESC_BLKSZ                   (0x800) //((512/16)*64) = 2KB
#define MEMX_DMA_CHIP0_DESC0_OFFSET           (MEMX_DMA_DESC_START_OFFSET) //0xA0000
#define MEMX_DMA_CHIP0_DESC1_OFFSET           (MEMX_DMA_CHIP0_DESC0_OFFSET+MEXM_DMA_DESC_BLKSZ) //0xA0800
#define MEMX_DMA_CHIP1_DESC0_OFFSET           (MEMX_DMA_CHIP0_DESC1_OFFSET+MEXM_DMA_DESC_BLKSZ) //0xA1000
#define MEMX_DMA_CHIP1_DESC1_OFFSET           (MEMX_DMA_CHIP1_DESC0_OFFSET+MEXM_DMA_DESC_BLKSZ) //0xA1800
#define MEMX_DMA_CHIP2_DESC0_OFFSET           (MEMX_DMA_CHIP1_DESC1_OFFSET+MEXM_DMA_DESC_BLKSZ) //0xA2000
#define MEMX_DMA_CHIP2_DESC1_OFFSET           (MEMX_DMA_CHIP2_DESC0_OFFSET+MEXM_DMA_DESC_BLKSZ) //0xA2800
#define MEMX_DMA_CHIP3_DESC0_OFFSET           (MEMX_DMA_CHIP2_DESC1_OFFSET+MEXM_DMA_DESC_BLKSZ) //0xA3000
#define MEMX_DMA_CHIP3_DESC1_OFFSET           (MEMX_DMA_CHIP3_DESC0_OFFSET+MEXM_DMA_DESC_BLKSZ) //0xA3800
#define MEMX_DMA_DESC_CHIPADDR(ch, idx, chip) (MEMX_DBGLOG_PCIE_LINK0_BASE + MEMX_DMA_CHIP0_DESC0_OFFSET + ((ch)*MEXM_DMA_DESC_BLKSZ)+((idx)*64)+(chip)*MEMX_DMA_DESC_DESC_COUNT_PER_CHIP*MEXM_DMA_DESC_BLKSZ)

#define MEMX_DMA_DESC_BUFFER_BLOCK_NUMBER(ADDR)     ((ADDR) >> 19) // The same as ADDR / MEMX_HOST_BUFFER_BLOCK_SIZE
#define MEMX_DMA_DESC_BUFFER_BLOCK_OFFSET(ADDR)     ((ADDR) & (MEMX_HOST_BUFFER_BLOCK_SIZE - 1))

#define MEMX_DBGLOG_PCIE_LINK0_BASE         (0x58000000)
    #define MEMX_GET_CHIP_DBGLOG_BUFFER_BUS_ADDR(chip_id)       (((chip_id) >= MEMX_DBGLOG_CHIP_LOG_64ID) ? \
		(MEMX_DBGLOG_PCIE_LINK0_BASE + MEMX_DBGLOG_DMA_HOST_BUFFER_OFFSET + (MEMX_DBGLOG_CHIP_BUFFER_SIZE_64KB*MEMX_DBGLOG_CHIP_LOG_64ID) + (((chip_id)-MEMX_DBGLOG_CHIP_LOG_64ID) * MEMX_DBGLOG_CHIP_BUFFER_SIZE_32KB)) : \
		(MEMX_DBGLOG_PCIE_LINK0_BASE + MEMX_DBGLOG_DMA_HOST_BUFFER_OFFSET + ((chip_id) * MEMX_DBGLOG_CHIP_BUFFER_SIZE_64KB)))
    #define MEMX_GET_CHIP_DBGLOG_WRITER_PTR_BUS_ADDR(chip_id)   (MEMX_DBGLOG_PCIE_LINK0_BASE + MEMX_DBGLOG_RWPTR_OFFSET + ((chip_id << 3)))
    #define MEMX_GET_CHIP_DBGLOG_READ_PTR_BUS_ADDR(chip_id)     (MEMX_DBGLOG_PCIE_LINK0_BASE + MEMX_DBGLOG_RWPTR_OFFSET + ((chip_id << 3)) + 4)
    #define MEMX_GET_CHIP_RMTCMD_COMMAND_BUS_ADDR(chip_id)      (MEMX_DBGLOG_PCIE_LINK0_BASE + MEMX_RMTCMD_CONTROLBASE_OFFSET + ((chip_id << 3)))
    #define MEMX_GET_CHIP_RMTCMD_PARAM_BUS_ADDR(chip_id)        (MEMX_DBGLOG_PCIE_LINK0_BASE + MEMX_RMTCMD_CONTROLBASE_OFFSET + ((chip_id << 3)) + 4)
    #define MEMX_GET_CHIP_RMTCMD_PARAM2_BUS_ADDR(chip_id)       (MEMX_DBGLOG_PCIE_LINK0_BASE + MEMX_RMTCMD_PARAM2BASE_OFFSET + ((chip_id << 4)) + 0)
    #define MEMX_GET_DVFS_UTIL_BUS_ADDR                         (MEMX_DBGLOG_PCIE_LINK0_BASE + MEMX_DVFS_UTILBASE_OFFSET)

#define MEMX_DGBLOG_RPTR_DEFAULT       (0x26F28)
#define MEMX_DGBLOG_WPTR_DEFAULT       (0x26F24)
#define MEMX_DGBLOG_SIZE_DEFAULT       (0x1000)
#define MEMX_DGBLOG_ADDRESS_DEFAULT    (0x25C00)
#define MEMX_RMTCMD_COMMAND_DEFAULT    (0x40046F44)
#define MEMX_RMTCMD_PARAMTER_DEFAULT   (0x40046F48)

#define MEMX_DBGLOG_CONTROL_BASE            (0x40046F00)
    #define MEMX_FW_GIT_VERSION                (0x08)
    #define MEMX_FW_DATE_CODE                  (0x0C)
    #define MEMX_DBGLOG_CTRL_BUFFERADDR_OFS    (0x1C)
    #define MEMX_DBGLOG_CTRL_BUFFERSIZE_OFS    (0x20)
    #define MEMX_DBGLOG_CTRL_WPTRADDR_OFS      (0x2C)
    #define MEMX_DBGLOG_CTRL_RPTRADDR_OFS      (0x30)
    #define MEMX_DBGLOG_CTRL_ENABLE_OFS        (0x3C)
    #define MEMX_RMTCMD_CMDADDR_OFS            (0x4C)
    #define MEMX_RMTCMD_PARAMADDR_OFS          (0x50)
    #define MEMX_DVFS_MPU_UTI_ADDR             (0x64)
    #define MEMX_FW_ANTIROLLBACK_INFO          (0x70)
    #define MEMX_RMTCMD_PARAM2ADDR_OFS         (0x74)
#define PHYSICAL_FW_COMMAND_SRAM_BASE       (0x40046E00)
#define PHYSICAL_MPU_PVTS_BASE              (0x40046D00)
#define MXCNST_BOOT_MODE                    (0x20000100)
#define MXCNST_CHIP_VERSION                 (0x20000500)
#define MXCNST_IGR_BUF_WPTR                 (0x40046F88)

#define PCIE_EP0_LM_LOCAL_ERROR_STATUS      (0x2110020C)

#define MEMX_FW_IMGFMT_OFFSET   (0x6F08)
#define MEMX_FW_IMGSIZE_OFFSET  (0x7000)
#define MEMX_FW_IMG_OFFSET      (0x7004)
#define MEMX_FW_IMGSIZE_LEN     (4)
#define MEMX_FW_IMGCRC_LEN      (4)
#define MEMX_IMG_TOTAL_SIZE_LEN (4)
#define MEMX_FSBL_SECTION_SIZE  (DEF_KB(28))


#define BYTE_ROUND_DOWN(x, unit)        ( (x) & (~(unit-1)) )
#define BYTE_ROUND_UP(x, unit)          ( ((x) + unit-1)  & (~(unit-1)) )

#define GLOBAL_FIRMWARE_DEFUALT_PATH    (L"\\SystemRoot\\System32\\drivers\\cascade.bin")

#define QUEUE_SIZE (MAX_SUPPORT_CHIP_NUM + 1)

#define TO_MPU (0)
#define TO_CONFIG_OUTPUT (1)

#define MXCNST_RP_XFLOW_ADDR (0x60000000)

#define DEVICE_IRQ_COUNT ((RESET_MPU) - (RESERVE_ID2) + 1)
#define MEMX_PCIE_IRQ_OFFSET(C, I) ((((15 - (C)) * DEVICE_IRQ_COUNT) + (I)) * 4 + 0x200)


typedef enum MSIxIdx {
  Firmware_MSIx_Acknowledge_Notification    = 0,
  Chip_00_Egress_Dcore_Done_Notification    = 1,
  Chip_00_Ingress_Dcore_Done_Notification   = 2,
  Chip_01_Egress_Dcore_Done_Notification    = 3,
  Chip_01_Ingress_Dcore_Done_Notification   = 4,
  Chip_02_Egress_Dcore_Done_Notification    = 5,
  Chip_02_Ingress_Dcore_Done_Notification   = 6,
  Chip_03_Egress_Dcore_Done_Notification    = 7,
  Chip_03_Ingress_Dcore_Done_Notification   = 8,
  Chip_04_Egress_Dcore_Done_Notification    = 9,
  Chip_04_Ingress_Dcore_Done_Notification   = 10,
  Chip_05_Egress_Dcore_Done_Notification    = 11,
  Chip_05_Ingress_Dcore_Done_Notification   = 12,
  Chip_06_Egress_Dcore_Done_Notification    = 13,
  Chip_06_Ingress_Dcore_Done_Notification   = 14,
  Chip_07_Egress_Dcore_Done_Notification    = 15,
  Chip_07_Ingress_Dcore_Done_Notification   = 16,
  Chip_08_Egress_Dcore_Done_Notification    = 17,
  Chip_08_Ingress_Dcore_Done_Notification   = 18,
  Chip_09_Egress_Dcore_Done_Notification    = 19,
  Chip_09_Ingress_Dcore_Done_Notification   = 20,
  Chip_10_Egress_Dcore_Done_Notification    = 21,
  Chip_10_Ingress_Dcore_Done_Notification   = 22,
  Chip_11_Egress_Dcore_Done_Notification    = 23,
  Chip_11_Ingress_Dcore_Done_Notification   = 24,
  Chip_12_Egress_Dcore_Done_Notification    = 25,
  Chip_12_Ingress_Dcore_Done_Notification   = 26,
  Chip_13_Egress_Dcore_Done_Notification    = 27,
  Chip_13_Ingress_Dcore_Done_Notification   = 28,
  Chip_14_Egress_Dcore_Done_Notification    = 29,
  Chip_14_Ingress_Dcore_Done_Notification   = 30,
  Chip_15_Egress_Dcore_Done_Notification    = 31,
  Chip_15_Ingress_Dcore_Done_Notification   = 32,
  NUM_OF_MSIX_USED                          = 33
} MSIxIdx_t;

typedef enum _BAR_ID {
    BAR0 = 0,
    BAR1,
    BAR2,
    BAR3,
    BAR4,
    BAR5,
    MAX_BAR
} BAR_ID;

typedef struct _BAR_CONTEXT {
    ULONGLONG        BaseAddress;
    PVOID            MappingAddress; //MmMapIoSpaceEx(BaseAddress, Length)
    ULONG            Length;
    ULONG            Flag;
} BAR_CONTEXT, * PBAR_CONTEXT;

typedef struct _BAR_CTRLINFO {
    ULONG            XflowVbufOffset;
    ULONG            XflowConfOffset;
    UCHAR            BarMode;
    UCHAR            SramIdx;
    UCHAR            XflowVbufIdx;
    UCHAR            XflowConfIdx;
    UCHAR            DeviceIrqIdx;
} BAR_CTRLINFO, * PBAR_CTRLINFO;

typedef enum _deviceState {
    DEVICE_INITIAL = 0,
    DEVICE_FWDONE  = 1,
    DEVICE_RUNNING = 2,
    MAX_STATE
} MEMXDEVICESTATE;

typedef enum __deviceBuffer {
    BUFFER_ID_START   = 0,
    BUFFER_IDX_READ   = BUFFER_ID_START,
    BUFFER_IDX_WRITE  = 1,
    BUFFER_IDX_DEBUG0 = 2,
    BUFFER_IDX_DEBUG1 = 3,
    MAX_NUM_COMBUFFER
} MEMXDEVICEBUFFER;

typedef enum __deviceIoQ {
    QUEUE_IDX_READ   = 0,
    QUEUE_IDX_WRITE  = 1,
    QUEUE_IDX_IOCTL  = 2,
    QUEUE_MAX_NUM    = 3
} MEMXIOQUEUE;

struct _DEVICE_CONTEXT;
struct _INT_CONTEXT;
typedef NTSTATUS (*MSIX_HANDLER)(struct _DEVICE_CONTEXT *pDevContext, struct _INT_CONTEXT *pIntContext);

typedef struct {
    ULONG        chip_id;
    ULONG        des_addr_base;
    void*        src_buf_base;
    ULONG        total_size;
    ULONG        written_size;
} WmemEntry, * pWmemEntry;

typedef struct {
    pWmemEntry    pContext;
    USHORT        uwMaxEntryCnt;
    USHORT        uwValidEntryCnt;
    USHORT        uwCurEntryIdx;
} DevWmemContext, * pDevWmemContext;

//
// The device context performs the same job as
// a WDM device extension in the driver frameworks
//
typedef struct _DEVICE_CONTEXT {
    WDFDEVICE           WdfDevice;
    WDFQUEUE            IoctlQueue;
    WDFQUEUE            WriteQueue;
    WDFQUEUE            ReadQueue;
    WDFQUEUE            IoctlPendingQueue;
    WDFQUEUE            WritePendingQueue;
    WDFQUEUE            ReadPendingQueue;
    BAR_CONTEXT         Bar[MAX_BAR];
    BAR_CTRLINFO        BarInfo;
    WDFINTERRUPT        WdfInterrupt[MAX_MSIX];
    ULONG               MsixCount;
    WDFSPINLOCK         IoQueueLockHandle[QUEUE_MAX_NUM];
    WDFSPINLOCK         XflowLockHandle[MAX_SUPPORT_CHIP_NUM];
    WDFSPINLOCK         device_lock;
    ULONG               deviceState;
    UCHAR               dfpImageInfo[MAX_SUPPORT_CHIP_NUM];
    ULONG               dfpRgcfgPmuConfig[MAX_SUPPORT_CHIP_NUM];
    WDFDMAENABLER       DmaEnabler;
    volatile PVOID      mmap_fw_command_buffer_base;
    struct hw_info      hw_info;
    // Read
    ULONG               IndicatorQueue[QUEUE_SIZE];
    ULONG               IndicatorHeader;
    ULONG               IndicatorTail;
    ULONG               BufMappingIdx;
    WDFCOMMONBUFFER     CommonBuffer[MAX_NUM_COMBUFFER];
    PUCHAR              CommonBufferBaseDriver[MAX_NUM_COMBUFFER];
    PHYSICAL_ADDRESS    CommonBufferBaseDevice[MAX_NUM_COMBUFFER];
    DevWmemContext      download_wmem_context[MAX_SUPPORT_CHIP_NUM];
    BUS_INTERFACE_STANDARD busInterface;
    UCHAR               is_read_abort;
    UCHAR               reference_count;
} DEVICE_CONTEXT, * PDEVICE_CONTEXT;

typedef struct _FILE_CONTEXT {
    WDFDEVICE   WdfDevice;
} FILE_CONTEXT, *PFILE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(FILE_CONTEXT, GetFileContext)

#define MEMX_PCIE_VENDOR_ID (0x1FE9)
#define MEMX_PCIE_DEVICE_ID (0x0100)

/*
 *                      0x0  +--------------------------+
 *                           |  512KB RX DMA Buf(ofmap) |
 *                  0x80000  +--------------------------+
 *                           |  512KB TX DMA Buf(ifmap) |
 *                 0x100000  +--------------------------+
 *                           |  1MB Debug log Buf(fwlog)|
 *                 0x200000  +--------------------------+
*/
#define MEMX_PCIE_DMA_COHERENT_BUFFER_SIZE_1MB              (0x100000)
#define MEMX_PCIE_OFMAP_DMA_COHERENT_BUFFER_SIZE_512KB      (0x80000)
#define MEMX_PCIE_IFMAP_DMA_COHERENT_BUFFER_SIZE_512KB      (0x80000)
#define MEMX_PCIE_FWLOG_DMA_COHERENT_BUFFER_SIZE_1MB        (0x100000)

#define DMA_COHERENT_BUFFER_SIZE_1MB                (MEMX_PCIE_OFMAP_DMA_COHERENT_BUFFER_SIZE_512KB + MEMX_PCIE_IFMAP_DMA_COHERENT_BUFFER_SIZE_512KB)
#define DMA_COHERENT_BUFFER_SIZE_2MB                (MEMX_PCIE_DMA_COHERENT_BUFFER_SIZE_1MB + MEMX_PCIE_FWLOG_DMA_COHERENT_BUFFER_SIZE_1MB)

#define XFLOW_BASE_ADDRESS_REGISTER_OFFSET      (0x0)
#define XFLOW_CONTROL_REGISTER_OFFSET           (0x4)

#define XFLOW_CONFIG_REG_PREFIX                 (0xC000000)
#define XFLOW_VIRTUAL_BUFFER_PREFIX             (0x8000000)

#define XFLOW_CHIP_ID_MASK                      (0xF)
#define XFLOW_CHIP_ID_SHIFT                     (22)

#define GET_XFLOW_OFFSET(chip_id, xflow_prefix) ((xflow_prefix) | (((chip_id) & XFLOW_CHIP_ID_MASK) << XFLOW_CHIP_ID_SHIFT))

typedef enum {
    TYPE_PCIE_SINGLE_DEVICE = 0,
    TYPE_PCIE_OPTIONAL_FIRST_DEVICE,
    TYPE_PCIE_OPTIONAL_LAST_DEVICE,
    TYPE_INVALID_DEVICE = 0xFFFFFFFF,
} memx_device_types_t;

typedef enum {
    CHIP_ID0 = 0,
    CHIP_ID1,
    CHIP_ID2,
    CHIP_ID3,
    CHIP_ID4,
    CHIP_ID5,
    CHIP_ID6,
    CHIP_ID7,
    CHIP_ID8,
    CHIP_ID9,
    CHIP_ID10,
    CHIP_ID11,
    CHIP_ID12,
    CHIP_ID13,
    CHIP_ID14,
    CHIP_ID15,
    CHIP_ID_MAX
} memx_chip_ids_t;

typedef enum {
    MEMX_GROUP_0 = 0,
    MEMX_GROUP_1,
    MEMX_GROUP_2,
    MEMX_GROUP_3,
    MEMX_GROUP_4,
    MEMX_GROUP_5,
    MEMX_GROUP_6,
    MEMX_GROUP_7,
    MEMX_GROUP_MAX
} memx_group_ids_t;

#define FIRMWARE_CMD_DATA_DWORD_COUNT (63)

typedef enum XFLOW_MPU_SWIRQ_ID {
    MPU_EGRESS_DCORE_DONE = 0, //  should only be used in our fw.
    DUMP_XFLOW_ERRSTS,         //  only for debug fw xflow purpose.
    RESERVE_ID2,
    RESERVE_ID3,
    FW_CMD_DONE,
    MOVE_SRAMDATA_TO_DIPORT,
    INIT_WTMEM_FMEM,
    RESET_MPU,
    MPU_SWIRQ_ID_MAX
}XFLOW_MPU_SWIRQ_ID_T;

// Note: The whole fw cmd format is 256 bytes.
typedef struct pcie_fw_cmd_format {
    USHORT firmware_command;
    USHORT expected_data_length;
    ULONG data[FIRMWARE_CMD_DATA_DWORD_COUNT]; // for now, data area almost writed by mpu pcie firmware.
} pcie_fw_cmd_format_t;

//
// This macro will generate an inline function called DeviceGetContext
// which will be used to get a pointer to the device context memory
// in a type safe manner.
//
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)

//
// This is the context that can be placed per Queue
//
typedef struct _QUEUE_CONTEXT {
    ULONG   data;
} QUEUE_CONTEXT, * PQUEUE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(QUEUE_CONTEXT, GetQueueContext)

//
// This is the context that can be placed per INT
//
typedef struct _INT_CONTEXT {
    ULONG   msixIndex;
    ULONG   flag;
} INT_CONTEXT, * PINT_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(INT_CONTEXT, GetINTContext)

/*!
    @brief Xflow Write Data

    @param[in]      devContext          The pointer to Device context
    @param[in]      chip_id             The operation Chip Index
    @param[in]      base_addr           The Target Base Address
    @param[in]      base_addr_offset    The offset from base address
    @param[in]      value               Write input value
    @param[in]      access_mpu          TO_MPU / TO_CONFIG_OUTPUT
    @return         NTSTATUS            STATUS_SUCCESS if successful
*/
NTSTATUS MemxUtilXflowWrite(PDEVICE_CONTEXT devContext, UCHAR chip_id, ULONG base_addr, ULONG base_addr_offset, ULONG value, BOOLEAN access_mpu);

/*!
    @brief Xflow Burst Write WTMEM

    @param[in]      devContext          The pointer to Device context
    @param[in]      chip_id             The operation Chip Index
    @param[in]      base_addr           The Target Base Address
    @param[in]      data                Write input data buffer
    @param[in]      write_size          Write input data size
    @param[in]      access_mpu          TO_MPU / TO_CONFIG_OUTPUT
    @return         NTSTATUS            STATUS_SUCCESS if successful
*/
NTSTATUS MemxUtilXflowBurstWriteWTMEM(PDEVICE_CONTEXT devContext, UCHAR chip_id, ULONG base_addr, UCHAR* data, ULONG write_size, BOOLEAN access_mpu);

/*!
    @brief Xflow Read Virtual Buffer

    @param[in]      devContext          The pointer to Device context
    @param[in]      chip_id             The operation Chip Index
    @param[in]      base_addr           The Target Base Address
    @param[in]      base_addr_offset    The offset from base address
    @param[in]      access_mpu          TO_MPU / TO_CONFIG_OUTPUT
    @return         ULONG               Read vaule result
*/
ULONG MemxUtilXflowRead(PDEVICE_CONTEXT devContext, UCHAR chip_id, ULONG base_addr, ULONG base_addr_offset, BOOLEAN access_mpu);

/*!
    @brief Xflow Send Fw command to device

    @param[in]      devContext                  The pointer to Device context
    @param[in]      op_code                     The OpCode
    @param[in]      expected_payload_length     The FW Payload size
    @param[in]      chip_id                     The operation Chip Index
    @return         NTSTATUS                   STATUS_SUCCESS if successful
*/
NTSTATUS MemxUtilSendFwCommand(PDEVICE_CONTEXT devContext, USHORT op_code, USHORT expected_payload_length, UCHAR chip_id);

/*!
    @brief Read 1st chip Sram

    @param[in]      devContext          The pointer to Device context
    @param[in]      axi_base_addr       The Target Base Address

    @return         ULONG               Read vaule result, 0xFFFFFFFF may means failed
*/
ULONG MemxUtilSramRead(PDEVICE_CONTEXT devContext, ULONG axi_base_addr);

/*!
    @brief Write 1st Chip Sram

    @param[in]      devContext          The pointer to Device context
    @param[in]      axi_base_addr       The Target Base Address
*/
void MemxUtilSramWrite(PDEVICE_CONTEXT devContext, ULONG axi_base_addr, ULONG value);

/*!
    @brief Xflow Download firmware

    @param[in]      devContext          The pointer to Device context
    @return         NTSTATUS            STATUS_SUCCESS if successful
*/
NTSTATUS MemxUtilXflowDownloadFirmware(PDEVICE_CONTEXT devContext);

NTSTATUS MemxPcieTriggerDeviceIrq(PDEVICE_CONTEXT devContext, XFLOW_MPU_SWIRQ_ID_T sw_irq_idx, UCHAR chip_id);

EXTERN_C_END
