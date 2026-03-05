package com.memryx.nativelib;

import android.util.Log;
import java.util.Map;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class NativeLib {

    private static final String TAG = "NativeLib";

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

    // --- Native Method Declarations ---

    /**
     * Loads a DFP file using the native DfpObject parser.
     * @param dfpPath Absolute path to the .dfp file.
     * @return A DfpInfo object containing metadata, or null on failure.
     */
    public static native DfpInfo nativeLoadDfpInfo(String dfpPath);

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
    public static native int nativeCalculateFormattedSize(int floatCount, int format, int h, int w, int z, int c);

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
    public static native boolean nativeConvertFloatToFormattedBytes(float[] inputFloatData, byte[] outputFormattedBytes, int h, int w, int z, int c, int format);

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
    public static native boolean nativeUnconvertFormattedBytesToFloat(byte[] inputFormattedBytes, float[] outputFloatData, int h, int w, int z, int c, int format, boolean hpocEnabled, int hpocSize, int[] hpocIndices, boolean rowPad);


    // --- Load the JNI Library ---
    static {
        try {
            // Use the name you define in CMakeLists.txt (without "lib" prefix and ".so" suffix)
            System.loadLibrary("nativelib");
            Log.i(TAG, "Successfully loaded nativelib library.");
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Failed to load nativelib library!", e);
            // Handle library loading failure (e.g., show error, disable functionality)
        }
    }
}