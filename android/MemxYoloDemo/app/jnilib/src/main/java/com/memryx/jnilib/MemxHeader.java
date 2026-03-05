package com.memryx.jnilib;

import java.nio.ByteBuffer;

public class MemxHeader {

    // Disable instantiation
    private MemxHeader() {}

    // --- Status ---
    public static final int MEMX_STATUS_OK = 0;
    public static final int MEMX_STATUS_OTHERS = 1;

    // --- Feature Opcodes ---
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
        OPCODE_GET_FEATURE_MAX(16); // Assuming max is the next value

        private final int value;
        MemxGetFeatureOpcode(int value) { this.value = value; }
        public int getValue() { return value; }
    }

    public enum MemxSetFeatureOpcode {
        OPCODE_SET_THERMAL_THRESHOLD(0),
        OPCODE_SET_FREQUENCY(1),
        OPCODE_SET_VOLTAGE(2),
        OPCODE_SET_POWERMANAGEMENT(3),
        OPCODE_SET_POWER_THRESHOLD(4),
        OPCODE_SET_POWER_ALERT_FREQUENCY(5),
        OPCODE_SET_FEATURE_MAX(6); // Assuming max is the next value

        private final int value;
        MemxSetFeatureOpcode(int value) { this.value = value; }
        public int getValue() { return value; }
    }

    // --- Power State ---
    public enum MemxPowerState {
        MEMX_PS0, // Operational power state (I/O Support)
        MEMX_PS1, // Operational power state (I/O Support)
        MEMX_PS2, // Non-operational power state (I/O not Support)
        MEMX_PS3, // Non-operational power state (I/O not Support)
        MEMX_PS4, // Non-operational power state (I/O not Support, xFlow not Support, device based)
        MAX_POWER_STATE;
    }

    // --- Boot Mode ---
    public enum MxmXBootMode {
        MXMX_BOOT_MODE_QSPI(0),
        MXMX_BOOT_MODE_USB(1),
        MXMX_BOOT_MODE_PCIE(2),
        MXMX_BOOT_MODE_UART(3),
        MXMX_BOOT_MODE_MAX(4); // Assuming max is the next value

        private final int value;
        MxmXBootMode(int value) { this.value = value; }
        public int getValue() { return value; }

        public static MxmXBootMode fromValue(int value) {
             for (MxmXBootMode mode : values()) {
                 if (mode.value == value) return mode;
             }
             return null; // Or throw exception
         }
    }

    // --- Chip Version ---
    public enum MxmXChipVersion {
        MXMX_CHIP_VERSION_A0(0),
        MXMX_CHIP_VERSION_A1(5), // Note the gap
        MXMX_CHIP_VERSION_MAX(6); // Assuming max follows last defined value numerically

        private final int value;
        MxmXChipVersion(int value) { this.value = value; }
        public int getValue() { return value; }

        public static MxmXChipVersion fromValue(int value) {
             for (MxmXChipVersion ver : values()) {
                 if (ver.value == value) return ver;
             }
             return null; // Or throw exception
         }
    }

    // --- Module Info Helpers ---
    // GET_BOOT_MODE(module_info) ((module_info >> 32) & 0x3)
    public static MxmXBootMode getBootMode(long moduleInfo) {
        int modeValue = (int)((moduleInfo >> 32) & 0x3);
        return MxmXBootMode.fromValue(modeValue);
    }

    // GET_CHIP_VERSION(module_info) (module_info & 0xF)
    public static MxmXChipVersion getChipVersion(long moduleInfo) {
        int versionValue = (int)(moduleInfo & 0xF);
        return MxmXChipVersion.fromValue(versionValue);
    }


    // --- Structs ---
    public static class MemxThroughputInformation {
        public int igr_from_host_us; // unsigned int -> int (assuming values fit)
        public int igr_from_host_kb;
        public int igr_to_mpu_us;
        public int igr_to_mpu_kb;
        public int egr_from_mpu_us;
        public int egr_from_mpu_kb;
        public int egr_to_host_us;
        public int egr_to_host_kb;
        public int kdrv_tx_us;
        public int kdrv_tx_kb;
        public int kdrv_rx_us;
        public int kdrv_rx_kb;
        public int udrv_write_us;
        public int udrv_write_kb;
        public int udrv_read_us;
        public int udrv_read_kb;

        // Default constructor
        public MemxThroughputInformation() {}
    }

    public static class MemxFmapBuf {
        public long size;  // size_t -> long
        public long idx;   // size_t -> long
        public ByteBuffer data; // uint8_t* -> ByteBuffer (assumed direct for efficiency)

        /**
         * Constructor for MemxFmapBuf.
         * @param size Capacity of the buffer (in bytes).
         * @param idx Index or offset information (usage defined by library).
         * @param data A direct ByteBuffer. Must remain valid for the duration of the native call.
         */
        public MemxFmapBuf(long size, long idx, ByteBuffer data) {
            this.size = size;
            this.idx = idx;
            this.data = data;
            // Optional: Add checks if data is direct and has enough capacity
            // if (data == null || !data.isDirect() || data.capacity() < size) {
            //     throw new IllegalArgumentException("ByteBuffer must be direct and have sufficient capacity.");
            // }
        }
    }

    // --- Constants ---
    /** MemryX device MX3:Cascade+ */
    public static final int MEMX_DEVICE_CASCADE_PLUS = 31;

    /** Maximum number of model contexts */
    public static final int MEMX_MODEL_MAX_NUMBER = 32;
    /** Maximum number of MPU device group contexts */
    public static final int MEMX_DEVICE_GROUP_MAX_NUMBER = 4;

    /** Option: 32-bits floating point */
    public static final int MEMX_FMAP_FORMAT_FLOAT32 = 5;
    /** Option: bfloat16 */
    public static final int MEMX_FMAP_FORMAT_BF16 = 4;
    /** Option: raw byte array */
    public static final int MEMX_FMAP_FORMAT_RAW = 2;
    /** Option: MemryX proprietary format group-bfloat-80 */
    public static final int MEMX_FMAP_FORMAT_GBF80 = 0;
    public static final int MEMX_FMAP_FORMAT_GBF80_ROW_PAD = 6;

    public static final int MEMX_DOWNLOAD_TYPE_FROM_FILE = 0;
    public static final int MEMX_DOWNLOAD_TYPE_FROM_BUFFER = (1 << 7); // 128

    /** Option: download weight memory only */
    public static final int MEMX_DOWNLOAD_TYPE_WTMEM = 1;
    /** Option: download model only */
    public static final int MEMX_DOWNLOAD_TYPE_MODEL = 2;
    /** Option: download both weight memory and model */
    public static final int MEMX_DOWNLOAD_TYPE_WTMEM_AND_MODEL = 3;
    /** Option: download both using buffer pointer */
    public static final int MEMX_DOWNLOAD_TYPE_WTMEM_AND_MODEL_BUFFER = (MEMX_DOWNLOAD_TYPE_FROM_BUFFER | MEMX_DOWNLOAD_TYPE_WTMEM_AND_MODEL); // 128 | 3 = 131

    /** Option: configure MPU group - one group, four MPUs */
    public static final int MEMX_MPU_GROUP_CONFIG_ONE_GROUP_FOUR_MPUS = 0;
    /** Option: configure MPU group - two groups, two MPUs each */
    public static final int MEMX_MPU_GROUP_CONFIG_TWO_GROUP_TWO_MPUS = 1;
     /** Option: configure MPU group - one group, one MPU */
    public static final int MEMX_MPU_GROUP_CONFIG_ONE_GROUP_ONE_MPU = 2;
    /** Option: configure MPU group - one group, three MPUs */
    public static final int MEMX_MPU_GROUP_CONFIG_ONE_GROUP_THREE_MPUS = 3;
    /** Option: configure MPU group - one group, two MPUs */
    public static final int MEMX_MPU_GROUP_CONFIG_ONE_GROUP_TWO_MPUS = 4;
    /** Option: configure MPU group - one group, eight MPUs */
    public static final int MEMX_MPU_GROUP_CONFIG_ONE_GROUP_EIGHT_MPUS = 5;
    /** Option: configure MPU group - one group, twelve MPUs */
    public static final int MEMX_MPU_GROUP_CONFIG_ONE_GROUP_TWELVE_MPUS = 6;
    /** Option: configure MPU group - one group, sixteen MPUs */
    public static final int MEMX_MPU_GROUP_CONFIG_ONE_GROUP_SIXTEEN_MPUS = 7;

    // --- Command Enum ---
    public enum MemxCommand {
        MEMX_CMD_READ_TOTAL_CHIP_COUNT(0),
        MEMX_CMD_GET_FW_DOWNLOAD_STATUS(1),
        MEMX_CMD_CONFIG_MPU_GROUP(2),
        MEMX_CMD_RESET_DEVICE(3),
        MEMX_CMD_MAX(4); // Assuming max is the next value

        private final int value;
        MemxCommand(int value) { this.value = value; }
        public int getValue() { return value; }
    }

    // --- Helper classes for output parameters ---
    // Used for functions returning multiple primitive values via pointers

    public static class IntRef { public int value; }
    public static class FloatRef { public float value; }
    public static class ByteRef { public byte value; }
    public static class LongRef { public long value; } // For size_t if needed

    // Wrapper for memx_get_ofmap_hpoc return
    public static class HpocResult {
        public int status; // The MemxStatus value
        public int hpocSize;
        public int[] hpocIndexes; // Can be null if hpocSize is 0 or on error
    }
}
