package com.memryx.jnilib;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import com.memryx.jnilib.MemxHeader.*; // Import constants, enums, structs

public class MemxJni {

    static {
        // Load the native library. The name should match the compiled JNI library file
        // (e.g., libmemx_jni.so on Linux, memx_jni.dll on Windows)
        try {
            // Use the name you define in CMakeLists.txt (without "lib" prefix and ".so" suffix)
            System.loadLibrary("memx_jni");
            System.out.println("Successfully loaded memx_jni library.");
        } catch (UnsatisfiedLinkError e) {
            System.err.println("Failed to load memx_jni library!");
            throw e; // Rethrow the error to indicate failure
        }
    }

    // --- Memx Native Method Declarations ---
    public static native int memx_lock(byte group_id);
    public static native int memx_trylock(byte group_id);
    public static native int memx_unlock(byte group_id);
    public static native int memx_open(byte model_id, byte group_id, float chip_gen); // chip_gen is deprecated, pass any float e.g., 0.0f
    public static native int memx_close(byte model_id);
    public static native int memx_operation(byte model_id, int cmd_id, ByteBuffer data, int size);
    public static native int memx_download_model_wtmem(byte model_id, String file_path);
    public static native int memx_download_model_config(byte model_id, String file_path, byte model_idx);
    public static native int memx_download_model(byte model_id, String file_path, byte model_idx, int type);
    public static native int memx_download_firmware(byte group_id, Object data, byte type); // data can be String or ByteBuffer
    public static native int memx_set_stream_enable(byte model_id, int wait);
    public static native int memx_set_stream_disable(byte model_id, int wait);
    public static native int memx_set_ifmap_queue_size(byte model_id, int size);
    public static native int memx_set_ofmap_queue_size(byte model_id, int size);
    public static native int memx_get_ifmap_size(byte model_id, byte flow_id,
                                                 IntRef height, IntRef width, IntRef z,
                                                 IntRef channel_number, IntRef format);
    public static native int memx_get_ifmap_range_convert(byte model_id, byte flow_id,
                                                        IntRef enable, FloatRef shift, FloatRef scale);
    public static native int memx_get_ofmap_size(byte model_id, byte flow_id,
                                                 IntRef height, IntRef width, IntRef z,
                                                 IntRef channel_number, IntRef format);
    public static native HpocResult memx_get_ofmap_hpoc(byte model_id, byte flow_id);
    public static native int memx_operation_get_device_count(IntRef count); // Assuming count is int/uint32
    public static native int memx_operation_get_mpu_group_count(byte group_id, IntRef count); // Assuming count is int/uint32
    public static native int memx_stream_ifmap(byte model_id, byte flow_id, ByteBuffer ifmap, int timeout);
    public static native int memx_stream_ofmap(byte model_id, byte flow_id, ByteBuffer ofmap, int timeout);
    public static native int memx_config_mpu_group(byte group_id, byte mpu_group_config);
    public static native int memx_get_chip_gen(byte model_id, ByteRef chip_gen);
    public static native int memx_set_powerstate(byte model_id, byte state);
    public static native int memx_enter_device_deep_sleep(byte group_id);
    public static native int memx_exit_device_deep_sleep(byte group_id);
    public static native int memx_get_total_chip_count(byte group_id, ByteRef chip_count);
    public static native int memx_get_feature(byte group_id, byte chip_id, int opcode, ByteBuffer buffer);
    public static native int memx_set_feature(byte group_id, byte chip_id, int opcode, short parameter); // uint16_t -> short
    public static native int memx_enqueue_ifmap_buf(byte model_id, byte flow_id, MemxFmapBuf fmap_buf, int timeout);
    public static native int memx_enqueue_ofmap_buf(byte model_id, byte flow_id, MemxFmapBuf fmap_buf, int timeout);
    public static native int memx_dequeue_ifmap_buf(byte model_id, byte flow_id, MemxFmapBuf fmap_buf, int timeout);
    public static native int memx_dequeue_ofmap_buf(byte model_id, byte flow_id, MemxFmapBuf fmap_buf, int timeout);

    // --- Data Structures to mirror C++ PortInfo/DfpMeta ---
    // These classes hold information retrieved from the native layer.
    public static class PortInfo {
        public int port;
        public boolean active;
        public int port_set;
        public int mpu_id;
        public int model_index;
        public int format; // Crucial: e.g., 0=GBF80, 4=BF16, 5=FP32 etc.
        public String layer_name;
        public int dim_h, dim_w, dim_z;
        public int dim_c; // Use int for Java compatibility, cast from uint32 in JNI if needed
        public int total_size; // Float count

        // HPOC related fields (for output ports)
        public boolean hpoc_en;
        public int hpoc_dim_c;
        public int[] hpoc_dummy_channels; // Use int[] instead of uint16_t*

        @Override
        public String toString() {
            // Basic toString for logging
            return "PortInfo{" +
                    "port=" + port +
                    ", active=" + active +
                    ", format=" + format +
                    ", layer_name='" + layer_name + '\'' +
                    ", shape=[" + dim_h + "," + dim_w + "," + dim_z + "," + dim_c +"]" +
                    ", total_size=" + total_size +
                    ", hpoc_en=" + hpoc_en +
                    '}';
        }
    }

    public static class DfpInfo {
        public boolean valid;
        public int num_chips;
        public List<PortInfo> inputPorts;
        public List<PortInfo> outputPorts;
        // Add other meta fields if needed (version, timestamp etc.)

        public DfpInfo() {
            inputPorts = new ArrayList<>();
            outputPorts = new ArrayList<>();
            valid = false;
        }
    }

    // --- DFP Native Method Declarations ---
    /**
     * Loads a DFP file using the native DfpObject parser.
     * @param dfpPath Absolute path to the .dfp file.
     * @return A DfpInfo object containing metadata, or null on failure.
     */
    public static native DfpInfo loadDfpInfo(String dfpPath);

    /**
     * Calculates the required byte size for a buffer given its float dimensions and format.
     * Calls the C++ cal_format_size logic.
     * @param floatCount Total number of float elements (h*w*z*c).
     * @param format The data format code (e.g., 0 for GBF80, 4 for BF16).
     * @param h Dimension h.
     * @param w Dimension w.
     * @param z Dimension z.
     * @param c Dimension c.
     * @return Required size in bytes for the formatted buffer, or -1 on error.
     */
    public static native int calculateFormattedSize(int floatCount, int format, int h, int w, int z, int c);

    /**
     * Converts a float array to a formatted byte array using native C++ functions.
     * @param inputFloatData The source float array.
     * @param outputFormattedBytes The pre-allocated byte array to be filled.
     * @param h Dimension h.
     * @param w Dimension w.
     * @param z Dimension z.
     * @param c Dimension c.
     * @param format The target data format code.
     * @return true on success, false on failure.
     */
    public static native boolean convertFloatToFormattedBytes(float[] inputFloatData, byte[] outputFormattedBytes, int h, int w, int z, int c, int format);

    /**
     * Converts a formatted byte array back to a float array using native C++ functions.
     * @param inputFormattedBytes The source byte array (filled by memxStreamOfmap).
     * @param outputFloatData The pre-allocated float array to be filled.
     * @param h Dimension h.
     * @param w Dimension w.
     * @param z Dimension z.
     * @param c Dimension c (original, before HPOC).
     * @param format The source data format code.
     * @param hpocEnabled Whether HPOC is enabled for this port.
     * @param hpocSize Number of HPOC channels (hpoc_dim_c - dim_c).
     * @param hpocIndices Array of HPOC dummy channel indices.
     * @param rowPad Whether row padding was used (relevant for GBF80_ROW).
     * @return true on success, false on failure.
     */
    public static native boolean unconvertFormattedBytesToFloat(byte[] inputFormattedBytes, float[] outputFloatData, int h, int w, int z, int c, int format, boolean hpocEnabled, int hpocSize, int[] hpocIndices, boolean rowPad);

}