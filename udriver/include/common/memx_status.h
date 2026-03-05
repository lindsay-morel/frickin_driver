/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2024 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#ifndef MEMX_STATUS_H_
#define MEMX_STATUS_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _memx_status_type {
  cMemxTypeGeneric         = 0x0,
  cMemxTypeModuleSpecific  = 0x1,
  cMemxTypeCommandSpecific = 0x2,
  cMemxTypeMax             = 0x8
} memx_status_type_t;

typedef enum _memx_module_type {
  cMemxModuleGeneric       = 0x0,
  cMemxModuleModel         = 0x1,
  cMemxModuleDeviceManager = 0x2,
  cMemxModuleMpu           = 0x3,
  cMemxModuleMpuIO         = 0x4,
  cMemxModulePlatform      = 0x5,
  cMemxModuleMax           = 0x10
} memx_module_type_t;

typedef enum _memx_submodule_type {
  cMemxSubModuleGeneric         = 0x0,
  // MPU Module
  cMemxMemxSubModuleEncoder     = 0x1,
  cMemxMemxSubModuleDfpParser   = 0x2,

  cMemxubModuleMax              = 0x100
} memx_submodule_type_t;

#define MEMX_STATUS_TYPE_OFFSET       (28)
#define MEMX_STATUS_MODULE_OFFSET     (24)
#define MEMX_STATUS_SUBMODULE_OFFSET  (16)
#define MEMX_STATUS_CODE_OFFSET       (0)

#define MEMX_STATUS_TYPE_MASK      (0x7)
#define MEMX_STATUS_MODULE_MASK    (0xF)
#define MEMX_STATUS_SUBMODULE_MASK (0xFF)
#define MEMX_STATUS_CODE_MASK      (0xFFFF)

#define MEMX_SET_STATUS(Type, Module, SubModule, Code)      ((((Type) & MEMX_STATUS_TYPE_MASK) << MEMX_STATUS_TYPE_OFFSET) | \
                                                            (((Module) & MEMX_STATUS_MODULE_MASK) << MEMX_STATUS_MODULE_OFFSET) | \
                                                            (((SubModule) & MEMX_STATUS_SUBMODULE_MASK) << MEMX_STATUS_SUBMODULE_OFFSET) | \
                                                            (((Code) & MEMX_STATUS_CODE_MASK) << MEMX_STATUS_CODE_OFFSET))
#define MEMX_SET_GENERIC_STATUS(Code)                   (MEMX_SET_STATUS(cMemxTypeGeneric, cMemxModuleGeneric, cMemxSubModuleGeneric, Code))

#define MEMX_SET_COMMAND_SPECIFIC_STATUS(Module, SubModule, Code)  (MEMX_SET_STATUS(cMemxTypeCommandSpecific, Module, SubModule, Code))
#define MEMX_SET_MODULE_SPECIFIC_STATUS(Module, SubModule, Code)   (MEMX_SET_STATUS(cMemxTypeModuleSpecific, Module, SubModule, Code))
/***************************************************************************//**
 * helper macro
 ******************************************************************************/
#define memx_status_no_error(_status_) ((_status_) == MEMX_STATUS_OK)
#define memx_status_error(_status_)    ((_status_) != MEMX_STATUS_OK)

/**
 * @brief All driver modules' return status code definition.
 *
 */
typedef enum _memx_status {
  // Generic Status  0x00000000 - 0x0000FFFF
  MEMX_STATUS_OK                                = MEMX_SET_GENERIC_STATUS(0x0000), // Without Error or Successful
  MEMX_STATUS_OTHERS                            = MEMX_SET_GENERIC_STATUS(0x0001), // Generic Error or failed
  MEMX_STATUS_INVALID_PARAMETER                 = MEMX_SET_GENERIC_STATUS(0x0002), // A reserved coded value or an unsupported value in a defined field (other than the opcode field).
  MEMX_STATUS_INVALID_OPCODE                    = MEMX_SET_GENERIC_STATUS(0x0003), // A reserved coded value or an unsupported value in the command opcode field.
  MEMX_STATUS_IN_PROGRESS                       = MEMX_SET_GENERIC_STATUS(0x0004), // No Error but Request is in progress
  MEMX_STATUS_TIMEOUT                           = MEMX_SET_GENERIC_STATUS(0x0005), // Time out Error
  MEMX_STATUS_DEVICE_NOT_READY                  = MEMX_SET_GENERIC_STATUS(0x0006), // Device is not ready to operate
  MEMX_STATUS_OUT_OF_RESOURCE                   = MEMX_SET_GENERIC_STATUS(0x0007), // Driver Interal Resource exhausted
  MEMX_STATUS_OUT_OF_MEMORY                     = MEMX_SET_GENERIC_STATUS(0x0008), // Driver allocate memory failed
  MEMX_STATUS_MEMORY_NOT_ALIGNED                = MEMX_SET_GENERIC_STATUS(0x0009), // Memory address, size is not properly aligned
  MEMX_STATUS_NULL_POINTER                      = MEMX_SET_GENERIC_STATUS(0x000A), // Driver Internal Null pointer
  MEMX_STATUS_FILE_NOT_FOUND                    = MEMX_SET_GENERIC_STATUS(0x000B), // Accessed Missing File
  MEMX_STATUS_FILE_READ_ERROR                   = MEMX_SET_GENERIC_STATUS(0x000C), // Read Data from File error
  MEMX_STATUS_INTERNAL_ERROR                    = MEMX_SET_GENERIC_STATUS(0x000D), // Opeation failed due to driver interal flow

  // Module Specific Status  0x10000000 - 0x1FFFFFFF
  // Generic Module Specific Status 0x10000000 - 0x10FFFFFF

  // Model Module Specific Status 0x11000000 - 0x11FFFFFF
  MEMX_STATUS_MODEL_INVALID_ID                  = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleModel, cMemxSubModuleGeneric, 0x0000),
  MEMX_STATUS_MODEL_NOT_OPEN                    = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleModel, cMemxSubModuleGeneric, 0x0001),
  MEMX_STATUS_MODEL_NOT_CONFIG                  = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleModel, cMemxSubModuleGeneric, 0x0002),
  MEMX_STATUS_MODEL_IN_USE                      = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleModel, cMemxSubModuleGeneric, 0x0003),

  // Device Manager Module Specific Status  0x12000000 - 0x12FFFFFF
  MEMX_STATUS_DEVICE_INVALID_ID                 = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleDeviceManager, cMemxSubModuleGeneric, 0x0000),
  MEMX_STATUS_DEVICE_OPEN_FAIL                  = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleDeviceManager, cMemxSubModuleGeneric, 0x0001),
  MEMX_STATUS_DEVICE_IN_USE                     = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleDeviceManager, cMemxSubModuleGeneric, 0x0002),
  MEMX_STATUS_DEVICE_NOT_OPENED                 = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleDeviceManager, cMemxSubModuleGeneric, 0x0003),
  MEMX_STATUS_DEVICE_LOCK_FAIL                  = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleDeviceManager, cMemxSubModuleGeneric, 0x0004),

  //MPU Module Specific Status 0x13000000 - 0x13FFFFFF
  MEMX_STATUS_MPU_OPEN_FAIL                     = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0000),
  MEMX_STATUS_MPU_INVALID_CHIP_GEN              = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0001),
  MEMX_STATUS_MPU_INVALID_CONTEXT               = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0002),
  MEMX_STATUS_MPU_INVALID_IO_CONTEXT            = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0003),
  MEMX_STATUS_MPU_NOT_IMPLEMENTED               = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0004),
  MEMX_STATUS_MPU_INVALID_PARAMETER             = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0005),
  MEMX_STATUS_MPU_IFMAP_NOT_CONFIG              = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0006),
  MEMX_STATUS_MPU_OFMAP_NOT_CONFIG              = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0007),
  MEMX_STATUS_MPU_SET_IFMAP_SIZE_FAIL           = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0008),
  MEMX_STATUS_MPU_SET_IFMAP_RANGE_CONVERT_FAIL  = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0009),
  MEMX_STATUS_MPU_SET_OFMAP_SIZE_FAIL           = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x000A),
  MEMX_STATUS_MPU_SET_OFMAP_HPOC_FAIL           = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x000B),
  MEMX_STATUS_MPU_GET_IFMAP_SIZE_FAIL           = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x000C),
  MEMX_STATUS_MPU_GET_IFMAP_RANGE_CONVERT_FAIL  = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x000D),
  MEMX_STATUS_MPU_GET_OFMAP_HPOC_FAIL           = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x000E),
  MEMX_STATUS_MPU_DOWNLOAD_MODEL_FAIL           = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x000F),
  MEMX_STATUS_MPU_DOWNLOAD_FIRMWARE_FAIL        = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0010),
  MEMX_STATUS_MPU_UPDATE_FMAP_FAIL              = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0011),
  MEMX_STATUS_MPU_INVALID_BUF_SIZE              = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0012),
  MEMX_STATUS_MPU_INVALID_DATA                  = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0013),
  MEMX_STATUS_MPU_INVALID_DATALEN               = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0014),
  MEMX_STATUS_MPU_INVALID_FLOW_ID               = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0015),
  MEMX_STATUS_MPU_INVALID_FMAP_SIZE             = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0016),
  MEMX_STATUS_MPU_INVALID_FMAP_FORMAT           = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0017),
  MEMX_STATUS_MPU_ALLOCATE_IFMAP_BUFFER_FAIL    = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0018),
  MEMX_STATUS_MPU_JOB_CREATE_FAIL               = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0019),
  MEMX_STATUS_MPU_JOB_ENQUEUE_FAIL              = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x001A),
  MEMX_STATUS_MPU_OFMAP_ALLOCATE_FAIL           = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x001B),
  MEMX_STATUS_MPU_OFMAP_ENQUEUE_FAIL            = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x001C),
  MEMX_STATUS_MPU_OFMAP_DEQUEUE_TIMEOUT         = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x001D),
  MEMX_STATUS_MPU_OFMAP_DEQUEUE_FAIL            = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x001E),
  MEMX_STATUS_MPU_IFMAP_ALLOCATE_FAIL           = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x001F),
  MEMX_STATUS_MPU_IFMAP_DEQUEUE_FAIL            = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0020),
  MEMX_STATUS_MPU_IFMAP_ENQUEUE_FAIL            = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0021),
  MEMX_STATUS_MPU_IFMAP_ENQUEUE_TIMEOUT         = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0022),
  MEMX_STATUS_MPU_INVALID_WORKER_NUMBER         = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0023),
  MEMX_STATUS_MPU_INVALID_QUEUE_SIZE            = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0024),
  MEMX_STATUS_MPU_WORKER_CREATE_FAIL            = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0025),
  MEMX_STATUS_MPU_HPOC_CONFIG_FAIL              = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0026),
  MEMX_STATUS_MPU_IFMAP_BUF_DEQUEUE_TIMEOUT     = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0027),
  MEMX_STATUS_MPU_OFMAP_BUF_DEQUEUE_TIMEOUT     = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0028),
  MEMX_STATUS_MPU_IFMAP_BUF_DEQUEUE_ERROR       = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxSubModuleGeneric, 0x0029),

  //MPU SubModule Encoder Specific Status 0x13010000 - 0x1301FFFF
  MEMX_STATUS_GBF_INVALID_DATA                  = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxMemxSubModuleEncoder, 0x0000),
  MEMX_STATUS_GBF_INVALID_CHANNEL_NUMBER        = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxMemxSubModuleEncoder, 0x0001),
  MEMX_STATUS_GBF_INVALID_WIDTH                 = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxMemxSubModuleEncoder, 0x0002),
  MEMX_STATUS_GBF_INVALID_HEIGHT                = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxMemxSubModuleEncoder, 0x0003),
  MEMX_STATUS_GBF_INVALID_LENGTH                = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxMemxSubModuleEncoder, 0x0004),
  MEMX_STATUS_GBF_INVALID_Z                     = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxMemxSubModuleEncoder, 0x0005),

  //MPU SubModule DFP Parser Specific Status 0x13020000 - 0x1302FFFF
  MEMX_STATUS_DFP_INVALID_FILE                  = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxMemxSubModuleDfpParser, 0x0000),
  MEMX_STATUS_DFP_READ_ERROR                    = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxMemxSubModuleDfpParser, 0x0001),
  MEMX_STATUS_DFP_INVALID_PARAMETER             = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxMemxSubModuleDfpParser, 0x0002),
  MEMX_STATUS_DFP_VERSION_TOO_OLD               = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxMemxSubModuleDfpParser, 0x0003),
  MEMX_STATUS_DFP_NOT_ENOUGH_MEMORY             = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpu, cMemxMemxSubModuleDfpParser, 0x0004),

  //MPUIO Module Specific Status 0x14000000 - 0x14FFFFFF
  MEMX_STATUS_MPUIO_OPEN_FAIL                 = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpuIO, cMemxSubModuleGeneric, 0x0000),
  MEMX_STATUS_MPUIO_INVALID_CONTEXT           = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpuIO, cMemxSubModuleGeneric, 0x0001),
  MEMX_STATUS_MPUIO_INVALID_CHIP_GEN          = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpuIO, cMemxSubModuleGeneric, 0x0002),
  MEMX_STATUS_MPUIO_INSUFFICENT_CHIP          = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpuIO, cMemxSubModuleGeneric, 0x0003),
  MEMX_STATUS_MPUIO_FUNCTION_NOT_IMPLEMENTED  = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpuIO, cMemxSubModuleGeneric, 0x0004),
  MEMX_STATUS_MPUIO_INVALID_DATA              = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpuIO, cMemxSubModuleGeneric, 0x0005),
  MEMX_STATUS_MPUIO_INVALID_DATALEN           = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpuIO, cMemxSubModuleGeneric, 0x0006),
  MEMX_STATUS_MPUIO_INVALID_FLOW_ID           = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModuleMpuIO, cMemxSubModuleGeneric, 0x0007),

  //Platform Module Specific Status 0x15000000 - 0x15FFFFFF
  MEMX_STATUS_PLATFORM_WRITE_FAIL             = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModulePlatform, cMemxSubModuleGeneric, 0x0000),
  MEMX_STATUS_PLATFORM_READ_FAIL              = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModulePlatform, cMemxSubModuleGeneric, 0x0001),
  MEMX_STATUS_PLATFORM_IOCTL_FAIL             = MEMX_SET_MODULE_SPECIFIC_STATUS(cMemxModulePlatform, cMemxSubModuleGeneric, 0x0002),


  //Command Specific Status  0x20000000 - 0x2FFFFFFF

} memx_status;


#ifdef __cplusplus
}
#endif

#endif /* MEMX_STATUS_H_ */
