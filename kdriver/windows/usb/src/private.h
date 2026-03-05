/**
 * @file public.h
 *
 * @brief This module contains the common declarations shared by driver and user applications.
 * @version 0.1
 * @date 2023
 *
 * @copyright Copyright (c) 2023 MemryX. Inc. All Rights Reserved.
 *
 */
#pragma warning(disable:4200)  //
#pragma warning(disable:4201)  // nameless struct/union
#pragma warning(disable:4214)  // bit field types other than int

#include <initguid.h>
#include <ntddk.h>
#include <ntintsafe.h>
#include "usbdi.h"
#include "usbdlib.h"
#include "trace.h"
#include <wdf.h>
#include <wdfusb.h>
#include "memx_ioctl.h"

#ifndef _H
#define _H

#define POOL_TAG (ULONG) 'SBSU'
#define GLOBAL_FIRMWARE_DEFUALT_PATH    (L"\\SystemRoot\\System32\\drivers\\cascade.bin")
#define DEFAULT_REGISTRY_TRANSFER_SIZE 65536

#define MEMX_IN_EP            0x81
#define MEMX_OUT_EP           0x01
#define MEMX_FW_IN_EP         0x82
#define MEMX_FW_OUT_EP        0x02
#define MAX_OPS_SIZE          (64*1024)
#define FWCFG_ID_CLR          0x952700
#define FWCFG_ID_FW           0x952701
#define FWCFG_ID_DFP          0x952702
#define FWCFG_ID_MPU_INSIZE   0x952703
#define FWCFG_ID_MPU_OUTSIZE  0x952704
#define FWCFG_ID_DFP_CHIPID   0x952705
#define FWCFG_ID_DFP_CFGSIZE  0x952706
#define FWCFG_ID_DFP_WMEMADR  0x952707
#define FWCFG_ID_DFP_WTMEMSZ  0x952708
#define FWCFG_ID_DFP_RESETMPU 0x952709
#define FWCFG_ID_DFP_RECFGMPU 0x95270A
#define FWCFG_ID_WREG         0x95270B
#define FWCFG_ID_RREG_ADR     0x95270C
#define FWCFG_ID_RREG         0x95270D
#define FWCFG_ID_MPU_GROUP    0x95270E
#define FWCFG_ID_RESET_DEVICE 0x95270F
#define FWCFG_ID_GET_FEATURE  0x952710
#define FWCFG_ID_SET_FEATURE  0x952711
#define FWCFG_ID_ADM_COMMAND  0x952712
#define DBGFS_ID_RDADDR       0x6D6581
#define USB_HANDSHAKE_ADDR    0x400FD200
#define DFP_FLASH_OFFSET      0x20000
#define MPU_CHIP_ID_BASE      (1)
#define MEMX_HEADER_SIZE      (64)
#define MAX_MPUIN_SIZE        (18000)
#define MAX_MPUOUT_SIZE       (54000)
#define FW_CFG_HEADER_SIZE    (8)
#define MAX_WT_CNT            (6)
#define DFP_CFG_SZ            (0xD0)
#define DFP_OFS(N)            (0x10 + (N) * 0xD0)
#define MAX_CFG_SZ            MAX_OPS_SIZE
#define SEP_NEXT_OFS          (4)
#define SEP_LEN_OFS           (12)
#define DEF_BYTE(x)           (x)
#define DEF_1KB               (1024UL)
#define DEF_1MB               (1024UL * DEF_1KB)
#define DEF_KB(x)             ((x)  * DEF_1KB)
#define DEF_MB(x)             ((x)  * DEF_1MB)

#define MEMX_FW_IMGFMT_OFFSET   (0x6F08)
#define MEMX_FW_IMGSIZE_OFFSET  (0x7000)
#define MEMX_FW_IMG_OFFSET      (0x7004)
#define MEMX_FW_IMGSIZE_LEN     (4)
#define MEMX_FW_IMGCRC_LEN      (4)
#define MEMX_IMG_TOTAL_SIZE_LEN (4)
#define MEMX_FSBL_SECTION_SIZE  (DEF_KB(28))

#define DEV_CODE_SRAM_MPUUTIL_BASE (0x40046d00)
#define MEMX_PCIE_CMD_BUF_STARTADDR (0x40046E00)

typedef struct _DEVICE_CONTEXT {
    USB_DEVICE_DESCRIPTOR           UsbDeviceDescriptor;
    PUSB_CONFIGURATION_DESCRIPTOR   UsbConfigurationDescriptor;
    WDFUSBDEVICE                    WdfUsbTargetDevice;
    ULONG                           WaitWakeEnable;
    BOOLEAN                         IsDeviceHighSpeed;
    BOOLEAN                         IsDeviceSuperSpeed;
    WDFUSBINTERFACE                 UsbInterface;
    UCHAR                           SelectedAlternateSetting;
    UCHAR                           NumberConfiguredPipes;
    ULONG                           MaximumTransferSize;

    WDFMEMORY                       FwUrbMemory;
    PURB                            FwUrb;
    WDFUSBPIPE                      BulkReadPipe;   // Pipe opened for the bulk IN endpoint.
    WDFUSBPIPE                      BulkWritePipe;  // Pipe opened for the bulk out endpoint.
    WDFUSBPIPE                      FwReadPipe;   // Pipe opened for the FW IN endpoint.
    WDFUSBPIPE                      FwWritePipe;  // Pipe opened for the FW out endpoint.
    KMUTEX                          ConfigMutex;
    WDFWAITLOCK                     read_lock;
    WDFSPINLOCK                     device_lock;
    ULONG                           flow_size[MEMX_TOTAL_FLOW_COUNT];
    ULONG                           buffer_size[MEMX_TOTAL_FLOW_COUNT];
    ULONG                           usb_first_chip_pipeline_flag;
    ULONG                           usb_last_chip_pingpong_flag;
    ULONG                           BulkWriteMaximumPacketSize;
    UCHAR                           flow_id;
    UCHAR                           is_read_abort;
    UCHAR                           reference_count;
    struct hw_info                  hw_info;

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)

typedef struct _FILE_CONTEXT {
    WDFUSBPIPE Pipe;
} FILE_CONTEXT, *PFILE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(FILE_CONTEXT, GetFileContext)

typedef struct _REQUEST_CONTEXT {
    WDFMEMORY         UrbMemory;
    PMDL              Mdl;
    ULONG             Length;         // remaining to xfer
    ULONG             Numxfer;
    ULONG_PTR         VirtualAddress; // va for next segment of xfer.
    BOOLEAN           Read;           // TRUE if Read
    BOOLEAN           ZLP;            // TRUE if need sending ZLP
} REQUEST_CONTEXT, * PREQUEST_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(REQUEST_CONTEXT, GetRequestContext)

DRIVER_INITIALIZE                   DriverEntry;
EVT_WDF_OBJECT_CONTEXT_CLEANUP      MemxEvtDriverContextCleanup;
EVT_WDF_DRIVER_DEVICE_ADD           MemxEvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE     MemxEvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE     MemxEvtDeviceReleaseHardware;
EVT_WDF_OBJECT_CONTEXT_CLEANUP      MemxEvtDeviceCleanup;

EVT_WDF_DEVICE_D0_ENTRY             MemxEvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT              MemxEvtDeviceD0Exit;
EVT_WDF_DEVICE_D0_ENTRY_POST_INTERRUPTS_ENABLED  MemxEvtPostInterruptEnable;

EVT_WDF_DEVICE_FILE_CREATE          MemxEvtDeviceFileCreate;
EVT_WDF_FILE_CLOSE                  MemxEvtDeviceFileClose;
EVT_WDF_IO_QUEUE_IO_READ            MemxEvtIoRead;
EVT_WDF_IO_QUEUE_IO_WRITE           MemxEvtIoWrite;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL  MemxEvtIoDeviceControl;
EVT_WDF_IO_QUEUE_IO_STOP            MemxEvtIoStop;
NTSTATUS memx_usb_init_hw_info(PDEVICE_CONTEXT DeviceContext);

#endif
