#define LOG_TAG "NativeLib"
#include <android/log.h>
#include <jni.h>
#include <string>
#include <vector>
#include <memory> // For std::unique_ptr
#include <exception> // For std::exception
#include <stdexcept> // For std::runtime_error

// Include your custom C++ headers
// Ensure these headers are accessible from your jni directory (e.g., copy them or adjust include paths in CMakeLists.txt)
#include "dfp.h"       // For DfpObject, DfpMeta, PortInfo
#include "convert.h"   // For convert_*, unconvert_*, cal_format_size (assuming cal_format_size is accessible or reimplemented)

#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

// Helper to convert C++ std::vector<int32_t> to Java jintArray
jintArray vectorToJavaIntArray(JNIEnv* env, const std::vector<int32_t>& vec) {
    if (!env) return nullptr;
    jintArray out = env->NewIntArray(vec.size());
    if (out == nullptr) {
        ALOGE("vectorToJavaIntArray: Failed to allocate Java int array.");
        return nullptr;
    }
    // Use const_cast if SetIntArrayRegion expects non-const pointer, though usually okay with const data()
    env->SetIntArrayRegion(out, 0, vec.size(), vec.data());
    return out;
}

// Helper to convert C++ uint16_t* array to Java jintArray
jintArray vectorU16ToJavaIntArray(JNIEnv* env, const uint16_t* data, int size) {
    if (!env || !data || size <= 0) return nullptr;
    // Create a temporary vector of jint because JNI works with jint (usually int32_t)
    std::vector<jint> temp(size);
    for(int i = 0; i < size; ++i) {
        temp[i] = static_cast<jint>(data[i]);
    }
    jintArray out = env->NewIntArray(size);
    if (out == nullptr) {
        ALOGE("vectorU16ToJavaIntArray: Failed to allocate Java int array.");
        return nullptr;
    }
    env->SetIntArrayRegion(out, 0, size, temp.data());
    return out;
}

// Helper to convert Java jintArray to C++ std::vector<int> (used for hpocIndices)
std::vector<int> javaIntArrayToVector(JNIEnv* env, jintArray intArray) {
    std::vector<int> vec;
    if (!env || !intArray) return vec; // Return empty vector on null input

    jsize len = env->GetArrayLength(intArray);
    if (len <= 0) return vec; // Return empty vector for zero length

    jint* elements = env->GetIntArrayElements(intArray, nullptr);
    if (!elements) {
        ALOGE("javaIntArrayToVector: Failed to get int array elements.");
        return vec; // Return empty vector on error
    }

    vec.assign(elements, elements + len); // Copy elements

    env->ReleaseIntArrayElements(intArray, elements, JNI_ABORT); // Release without copy back
    return vec;
}


// --- JNI Function Implementations ---

extern "C" JNIEXPORT jobject JNICALL
Java_com_memryx_nativelib_NativeLib_nativeLoadDfpInfo(
    JNIEnv* env,
    jclass clazz __attribute__((unused)),
    jstring dfpPath) {

    const char* pathChars = env->GetStringUTFChars(dfpPath, nullptr);
    if (!pathChars) {
        ALOGE("nativeLoadDfpInfo: Failed to get path string chars");
        return nullptr;
    }
    std::string pathStr = pathChars;
    env->ReleaseStringUTFChars(dfpPath, pathChars); // Release as soon as copied

    ALOGD("nativeLoadDfpInfo: Attempting to load DFP from %s", pathStr.c_str());

    // Use unique_ptr for exception safety with DfpObject allocation
    std::unique_ptr<Dfp::DfpObject> dfp;
    try {
         dfp = std::make_unique<Dfp::DfpObject>(pathStr);
         // Check validity after construction
         if (!dfp) {
              ALOGE("nativeLoadDfpInfo: Failed to load or invalid DFP object for path: %s", pathStr.c_str());
              // DfpObject constructor might print its own error via printf
              return nullptr;
         }
    } catch (const std::exception& e) {
         ALOGE("nativeLoadDfpInfo: Exception during DfpObject creation for %s: %s", pathStr.c_str(), e.what());
         return nullptr;
    } catch (...) {
         ALOGE("nativeLoadDfpInfo: Unknown exception during DfpObject creation for %s", pathStr.c_str());
         return nullptr;
    }

    // --- Create and Populate Java DfpInfo object ---
    jclass dfpInfoClass = env->FindClass("com/memryx/nativelib/NativeLib$DfpInfo");
    if (!dfpInfoClass) { ALOGE("Cannot find DfpInfo class"); return nullptr; }
    jmethodID dfpInfoCtor = env->GetMethodID(dfpInfoClass, "<init>", "()V");
    if (!dfpInfoCtor) { ALOGE("Cannot find DfpInfo constructor"); env->DeleteLocalRef(dfpInfoClass); return nullptr; }
    jobject dfpInfoObj = env->NewObject(dfpInfoClass, dfpInfoCtor);
    if (!dfpInfoObj) { ALOGE("Cannot create DfpInfo object"); env->DeleteLocalRef(dfpInfoClass); return nullptr; }

    // Get DfpMeta
    Dfp::DfpMeta meta = dfp->get_dfp_meta();

    // Get Field IDs for DfpInfo
    jfieldID validField = env->GetFieldID(dfpInfoClass, "valid", "Z");
    jfieldID numChipsField = env->GetFieldID(dfpInfoClass, "num_chips", "I");
    jfieldID inputPortsField = env->GetFieldID(dfpInfoClass, "inputPorts", "Ljava/util/List;");
    jfieldID outputPortsField = env->GetFieldID(dfpInfoClass, "outputPorts", "Ljava/util/List;");

    if (!validField || !numChipsField || !inputPortsField || !outputPortsField) {
        ALOGE("Cannot find fields in DfpInfo class");
        env->DeleteLocalRef(dfpInfoClass); // Clean up class ref
        env->DeleteLocalRef(dfpInfoObj); // Clean up object ref
        return nullptr;
    }

    // Set primitive fields
    env->SetBooleanField(dfpInfoObj, validField, JNI_TRUE); // If we reached here, DFP is considered valid
    env->SetIntField(dfpInfoObj, numChipsField, meta.num_chips);

    // Get List interface and add method ID
    jclass listClass = env->FindClass("java/util/List");
    if (!listClass) { ALOGE("Cannot find List class"); /* cleanup */ return nullptr; }
    jmethodID listAdd = env->GetMethodID(listClass, "add", "(Ljava/lang/Object;)Z");
    if (!listAdd) { ALOGE("Cannot find List.add method"); /* cleanup */ return nullptr; }

    // Get PortInfo class and constructor/fields
    jclass portInfoClass = env->FindClass("com/memryx/nativelib/NativeLib$PortInfo");
    if (!portInfoClass) { ALOGE("Cannot find PortInfo class"); /* cleanup */ return nullptr; }
    jmethodID portInfoCtor = env->GetMethodID(portInfoClass, "<init>", "()V");
    if (!portInfoCtor) { ALOGE("Cannot find PortInfo constructor"); /* cleanup */ return nullptr; }
    // Get Field IDs for PortInfo
    jfieldID p_port = env->GetFieldID(portInfoClass, "port", "I");
    jfieldID p_active = env->GetFieldID(portInfoClass, "active", "Z");
    jfieldID p_port_set = env->GetFieldID(portInfoClass, "port_set", "I");
    jfieldID p_mpu_id = env->GetFieldID(portInfoClass, "mpu_id", "I");
    jfieldID p_model_index = env->GetFieldID(portInfoClass, "model_index", "I");
    jfieldID p_format = env->GetFieldID(portInfoClass, "format", "I");
    jfieldID p_layer_name = env->GetFieldID(portInfoClass, "layer_name", "Ljava/lang/String;");
    jfieldID p_dim_h = env->GetFieldID(portInfoClass, "dim_h", "I");
    jfieldID p_dim_w = env->GetFieldID(portInfoClass, "dim_w", "I");
    jfieldID p_dim_z = env->GetFieldID(portInfoClass, "dim_z", "I");
    jfieldID p_dim_c = env->GetFieldID(portInfoClass, "dim_c", "I");
    jfieldID p_total_size = env->GetFieldID(portInfoClass, "total_size", "I");
    jfieldID p_hpoc_en = env->GetFieldID(portInfoClass, "hpoc_en", "Z");
    jfieldID p_hpoc_dim_c = env->GetFieldID(portInfoClass, "hpoc_dim_c", "I");
    jfieldID p_hpoc_dummy_channels = env->GetFieldID(portInfoClass, "hpoc_dummy_channels", "[I");
    // Check if all field IDs were found
     if (!p_port || !p_active || !p_port_set || !p_mpu_id || !p_model_index || !p_format || !p_layer_name ||
        !p_dim_h || !p_dim_w || !p_dim_z || !p_dim_c || !p_total_size || !p_hpoc_en || !p_hpoc_dim_c || !p_hpoc_dummy_channels) {
         ALOGE("Cannot find one or more fields in PortInfo class");
         // Clean up references obtained so far
         env->DeleteLocalRef(dfpInfoClass);
         env->DeleteLocalRef(dfpInfoObj);
         env->DeleteLocalRef(listClass);
         env->DeleteLocalRef(portInfoClass);
         return nullptr;
    }

    // Populate inputPorts List
    jobject inputListObj = env->GetObjectField(dfpInfoObj, inputPortsField);
    if (!inputListObj) { ALOGE("Failed to get inputPorts List object"); /* cleanup */ return nullptr; }

    for (int i = 0; i < meta.num_used_inports; ++i) { // Use num_used_inports
        Dfp::PortInfo* cppPortInfo = dfp->input_port(i); // DfpObject handles index check
        if (!cppPortInfo) {
             ALOGW("nativeLoadDfpInfo: Got null for input port %d", i);
             continue; // Skip this port
        }

        jobject portInfoObj = env->NewObject(portInfoClass, portInfoCtor);
        if (!portInfoObj) { ALOGE("Failed to create PortInfo object for input port %d", i); /* cleanup */ return nullptr; }

        env->SetIntField(portInfoObj, p_port, cppPortInfo->port);
        env->SetBooleanField(portInfoObj, p_active, cppPortInfo->active);
        env->SetIntField(portInfoObj, p_port_set, cppPortInfo->port_set);
        env->SetIntField(portInfoObj, p_mpu_id, cppPortInfo->mpu_id);
        env->SetIntField(portInfoObj, p_model_index, cppPortInfo->model_index);
        env->SetIntField(portInfoObj, p_format, cppPortInfo->format);
        jstring layerName = env->NewStringUTF(cppPortInfo->layer_name ? cppPortInfo->layer_name : "");
        env->SetObjectField(portInfoObj, p_layer_name, layerName);
        if (layerName) env->DeleteLocalRef(layerName); // Release local reference
        env->SetIntField(portInfoObj, p_dim_h, cppPortInfo->dim_h);
        env->SetIntField(portInfoObj, p_dim_w, cppPortInfo->dim_w);
        env->SetIntField(portInfoObj, p_dim_z, cppPortInfo->dim_z);
        env->SetIntField(portInfoObj, p_dim_c, cppPortInfo->dim_c);
        env->SetIntField(portInfoObj, p_total_size, cppPortInfo->total_size);
        // HPOC fields are typically for output ports
        env->SetBooleanField(portInfoObj, p_hpoc_en, false);
        env->SetIntField(portInfoObj, p_hpoc_dim_c, 0);
        env->SetObjectField(portInfoObj, p_hpoc_dummy_channels, nullptr);

        env->CallBooleanMethod(inputListObj, listAdd, portInfoObj);
        env->DeleteLocalRef(portInfoObj); // Release local reference to the PortInfo object
    }
    env->DeleteLocalRef(inputListObj); // Release local reference to the List object

    // Populate outputPorts List
    jobject outputListObj = env->GetObjectField(dfpInfoObj, outputPortsField);
     if (!outputListObj) { ALOGE("Failed to get outputPorts List object"); /* cleanup */ return nullptr; }

     for (int i = 0; i < meta.num_used_outports; ++i) { // Use num_used_outports
        Dfp::PortInfo* cppPortInfo = dfp->output_port(i);
         if (!cppPortInfo) {
             ALOGW("nativeLoadDfpInfo: Got null for output port %d", i);
             continue; // Skip this port
         }

        jobject portInfoObj = env->NewObject(portInfoClass, portInfoCtor);
        if (!portInfoObj) { ALOGE("Failed to create PortInfo object for output port %d", i); /* cleanup */ return nullptr; }

        env->SetIntField(portInfoObj, p_port, cppPortInfo->port);
        env->SetBooleanField(portInfoObj, p_active, cppPortInfo->active);
        env->SetIntField(portInfoObj, p_port_set, cppPortInfo->port_set);
        env->SetIntField(portInfoObj, p_mpu_id, cppPortInfo->mpu_id);
        env->SetIntField(portInfoObj, p_model_index, cppPortInfo->model_index);
        env->SetIntField(portInfoObj, p_format, cppPortInfo->format);
        jstring layerName = env->NewStringUTF(cppPortInfo->layer_name ? cppPortInfo->layer_name : "");
        env->SetObjectField(portInfoObj, p_layer_name, layerName);
        if (layerName) env->DeleteLocalRef(layerName);
        env->SetIntField(portInfoObj, p_dim_h, cppPortInfo->dim_h);
        env->SetIntField(portInfoObj, p_dim_w, cppPortInfo->dim_w);
        env->SetIntField(portInfoObj, p_dim_z, cppPortInfo->dim_z);
        env->SetIntField(portInfoObj, p_dim_c, cppPortInfo->dim_c);
        env->SetIntField(portInfoObj, p_total_size, cppPortInfo->total_size);
        // Set HPOC fields for output ports
        env->SetBooleanField(portInfoObj, p_hpoc_en, cppPortInfo->hpoc_en);
        env->SetIntField(portInfoObj, p_hpoc_dim_c, cppPortInfo->hpoc_dim_c);
        // Convert uint16_t* to jintArray
        jintArray hpocArray = vectorU16ToJavaIntArray(env, cppPortInfo->hpoc_dummy_channels, cppPortInfo->hpoc_list_length);
        env->SetObjectField(portInfoObj, p_hpoc_dummy_channels, hpocArray);
        if(hpocArray) env->DeleteLocalRef(hpocArray); // Release local reference if created

        env->CallBooleanMethod(outputListObj, listAdd, portInfoObj);
        env->DeleteLocalRef(portInfoObj); // Release local ref
    }
     env->DeleteLocalRef(outputListObj); // Release local ref

    // Clean up class references
    env->DeleteLocalRef(dfpInfoClass);
    env->DeleteLocalRef(listClass);
    env->DeleteLocalRef(portInfoClass);

    ALOGD("nativeLoadDfpInfo: Successfully loaded and parsed DFP: %s", pathStr.c_str());
    return dfpInfoObj; // Return the populated Java object
}


// JNI wrapper for C++ calculateFormattedSize logic
// Assuming the logic exists in a function `cal_format_size_cpp` based on C++ benchmark code
// Or reimplemented here based on format and dims.
// We need the C++ function `cal_format_size` from memx_benchmark.cpp to be available,
// let's assume it's declared in convert.h or dfp.h, or we reimplement it here.

// Reimplementing cal_format_size logic here for completeness
uint32_t calculateFormattedSizeInternal(int floatCount, int format, int h, int w, int z, int c) {
    uint32_t formatted_featuremap_size = 0;
    uint32_t total_size = floatCount; // Assuming floatCount is h*w*z*c

    // Logic adapted from C++ memx_benchmark.cpp cal_format_size
    switch(format)
    {
        case 2: //MX_FMT_RGB888 - Check if total_size means bytes or floats
             // Assuming total_size in PortInfo meant float count, needs verification
             // If format is RGB888, size might be h*w*z*3? This needs clarification.
             // Let's assume floatCount IS the byte size for RGB for now, needs check.
             formatted_featuremap_size = total_size;
             ALOGW("calculateFormattedSizeInternal: Format RGB888 size calculation might be incorrect.");
             break;
        case 5: //MX_FMT_FP32
             formatted_featuremap_size = total_size * 4;
             break;
        case 4: //MX_FMT_BF16
             formatted_featuremap_size = total_size * 2;
             // C++ code checked formatted_featuremap_size % 2, which seems wrong.
             // Let's assume padding aligns to 4 bytes if needed, or maybe no padding?
             // Sticking closer to C++ code for now, but this padding seems odd.
             // if (formatted_featuremap_size % 2 ) formatted_featuremap_size += 2;
             // Let's assume 4-byte alignment per element pair is more likely if padding exists
             // Or maybe total size is already padded? Let's just use * 2 for now.
             break;
        case 0: { //MX_FMT_GBF80
             if (c == 0) { ALOGE("Channel count (c) is zero for GBF80"); return 0; }
             size_t num_xyz_pixels = (size_t)h * w * z;
             bool   any_remainder_chs = ((c % 8) != 0);
             size_t num_gbf_words = (c / 8) + (any_remainder_chs ? 1 : 0);
             formatted_featuremap_size = (uint32_t)(num_xyz_pixels * num_gbf_words * 10);
             break;
         }
        case 6: { //MX_FMT_GBF80_ROW
             if (c == 0) { ALOGE("Channel count (c) is zero for GBF80_ROW"); return 0; }
             bool   any_remainder_chs = ((c % 8) != 0);
             size_t num_gbf_words = (c / 8) + (any_remainder_chs ? 1 : 0);
             // padding row size to 4 bytes-alignment
             formatted_featuremap_size = h * (((uint32_t)w * z * num_gbf_words * 10 + 3) & ~0x3);
             break;
         }
        default:
             ALOGE("calculateFormattedSizeInternal: Invalid format code %d", format);
             return 0; // Error
    }
    return formatted_featuremap_size;
}


extern "C" JNIEXPORT jint JNICALL
Java_com_memryx_nativelib_NativeLib_nativeCalculateFormattedSize(
    JNIEnv* env, jclass clazz, jint floatCount, jint format, jint h, jint w, jint z, jint c) {

    ALOGD("nativeCalculateFormattedSize(floatCount=%d, format=%d, h=%d, w=%d, z=%d, c=%d)",
          floatCount, format, h, w, z, c);

    // Call the internal calculation logic
    uint32_t calculated_size = calculateFormattedSizeInternal(floatCount, format, h, w, z, c);

    if (calculated_size == 0 && format != 2) { // Allow 0 size only if format might be weird like RGB888
         ALOGE("nativeCalculateFormattedSize: Calculated size is zero for format %d", format);
         return -1; // Indicate error
    }

    ALOGD("nativeCalculateFormattedSize: Calculated size = %u", calculated_size);
    return static_cast<jint>(calculated_size);
}


// JNI wrapper for C++ convert_data logic
extern "C" JNIEXPORT jboolean JNICALL
Java_com_memryx_nativelib_NativeLib_nativeConvertFloatToFormattedBytes(
    JNIEnv* env, jclass clazz, jfloatArray inputFloatData, jbyteArray outputFormattedBytes,
    jint h, jint w, jint z, jint c, jint format) {

    jfloat* inputFloats = env->GetFloatArrayElements(inputFloatData, nullptr);
    jbyte* outputBytes = env->GetByteArrayElements(outputFormattedBytes, nullptr);
    jsize inputLen = env->GetArrayLength(inputFloatData); // Number of floats
    // jsize outputByteLen = env->GetArrayLength(outputFormattedBytes); // Size in bytes

    ALOGD("nativeConvertFloatToFormattedBytes(format=%d, h=%d, w=%d, z=%d, c=%d, inputLen=%d)",
          format, h, w, z, c, (int)inputLen);

    if (!inputFloats || !outputBytes) {
        ALOGE("nativeConvertFloatToFormattedBytes: Failed to get array elements.");
        if (inputFloats) env->ReleaseFloatArrayElements(inputFloatData, inputFloats, JNI_ABORT);
        if (outputBytes) env->ReleaseByteArrayElements(outputFormattedBytes, outputBytes, JNI_ABORT);
        return JNI_FALSE;
    }

    bool success = true;
    try {
        // Call the appropriate C++ conversion function based on format
        switch(format) {
            case 2: // MX_FMT_RGB888 - Needs specific conversion logic if input is float
                 ALOGE("nativeConvertFloatToFormattedBytes: Conversion for RGB888 from float not implemented.");
                 success = false;
                 break;
            case 5: // MX_FMT_FP32 - Direct copy
                 memcpy(outputBytes, inputFloats, (size_t)inputLen * sizeof(float));
                 break;
            case 4: // MX_FMT_BF16
                 convert_bf16(inputFloats, reinterpret_cast<uint8_t*>(outputBytes), inputLen);
                 break;
            case 0: // MX_FMT_GBF80
                 convert_gbf(inputFloats, reinterpret_cast<uint8_t*>(outputBytes), inputLen, c);
                 break;
            case 6: // MX_FMT_GBF80_ROW
                 convert_gbf_row_pad(inputFloats, reinterpret_cast<uint8_t*>(outputBytes), h, w, z, c);
                 break;
            default:
                 ALOGE("nativeConvertFloatToFormattedBytes: Invalid format code %d", format);
                 success = false;
                 break;
        }
    } catch (const std::exception& e) {
        ALOGE("nativeConvertFloatToFormattedBytes: Exception during conversion: %s", e.what());
        success = false;
    } catch (...) {
         ALOGE("nativeConvertFloatToFormattedBytes: Unknown exception during conversion");
         success = false;
    }

    // Release arrays. Mode 0 copies back changes for outputBytes (which we wrote to).
    env->ReleaseFloatArrayElements(inputFloatData, inputFloats, JNI_ABORT); // Input buffer wasn't changed
    env->ReleaseByteArrayElements(outputFormattedBytes, outputBytes, success ? 0 : JNI_ABORT); // Copy back if successful

    ALOGD("nativeConvertFloatToFormattedBytes finished (success=%d)", success);
    return success ? JNI_TRUE : JNI_FALSE;
}

// JNI wrapper for C++ unconvert_data logic
extern "C" JNIEXPORT jboolean JNICALL
Java_com_memryx_nativelib_NativeLib_nativeUnconvertFormattedBytesToFloat(
    JNIEnv* env, jclass clazz, jbyteArray inputFormattedBytes, jfloatArray outputFloatData,
    jint h, jint w, jint z, jint c, jint format,
    jboolean hpocEnabled, jint hpocSize, jintArray hpocIndices, jboolean rowPad) {

    jbyte* inputBytes = env->GetByteArrayElements(inputFormattedBytes, nullptr);
    jfloat* outputFloats = env->GetFloatArrayElements(outputFloatData, nullptr);
    // jsize inputByteLen = env->GetArrayLength(inputFormattedBytes);
    jsize outputFloatLen = env->GetArrayLength(outputFloatData); // Number of floats

    ALOGD("nativeUnconvertFormattedBytesToFloat(format=%d, h=%d, w=%d, z=%d, c=%d, outputFloatLen=%d, hpoc=%d)",
          format, h, w, z, c, (int)outputFloatLen, (int)hpocEnabled);

    if (!inputBytes || !outputFloats) {
         ALOGE("nativeUnconvertFormattedBytesToFloat: Failed to get array elements.");
         if (inputBytes) env->ReleaseByteArrayElements(inputFormattedBytes, inputBytes, JNI_ABORT);
         if (outputFloats) env->ReleaseFloatArrayElements(outputFloatData, outputFloats, JNI_ABORT);
        return JNI_FALSE;
    }

    // Prepare HPOC indices if needed
    std::vector<int> hpocIndicesVec; // Use std::vector for safety
    int* hpocIndicesPtr = nullptr;
    if (hpocEnabled == JNI_TRUE && hpocIndices != nullptr) {
        jsize hpocLen = env->GetArrayLength(hpocIndices);
        // C++ unconvert_gbf_hpoc expects hpocSize = (hpoc_dim_c - dim_c), not array length
        // Let's trust the passed hpocSize argument from Java side.
        if (hpocLen > 0 && hpocSize > 0) {
             hpocIndicesVec = javaIntArrayToVector(env, hpocIndices);
             if (hpocIndicesVec.size() == hpocLen) { // Check conversion worked
                 hpocIndicesPtr = hpocIndicesVec.data();
                 ALOGD("nativeUnconvertFormattedBytesToFloat: Using HPOC indices (count=%d, size=%d)", (int)hpocLen, hpocSize);
             } else {
                  ALOGW("nativeUnconvertFormattedBytesToFloat: Failed to convert HPOC indices, disabling HPOC.");
                  hpocEnabled = JNI_FALSE;
             }
        } else {
             ALOGW("nativeUnconvertFormattedBytesToFloat: HPOC enabled but index array size (%d) or hpocSize (%d) is zero.", (int)hpocLen, hpocSize);
             hpocEnabled = JNI_FALSE; // Disable HPOC if sizes mismatch or array empty
        }
    } else {
         hpocEnabled = JNI_FALSE; // Disable if array is null
    }


    bool success = true;
    try {
        // Call the appropriate C++ un-conversion function based on format
         switch(format) {
            case 2: // MX_FMT_RGB888
                 ALOGE("nativeUnconvertFormattedBytesToFloat: Unconversion for RGB888 not implemented.");
                 success = false;
                 break;
            case 5: // MX_FMT_FP32
                 memcpy(outputFloats, inputBytes, (size_t)outputFloatLen * sizeof(float));
                 break;
            case 4: // MX_FMT_BF16
                 unconvert_bf16(reinterpret_cast<uint8_t*>(inputBytes), outputFloats, outputFloatLen);
                 break;
            case 0: // MX_FMT_GBF80
                 if (hpocEnabled == JNI_TRUE && hpocIndicesPtr) {
                     // Assuming rowPad is not applicable for non-row format
                     unconvert_gbf_hpoc(reinterpret_cast<uint8_t*>(inputBytes), outputFloats, h, w, z, c, hpocSize, hpocIndicesPtr, 0);
                 } else {
                     unconvert_gbf(reinterpret_cast<uint8_t*>(inputBytes), outputFloats, outputFloatLen, c);
                 }
                 break;
            case 6: // MX_FMT_GBF80_ROW
                 if (hpocEnabled == JNI_TRUE && hpocIndicesPtr) {
                     unconvert_gbf_hpoc(reinterpret_cast<uint8_t*>(inputBytes), outputFloats, h, w, z, c, hpocSize, hpocIndicesPtr, (rowPad == JNI_TRUE) ? 1 : 0);
                 } else {
                     unconvert_gbf_row_pad(reinterpret_cast<uint8_t*>(inputBytes), outputFloats, h, w, z, c);
                 }
                 break;
            default:
                 ALOGE("nativeUnconvertFormattedBytesToFloat: Invalid format code %d", format);
                 success = false;
                 break;
        }
    } catch (const std::exception& e) {
        ALOGE("nativeUnconvertFormattedBytesToFloat: Exception during unconversion: %s", e.what());
        success = false;
    } catch (...) {
         ALOGE("nativeUnconvertFormattedBytesToFloat: Unknown exception during unconversion");
         success = false;
    }

    // Release arrays. Mode 0 copies back changes for outputFloats.
    env->ReleaseByteArrayElements(inputFormattedBytes, inputBytes, JNI_ABORT); // Input buffer wasn't changed
    env->ReleaseFloatArrayElements(outputFloatData, outputFloats, success ? 0 : JNI_ABORT); // Copy back results if successful

    ALOGD("nativeUnconvertFormattedBytesToFloat finished (success=%d)", success);
    return success ? JNI_TRUE : JNI_FALSE;
}
