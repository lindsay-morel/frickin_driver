package com.memryx.memxdriver;

public class MemxHeader {

    /**
     * @brief Driver internal error code. '0' should always be used to indicate no
     * error occurs.
     */
    public static final int MEMX_STATUS_OK = 0;
    public static final int MEMX_STATUS_OTHERS = 1;

    public enum MemxGetFeatureOpcode {
        OPCODE_GET_MANUFACTURERID(0),
        OPCODE_GET_FW_COMMIT(1),
        OPCODE_GET_DATE_CODE(2),
        OPCODE_GET_COLD_WARM_REBOOT_COUNT(3),
        OPCODE_GET_WARM_REBOOT_COUNT(4),
        OPCODE_GET_KDRIVER_VERSION(5),
        OPCODE_GET_TEMPERATURE(6),
        OPCODE_GET_THERMAL_STATE(7),
        OPCODE_GET_THERMAL_THRESHOLD(8),
        OPCODE_GET_FREQUENCY(9),
        OPCODE_GET_VOLTAGE(10),
        OPCODE_GET_THROUGHPUT(11),
        OPCODE_GET_POWER(12),
        OPCODE_GET_POWERMANAGEMENT(13),
        OPCODE_GET_POWER_ALERT(14),
        OPCODE_GET_MODULE_INFORMATION(15),
        OPCODE_GET_FEATURE_MAX(16);

        private final int value;

        MemxGetFeatureOpcode(int value) {
            this.value = value;
        }

        public int getValue() {
            return value;
        }
    }

    public enum MemxSetFeatureOpcode {
        OPCODE_SET_THERMAL_THRESHOLD(0),
        OPCODE_SET_FREQUENCY(1),
        OPCODE_SET_VOLTAGE(2),
        OPCODE_SET_POWERMANAGEMENT(3),
        OPCODE_SET_POWER_THRESHOLD(4),
        OPCODE_SET_POWER_ALERT_FREQUENCY(5),
        OPCODE_SET_FEATURE_MAX(6);

        private final int value;

        MemxSetFeatureOpcode(int value) {
            this.value = value;
        }

        public int getValue() {
            return value;
        }
    }

    public enum MemxPowerState {
        MEMX_PS0,  // Operational power state (I/O Support)
        MEMX_PS1,  // Operational power state (I/O Support)
        MEMX_PS2,  // Non-operational power state (I/O not Support)
        MEMX_PS3,  // Non-operational power state (I/O not Support)
        MEMX_PS4,  // Non-operational power state (I/O not Support, xFlow not Support, device based)
        MAX_POWER_STATE;
    }

    public enum MxmxBootMode {
        MXMX_BOOT_MODE_QSPI(0),
        MXMX_BOOT_MODE_USB(1),
        MXMX_BOOT_MODE_PCIE(2),
        MXMX_BOOT_MODE_UART(3),
        MXMX_BOOT_MODE_MAX(4);

        private final int value;

        MxmxBootMode(int value) {
            this.value = value;
        }

        public int getValue() {
            return value;
        }
    }

    public enum MxmxChipVersion {
        MXMX_CHIP_VERSION_A0(0),
        MXMX_CHIP_VERSION_A1(5),
        MXMX_CHIP_VERSION_MAX(6);

        private final int value;

        MxmxChipVersion(int value) {
            this.value = value;
        }

        public int getValue() {
            return value;
        }
    }

    /**
     * @brief MemryX device MX3:Cascade+. Constant value which should be referenced
     * only and not be modified manually.
     */
    public static final int MEMX_DEVICE_CASCADE_PLUS = 31;

    /**
     * @brief Maximum number of model contexts can be stored within driver. Constant
     * value which should be referenced only and not be modified manually.
     */
    public static final int MEMX_MODEL_MAX_NUMBER = 32;

    /**
     * @brief Maximum number of MPU device group contexts can be stored within
     * driver. Constant value which should be referenced only and not be modified
     * manually.
     */
    public static final int MEMX_DEVICE_GROUP_MAX_NUMBER = 4;

    /**
     * @brief Option to configure model input or output format to 32-bits floating point.
     * By default, input and output feature map should be configured using this option
     * if model input and output are using floating-point.
     */
    public static final int MEMX_FMAP_FORMAT_FLOAT32 = 5;

    /**
     * @brief Option to configure model input or output format to bfloat16.
     * By default, input and output feature map should be configured using this option
     * if model input and output are using floating-point.
     */
    public static final int MEMX_FMAP_FORMAT_BF16 = 4;

    /**
     * @brief Option to configure model input or output format to raw byte array.
     * By default, input and output feature map should be configured using this option
     * if model input and output are using floating-point.
     */
    public static final int MEMX_FMAP_FORMAT_RAW = 2;

    /**
     * @brief Option to configure model input or output feature map format to
     * MemryX proprietary format group-bfloat-80.
     */
    public static final int MEMX_FMAP_FORMAT_GBF80 = 0;
    public static final int MEMX_FMAP_FORMAT_GBF80_ROW_PAD = 6;

    public static final int MEMX_DOWNLOAD_TYPE_FROM_FILE = 0;
    public static final int MEMX_DOWNLOAD_TYPE_FROM_BUFFER = 1 << 7; // Equivalent to 128

    /**
     * @brief Option of `memx_download_model()` to download weight memory only to
     * device. Can be used together with `MEMX_DOWNLOAD_TYPE_MODEL`.
     */
    public static final int MEMX_DOWNLOAD_TYPE_WTMEM = 1;

    /**
     * @brief Option of `memx_download_model()` to download model only to device.
     * Can be used together with `MEMX_DOWNLOAD_TYPE_WTMEM`.
     */
    public static final int MEMX_DOWNLOAD_TYPE_MODEL = 2;

    /**
     * @brief Option of `memx_download_model()` to download both weight memory and
     * model to device. The same effect as using `MEMX_DOWNLOAD_TYPE_WTMEM` and
     * `MEMX_DOWNLOAD_TYPE_MODEL` together.
     */
    public static final int MEMX_DOWNLOAD_TYPE_WTMEM_AND_MODEL = 3;

    /**
     * @brief Option of `memx_download_model()`.The same effect as using
     * `MEMX_DOWNLOAD_TYPE_WTMEM_AND_MODEL`, but using buffer pointer.
     */
    public static final int MEMX_DOWNLOAD_TYPE_WTMEM_AND_MODEL_BUFFER = MEMX_DOWNLOAD_TYPE_FROM_BUFFER | MEMX_DOWNLOAD_TYPE_WTMEM_AND_MODEL; // 128 | 3 = 131

    /**
     * @brief Option of `memx_config_mpu_group()` to set different MPU group
     */
    public static final int MEMX_MPU_GROUP_CONFIG_ONE_GROUP_FOUR_MPUS = 0;
    public static final int MEMX_MPU_GROUP_CONFIG_TWO_GROUP_TWO_MPUS = 1;
    public static final int MEMX_MPU_GROUP_CONFIG_ONE_GROUP_ONE_MPU = 2;
    public static final int MEMX_MPU_GROUP_CONFIG_ONE_GROUP_THREE_MPUS = 3;
    public static final int MEMX_MPU_GROUP_CONFIG_ONE_GROUP_TWO_MPUS = 4;
    public static final int MEMX_MPU_GROUP_CONFIG_ONE_GROUP_EIGHT_MPUS = 5;
    public static final int MEMX_MPU_GROUP_CONFIG_ONE_GROUP_TWELVE_MPUS = 6;
    public static final int MEMX_MPU_GROUP_CONFIG_ONE_GROUP_SIXTEEN_MPUS = 7;

    /***************************************************************************//**
     * command
     ******************************************************************************/
    /**
     * @brief All MPU and MPUIO command should be registered here to obtain a
     * global unique ID. New command added should be appended to tail. The reason
     * why MPU and MPUIO share the same enumeration is because that command through
     * MPU interface will possibly be forwarded to MPUIO once not recognized.
     */
    public enum MemxCommand {
        MEMX_CMD_READ_TOTAL_CHIP_COUNT(0),
        MEMX_CMD_GET_FW_DOWNLOAD_STATUS(1),
        MEMX_CMD_CONFIG_MPU_GROUP(2),
        MEMX_CMD_RESET_DEVICE(3),
        MEMX_CMD_MAX(4); // It's common to include MAX as the last enum member

        private final int value;

        MemxCommand(int value) {
            this.value = value;
        }

        public int getValue() {
            return value;
        }
    }
}
