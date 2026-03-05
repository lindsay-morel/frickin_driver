#include <jni.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include <memory> // For std::unique_ptr
#include <stdexcept> // For exceptions if desired (though returning status code is more C-like)

// Include the original header file
#include "memx.h"
#include "dfp.h"       // For DfpObject, DfpMeta, PortInfo
#include "convert.h"   // For convert_*, unconvert_*, cal_format_size (assuming cal_format_size is accessible or reimplemented)

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "MemxJni"
#if defined(__ANDROID__)
#include <android/log.h>
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#else
#define ALOGE(...) fprintf(stderr, "E/%s: ", LOG_TAG); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n")
    #if defined(DEBUG)
    #define ALOGD(...) fprintf(stdout, "D/%s: ", LOG_TAG); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n")
    #define ALOGW(...) fprintf(stdout, "W/%s: ", LOG_TAG); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n")
    #else
    #define ALOGD(...) do {} while (0)
    #define ALOGW(...) do {} while (0)
    #endif
#endif

#ifndef JINT_PTR
  #define JINT_PTR(ptr) reinterpret_cast<const jint*>(ptr)
#endif

#ifndef JSIZE_CAST
  #define JSIZE_CAST(x) static_cast<jsize>(x)
#endif

#if defined(_MSC_VER)
  #define MAYBE_UNUSED
#else
  #define MAYBE_UNUSED [[maybe_unused]]
#endif

// Helper to get direct buffer address or throw if not direct
static inline void* GetDirectBufferAddressOrThrow(JNIEnv *env, jobject buf) {
    if (!buf) {
        // Optional: Throw an exception
        // env->ThrowNew(env->FindClass("java/lang/NullPointerException"), "ByteBuffer is null");
        return nullptr;
    }
    void* addr = env->GetDirectBufferAddress(buf);
    if (!addr) {
        // Optional: Throw an exception
         // env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), "ByteBuffer is not direct");
        // It's often better practice in JNI to return an error code matching the library's style
    }
    return addr;
}

// Helper function to get C string from Java String
// Remember to call releaseString() on the returned pointer
class JniStringHelper {
    JNIEnv* env_;
    jstring jstr_;
    const char* cstr_;
public:
    JniStringHelper(JNIEnv* env, jstring jstr) : env_(env), jstr_(jstr), cstr_(nullptr) {
        if (jstr_) {
            cstr_ = env_->GetStringUTFChars(jstr_, nullptr);
        }
    }
    ~JniStringHelper() {
        releaseString();
    }
    const char* getCString() const {
        return cstr_;
    }
    void releaseString() {
        if (cstr_) {
            env_->ReleaseStringUTFChars(jstr_, cstr_);
            cstr_ = nullptr; // Prevent double release
        }
    }
    // Delete copy constructor and assignment operator
    JniStringHelper(const JniStringHelper&) = delete;
    JniStringHelper& operator=(const JniStringHelper&) = delete;
};

// Helper to set primitive ref fields (IntRef, FloatRef, etc.)
template<typename T, typename J>
static inline void SetRefField(JNIEnv *env, jobject refObj, const char* fieldName, T value) {
    if (!refObj) return;
    jclass refClass = env->GetObjectClass(refObj);
    if (!refClass) return; // Error finding class
    // Field signature needs to match the Java type (I for int, F for float, B for byte, J for long)
    const char* sig = nullptr;
    if constexpr (std::is_same_v<T, int32_t> && std::is_same_v<J, jint>) sig = "I";
    else if constexpr (std::is_same_v<T, float> && std::is_same_v<J, jfloat>) sig = "F";
    else if constexpr (std::is_same_v<T, int8_t> && std::is_same_v<J, jbyte>) sig = "B";
    else if constexpr (std::is_same_v<T, int64_t> && std::is_same_v<J, jlong>) sig = "J";
    // Add more types if needed (e.g., short 'S', double 'D', boolean 'Z')
    else { /* Unsupported type */ return; }

    jfieldID fieldId = env->GetFieldID(refClass, "value", sig);
    if (!fieldId) return; // Error finding field

    if constexpr (std::is_same_v<J, jint>) env->SetIntField(refObj, fieldId, static_cast<jint>(value));
    else if constexpr (std::is_same_v<J, jfloat>) env->SetFloatField(refObj, fieldId, static_cast<jfloat>(value));
    else if constexpr (std::is_same_v<J, jbyte>) env->SetByteField(refObj, fieldId, static_cast<jbyte>(value));
    else if constexpr (std::is_same_v<J, jlong>) env->SetLongField(refObj, fieldId, static_cast<jlong>(value));

    env->DeleteLocalRef(refClass); // Clean up local reference
}

// Helper to get MemxFmapBuf fields
static inline bool GetMemxFmapBuf(JNIEnv *env, jobject jFmapBuf, memx_fmap_buf_t& cFmapBuf) {
     if (!jFmapBuf) return false;
     jclass fmapBufClass = env->GetObjectClass(jFmapBuf);
     if (!fmapBufClass) return false;

     jfieldID sizeField = env->GetFieldID(fmapBufClass, "size", "J"); // long -> J
     jfieldID idxField = env->GetFieldID(fmapBufClass, "idx", "J"); // long -> J
     jfieldID dataField = env->GetFieldID(fmapBufClass, "data", "Ljava/nio/ByteBuffer;"); // ByteBuffer

     if (!sizeField || !idxField || !dataField) {
         env->DeleteLocalRef(fmapBufClass);
         return false; // Field lookup failed
     }

     cFmapBuf.size = static_cast<size_t>(env->GetLongField(jFmapBuf, sizeField));
     cFmapBuf.idx = static_cast<size_t>(env->GetLongField(jFmapBuf, idxField));

     jobject jDataBuffer = env->GetObjectField(jFmapBuf, dataField);
     if (!jDataBuffer) {
         cFmapBuf.data = nullptr; // Or handle as error
     } else {
         cFmapBuf.data = static_cast<uint8_t*>(env->GetDirectBufferAddress(jDataBuffer));
         if (!cFmapBuf.data) {
            // Handle non-direct buffer case if needed, or return error/throw
            env->DeleteLocalRef(fmapBufClass);
            env->DeleteLocalRef(jDataBuffer);
            // Optionally throw: env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), "MemxFmapBuf ByteBuffer must be direct");
            return false; // Indicate error
         }
         env->DeleteLocalRef(jDataBuffer); // Clean up local ref
     }

     env->DeleteLocalRef(fmapBufClass); // Clean up local reference
     return true;
 }

// Helper to set MemxFmapBuf fields back (needed for dequeue)
// Note: This assumes the C function might modify idx and potentially size.
// It usually doesn't replace the buffer itself, but fills it.
static inline bool SetMemxFmapBuf(JNIEnv *env, jobject jFmapBuf, const memx_fmap_buf_t& cFmapBuf) {
     if (!jFmapBuf) return false;
     jclass fmapBufClass = env->GetObjectClass(jFmapBuf);
     if (!fmapBufClass) return false;

     jfieldID sizeField = env->GetFieldID(fmapBufClass, "size", "J");
     jfieldID idxField = env->GetFieldID(fmapBufClass, "idx", "J");
     // We typically don't set the ByteBuffer back unless the native layer allocated a new one
     // jfieldID dataField = env->GetFieldID(fmapBufClass, "data", "Ljava/nio/ByteBuffer;");

     if (!sizeField || !idxField /* || !dataField */) {
         env->DeleteLocalRef(fmapBufClass);
         return false; // Field lookup failed
     }

     env->SetLongField(jFmapBuf, sizeField, static_cast<jlong>(cFmapBuf.size));
     env->SetLongField(jFmapBuf, idxField, static_cast<jlong>(cFmapBuf.idx));
     // Setting data buffer back is complex if native allocated it. Assume native only fills existing buffer.

     env->DeleteLocalRef(fmapBufClass);
     return true;
}


// JNI Function Implementations
#ifdef __cplusplus
extern "C" {
#endif 

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1lock(
    JNIEnv *env, jclass clazz, jbyte group_id)
{
    return static_cast<jint>(memx_lock(static_cast<uint8_t>(group_id)));
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1trylock(
    JNIEnv *env, jclass clazz, jbyte group_id)
{
    return static_cast<jint>(memx_trylock(static_cast<uint8_t>(group_id)));
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1unlock(
    JNIEnv *env, jclass clazz, jbyte group_id)
{
    return static_cast<jint>(memx_unlock(static_cast<uint8_t>(group_id)));
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1open(
    JNIEnv *env, jclass clazz, jbyte model_id, jbyte group_id, jfloat chip_gen)
{
    // chip_gen is deprecated in the C header comment, pass it along anyway.
    return static_cast<jint>(memx_open(
        static_cast<uint8_t>(model_id),
        static_cast<uint8_t>(group_id),
        static_cast<float>(chip_gen) // Deprecated, value likely ignored by driver
    ));
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1close(
    JNIEnv *env, jclass clazz, jbyte model_id)
{
    return static_cast<jint>(memx_close(static_cast<uint8_t>(model_id)));
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1operation(
    JNIEnv *env, jclass clazz, jbyte model_id, jint cmd_id, jobject data, jint size)
{
    void* c_data = GetDirectBufferAddressOrThrow(env, data);
    // If GetDirectBufferAddressOrThrow doesn't throw, check for null
    if (size > 0 && !c_data) {
        // Decide: return error code or let GetDirectBufferAddressOrThrow handle it
        return static_cast<jint>(MEMX_STATUS_OTHERS); // Indicate buffer error
    }

    return static_cast<jint>(memx_operation(
        static_cast<uint8_t>(model_id),
        static_cast<uint32_t>(cmd_id),
        c_data,
        static_cast<uint32_t>(size)
    ));
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1download_1model_1wtmem(
    JNIEnv *env, jclass clazz, jbyte model_id, jstring file_path)
{
    JniStringHelper pathHelper(env, file_path);
    const char* c_file_path = pathHelper.getCString();
    if (!c_file_path) {
        return static_cast<jint>(MEMX_STATUS_OTHERS); // Or throw NullPointerException
    }
    return static_cast<jint>(memx_download_model_wtmem(
        static_cast<uint8_t>(model_id),
        c_file_path
    ));
     // pathHelper destructor releases the string
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1download_1model_1config(
    JNIEnv *env, jclass clazz, jbyte model_id, jstring file_path, jbyte model_idx)
{
    JniStringHelper pathHelper(env, file_path);
    const char* c_file_path = pathHelper.getCString();
    if (!c_file_path) {
        return static_cast<jint>(MEMX_STATUS_OTHERS);
    }
    return static_cast<jint>(memx_download_model_config(
        static_cast<uint8_t>(model_id),
        c_file_path,
        static_cast<uint8_t>(model_idx)
    ));
     // pathHelper destructor releases the string
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1download_1model(
    JNIEnv *env, jclass clazz, jbyte model_id, jstring file_path, jbyte model_idx, jint type)
{
    JniStringHelper pathHelper(env, file_path);
    const char* c_file_path = pathHelper.getCString();
     if (!c_file_path && (type & MEMX_DOWNLOAD_TYPE_FROM_BUFFER) == 0) { // Only require path if downloading from file
         return static_cast<jint>(MEMX_STATUS_OTHERS);
     }
    // Note: If type includes MEMX_DOWNLOAD_TYPE_FROM_BUFFER, file_path might contain buffer pointer as string?
    // The C API seems ambiguous here. Assuming file_path is always path for file types.
    // If buffer is used, a different JNI function taking ByteBuffer might be better.
    // Let's assume file_path is only used when MEMX_DOWNLOAD_TYPE_FROM_BUFFER is NOT set.

    return static_cast<jint>(memx_download_model(
        static_cast<uint8_t>(model_id),
        c_file_path, // Pass null if using buffer? API needs clarification.
        static_cast<uint8_t>(model_idx),
        static_cast<int32_t>(type)
    ));
     // pathHelper destructor releases the string
}


JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1download_1firmware(
    JNIEnv *env, jclass clazz, jbyte group_id, jobject data, jbyte type)
{
    memx_status status = MEMX_STATUS_OTHERS;
    uint8_t c_type = static_cast<uint8_t>(type);

    if (c_type == MEMX_DOWNLOAD_TYPE_FROM_FILE) {
        if (!data || !env->IsInstanceOf(data, env->FindClass("java/lang/String"))) {
             return static_cast<jint>(MEMX_STATUS_OTHERS); // Invalid argument
        }
        JniStringHelper pathHelper(env, (jstring)data);
        const char* c_file_path = pathHelper.getCString();
        if (!c_file_path) {
            return static_cast<jint>(MEMX_STATUS_OTHERS);
        }
        status = memx_download_firmware(static_cast<uint8_t>(group_id), c_file_path, c_type);
    } else if (c_type == MEMX_DOWNLOAD_TYPE_FROM_BUFFER) {
        if (!data || !env->IsInstanceOf(data, env->FindClass("java/nio/ByteBuffer"))) {
             return static_cast<jint>(MEMX_STATUS_OTHERS); // Invalid argument
        }
        void* buffer_ptr = GetDirectBufferAddressOrThrow(env, data);
        if (!buffer_ptr) {
             return static_cast<jint>(MEMX_STATUS_OTHERS); // Buffer must be direct
        }
        // The C API takes const char*, which is awkward for a buffer.
        // Assuming it casts internally, or the header is slightly misleading for the buffer case.
        // Passing the buffer pointer directly, cast to const char*.
        status = memx_download_firmware(static_cast<uint8_t>(group_id), static_cast<const char*>(buffer_ptr), c_type);
    } else {
        // Invalid type
        return static_cast<jint>(MEMX_STATUS_OTHERS);
    }

    return static_cast<jint>(status);
}


JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1set_1stream_1enable(
    JNIEnv *env, jclass clazz, jbyte model_id, jint wait)
{
    return static_cast<jint>(memx_set_stream_enable(
        static_cast<uint8_t>(model_id),
        static_cast<int32_t>(wait)
    ));
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1set_1stream_1disable(
    JNIEnv *env, jclass clazz, jbyte model_id, jint wait)
{
    return static_cast<jint>(memx_set_stream_disable(
        static_cast<uint8_t>(model_id),
        static_cast<int32_t>(wait)
    ));
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1set_1ifmap_1queue_1size(
    JNIEnv *env, jclass clazz, jbyte model_id, jint size)
{
    return static_cast<jint>(memx_set_ifmap_queue_size(
        static_cast<uint8_t>(model_id),
        static_cast<int32_t>(size)
    ));
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1set_1ofmap_1queue_1size(
    JNIEnv *env, jclass clazz, jbyte model_id, jint size)
{
    return static_cast<jint>(memx_set_ofmap_queue_size(
        static_cast<uint8_t>(model_id),
        static_cast<int32_t>(size)
    ));
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1get_1ifmap_1size(
    JNIEnv *env, jclass clazz, jbyte model_id, jbyte flow_id,
    jobject heightRef, jobject widthRef, jobject zRef,
    jobject channelNumberRef, jobject formatRef)
{
    int32_t c_height = 0, c_width = 0, c_z = 0, c_channel_number = 0, c_format = 0;
    memx_status status = memx_get_ifmap_size(
        static_cast<uint8_t>(model_id),
        static_cast<uint8_t>(flow_id),
        &c_height, &c_width, &c_z, &c_channel_number, &c_format
    );

    if (memx_status_no_error(status)) {
        SetRefField<int32_t, jint>(env, heightRef, "value", c_height);
        SetRefField<int32_t, jint>(env, widthRef, "value", c_width);
        SetRefField<int32_t, jint>(env, zRef, "value", c_z);
        SetRefField<int32_t, jint>(env, channelNumberRef, "value", c_channel_number);
        SetRefField<int32_t, jint>(env, formatRef, "value", c_format);
    }
    return static_cast<jint>(status);
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1get_1ifmap_1range_1convert(
    JNIEnv *env, jclass clazz, jbyte model_id, jbyte flow_id,
    jobject enableRef, jobject shiftRef, jobject scaleRef)
{
    int32_t c_enable = 0;
    float c_shift = 0.0f, c_scale = 0.0f;
    memx_status status = memx_get_ifmap_range_convert(
        static_cast<uint8_t>(model_id),
        static_cast<uint8_t>(flow_id),
        &c_enable, &c_shift, &c_scale
    );

    if (memx_status_no_error(status)) {
        SetRefField<int32_t, jint>(env, enableRef, "value", c_enable);
        SetRefField<float, jfloat>(env, shiftRef, "value", c_shift);
        SetRefField<float, jfloat>(env, scaleRef, "value", c_scale);
    }
    return static_cast<jint>(status);
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1get_1ofmap_1size(
    JNIEnv *env, jclass clazz, jbyte model_id, jbyte flow_id,
    jobject heightRef, jobject widthRef, jobject zRef,
    jobject channelNumberRef, jobject formatRef)
{
    int32_t c_height = 0, c_width = 0, c_z = 0, c_channel_number = 0, c_format = 0;
    memx_status status = memx_get_ofmap_size(
        static_cast<uint8_t>(model_id),
        static_cast<uint8_t>(flow_id),
        &c_height, &c_width, &c_z, &c_channel_number, &c_format
    );

    if (memx_status_no_error(status)) {
        SetRefField<int32_t, jint>(env, heightRef, "value", c_height);
        SetRefField<int32_t, jint>(env, widthRef, "value", c_width);
        SetRefField<int32_t, jint>(env, zRef, "value", c_z);
        SetRefField<int32_t, jint>(env, channelNumberRef, "value", c_channel_number);
        SetRefField<int32_t, jint>(env, formatRef, "value", c_format);
    }
    return static_cast<jint>(status);
}

JNIEXPORT jobject JNICALL Java_com_memryx_jnilib_MemxJni_memx_1get_1ofmap_1hpoc(
    JNIEnv *env, jclass clazz, jbyte model_id, jbyte flow_id)
{
    int32_t c_hpoc_size = 0;
    int32_t* c_hpoc_indexes = nullptr; // Pointer owned by driver

    memx_status status = memx_get_ofmap_hpoc(
        static_cast<uint8_t>(model_id),
        static_cast<uint8_t>(flow_id),
        &c_hpoc_size,
        &c_hpoc_indexes
    );

    // Find the HpocResult class and its constructor HpocResult()
    jclass resultClass = env->FindClass("com/memryx/jnilib/MemxHeader$HpocResult");
    if (!resultClass) return nullptr; // Error
    jmethodID constructor = env->GetMethodID(resultClass, "<init>", "()V");
    if (!constructor) return nullptr; // Error

    // Create a new HpocResult object
    jobject resultObj = env->NewObject(resultClass, constructor);
    if (!resultObj) return nullptr; // Error

    // Find the fields to set
    jfieldID statusField = env->GetFieldID(resultClass, "status", "I");
    jfieldID sizeField = env->GetFieldID(resultClass, "hpocSize", "I");
    jfieldID indexesField = env->GetFieldID(resultClass, "hpocIndexes", "[I"); // int[] -> [I

    if (!statusField || !sizeField || !indexesField) return nullptr; // Error finding fields

    // Set the status field
    env->SetIntField(resultObj, statusField, static_cast<jint>(status));
    env->SetIntField(resultObj, sizeField, 0); // Default size to 0
    env->SetObjectField(resultObj, indexesField, nullptr); // Default indexes to null


    // If the call was successful and there are indexes, copy them
    if (memx_status_no_error(status) && c_hpoc_size > 0 && c_hpoc_indexes != nullptr) {
        env->SetIntField(resultObj, sizeField, static_cast<jint>(c_hpoc_size));

        // Create a new Java int array
        jintArray jIndexes = env->NewIntArray(static_cast<jsize>(c_hpoc_size));
        if (jIndexes) {
            // Copy data from C array to Java array
            // Note: C array uses int32_t, Java int is usually 32-bit. Direct copy safe.
            env->SetIntArrayRegion(jIndexes, 0, static_cast<jsize>(c_hpoc_size), (const jint*)c_hpoc_indexes);
            // Set the indexes field in the result object
            env->SetObjectField(resultObj, indexesField, jIndexes);
            // Clean up the local reference to the new array if no longer needed immediately
            // env->DeleteLocalRef(jIndexes); // Not needed here as it's being returned within the object
        } else {
            // Failed to allocate Java array, maybe set status to error?
             env->SetIntField(resultObj, statusField, static_cast<jint>(MEMX_STATUS_OTHERS)); // Indicate allocation failure
        }
    }

    return resultObj;
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1operation_1get_1device_1count(
    JNIEnv *env, jclass clazz, jobject countRef)
{
    // Assuming the C function expects a pointer to uint32_t or similar
    uint32_t device_count = 0; // Use appropriate type based on API expectation
    memx_status status = memx_operation_get_device_count(static_cast<void*>(&device_count));

    if (memx_status_no_error(status)) {
        SetRefField<int32_t, jint>(env, countRef, "value", static_cast<int32_t>(device_count));
    }
    return static_cast<jint>(status);
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1operation_1get_1mpu_1group_1count(
    JNIEnv *env, jclass clazz, jbyte group_id, jobject countRef)
{
     // Assuming the C function expects a pointer to uint32_t or similar
    uint32_t group_count = 0; // Use appropriate type based on API expectation
    memx_status status = memx_operation_get_mpu_group_count(
        static_cast<uint8_t>(group_id),
        static_cast<void*>(&group_count)
    );

    if (memx_status_no_error(status)) {
         SetRefField<int32_t, jint>(env, countRef, "value", static_cast<int32_t>(group_count));
    }
    return static_cast<jint>(status);
}


JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1stream_1ifmap(
    JNIEnv *env, jclass clazz, jbyte model_id, jbyte flow_id, jobject ifmap, jint timeout)
{
    void* c_ifmap = GetDirectBufferAddressOrThrow(env, ifmap);
     if (!c_ifmap) {
        return static_cast<jint>(MEMX_STATUS_OTHERS); // Indicate buffer error
    }
    return static_cast<jint>(memx_stream_ifmap(
        static_cast<uint8_t>(model_id),
        static_cast<uint8_t>(flow_id),
        c_ifmap,
        static_cast<int32_t>(timeout)
    ));
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1stream_1ofmap(
    JNIEnv *env, jclass clazz, jbyte model_id, jbyte flow_id, jobject ofmap, jint timeout)
{
    void* c_ofmap = GetDirectBufferAddressOrThrow(env, ofmap);
     if (!c_ofmap) {
        return static_cast<jint>(MEMX_STATUS_OTHERS); // Indicate buffer error
    }
    return static_cast<jint>(memx_stream_ofmap(
        static_cast<uint8_t>(model_id),
        static_cast<uint8_t>(flow_id),
        c_ofmap, // C function will write into this buffer
        static_cast<int32_t>(timeout)
    ));
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1config_1mpu_1group(
    JNIEnv *env, jclass clazz, jbyte group_id, jbyte mpu_group_config)
{
    return static_cast<jint>(memx_config_mpu_group(
        static_cast<uint8_t>(group_id),
        static_cast<uint8_t>(mpu_group_config)
    ));
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1get_1chip_1gen(
    JNIEnv *env, jclass clazz, jbyte model_id, jobject chipGenRef)
{
    uint8_t c_chip_gen = 0;
    memx_status status = memx_get_chip_gen(
        static_cast<uint8_t>(model_id),
        &c_chip_gen
    );

    if (memx_status_no_error(status)) {
        SetRefField<int8_t, jbyte>(env, chipGenRef, "value", static_cast<int8_t>(c_chip_gen));
    }
    return static_cast<jint>(status);
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1set_1powerstate(
    JNIEnv *env, jclass clazz, jbyte model_id, jbyte state)
{
    return static_cast<jint>(memx_set_powerstate(
        static_cast<uint8_t>(model_id),
        static_cast<uint8_t>(state)
    ));
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1enter_1device_1deep_1sleep(
    JNIEnv *env, jclass clazz, jbyte group_id)
{
    return static_cast<jint>(memx_enter_device_deep_sleep(static_cast<uint8_t>(group_id)));
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1exit_1device_1deep_1sleep(
    JNIEnv *env, jclass clazz, jbyte group_id)
{
    return static_cast<jint>(memx_exit_device_deep_sleep(static_cast<uint8_t>(group_id)));
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1get_1total_1chip_1count(
    JNIEnv *env, jclass clazz, jbyte group_id, jobject chipCountRef)
{
    uint8_t c_chip_count = 0;
    memx_status status = memx_get_total_chip_count(
        static_cast<uint8_t>(group_id),
        &c_chip_count
    );

    if (memx_status_no_error(status)) {
        SetRefField<int8_t, jbyte>(env, chipCountRef, "value", static_cast<int8_t>(c_chip_count));
    }
    return static_cast<jint>(status);
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1get_1feature(
    JNIEnv *env, jclass clazz, jbyte group_id, jbyte chip_id, jint opcode, jobject buffer)
{
    void* c_buffer = GetDirectBufferAddressOrThrow(env, buffer);
     if (!c_buffer) {
        // Buffer required for output
        return static_cast<jint>(MEMX_STATUS_OTHERS);
    }

    // Need to know buffer size expected by opcode. Java side must allocate sufficiently.
    // The C API doesn't specify size, which is problematic. Assuming Java pre-allocates.
    return static_cast<jint>(memx_get_feature(
        static_cast<uint8_t>(group_id),
        static_cast<uint8_t>(chip_id),
        static_cast<memx_get_feature_opcode>(opcode), // Enum value passed as int
        c_buffer // C function will write into this buffer
    ));
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1set_1feature(
    JNIEnv *env, jclass clazz, jbyte group_id, jbyte chip_id, jint opcode, jshort parameter)
{
    return static_cast<jint>(memx_set_feature(
        static_cast<uint8_t>(group_id),
        static_cast<uint8_t>(chip_id),
        static_cast<memx_set_feature_opcode>(opcode), // Enum value passed as int
        static_cast<uint16_t>(parameter) // jshort (16-bit signed) -> uint16_t
    ));
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1enqueue_1ifmap_1buf(
    JNIEnv *env, jclass clazz, jbyte model_id, jbyte flow_id, jobject fmap_buf, jint timeout)
{
    memx_fmap_buf_t c_fmap_buf;
    if (!GetMemxFmapBuf(env, fmap_buf, c_fmap_buf)) {
        return static_cast<jint>(MEMX_STATUS_OTHERS); // Error getting struct data
    }
    // Ensure buffer pointer is valid if size > 0
    if (c_fmap_buf.size > 0 && !c_fmap_buf.data) {
         return static_cast<jint>(MEMX_STATUS_OTHERS); // Invalid buffer in struct
    }

    return static_cast<jint>(memx_enqueue_ifmap_buf(
        static_cast<uint8_t>(model_id),
        static_cast<uint8_t>(flow_id),
        &c_fmap_buf, // Pass pointer to the C struct
        static_cast<int32_t>(timeout)
    ));
    // Note: C function might modify the struct pointed to by c_fmap_buf pointer,
    // but we usually don't need to propagate those changes back for enqueue input.
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1enqueue_1ofmap_1buf(
    JNIEnv *env, jclass clazz, jbyte model_id, jbyte flow_id, jobject fmap_buf, jint timeout)
{
    memx_fmap_buf_t c_fmap_buf;
     if (!GetMemxFmapBuf(env, fmap_buf, c_fmap_buf)) {
        return static_cast<jint>(MEMX_STATUS_OTHERS); // Error getting struct data
    }
     // Ensure buffer pointer is valid if size > 0 (needed for output)
    if (c_fmap_buf.size > 0 && !c_fmap_buf.data) {
         return static_cast<jint>(MEMX_STATUS_OTHERS); // Invalid buffer in struct
    }

    // This call prepares an output buffer. The C layer likely just registers it.
    return static_cast<jint>(memx_enqueue_ofmap_buf(
        static_cast<uint8_t>(model_id),
        static_cast<uint8_t>(flow_id),
        &c_fmap_buf, // Pass pointer to the C struct
        static_cast<int32_t>(timeout)
    ));
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1dequeue_1ifmap_1buf(
    JNIEnv *env, jclass clazz, jbyte model_id, jbyte flow_id, jobject fmap_buf, jint timeout)
{
    memx_fmap_buf_t c_fmap_buf;
    // For dequeue, the C function will *fill* the struct.
    // We pass a pointer to our local C struct.
    memset(&c_fmap_buf, 0, sizeof(c_fmap_buf)); // Initialize C struct

    memx_status status = memx_dequeue_ifmap_buf(
        static_cast<uint8_t>(model_id),
        static_cast<uint8_t>(flow_id),
        &c_fmap_buf, // C function fills this
        static_cast<int32_t>(timeout)
    );

    // If successful, update the Java object
    if (memx_status_no_error(status)) {
        if (!SetMemxFmapBuf(env, fmap_buf, c_fmap_buf)) {
             // Failed to set fields back, indicate potential issue
             // return static_cast<jint>(MEMX_STATUS_OTHERS); // Optional: override status
        }
        // Note: The C struct's 'data' pointer might now point somewhere.
        // The current SetMemxFmapBuf doesn't update the Java ByteBuffer object itself,
        // which is usually correct as dequeue typically reuses buffers provided via enqueue.
        // If dequeue *allocates* new buffers, the JNI layer needs significant changes.
    }

    return static_cast<jint>(status);
}

JNIEXPORT jint JNICALL Java_com_memryx_jnilib_MemxJni_memx_1dequeue_1ofmap_1buf(
    JNIEnv *env, jclass clazz, jbyte model_id, jbyte flow_id, jobject fmap_buf, jint timeout)
{
    memx_fmap_buf_t c_fmap_buf;
    memset(&c_fmap_buf, 0, sizeof(c_fmap_buf)); // Initialize C struct

    memx_status status = memx_dequeue_ofmap_buf(
        static_cast<uint8_t>(model_id),
        static_cast<uint8_t>(flow_id),
        &c_fmap_buf, // C function fills this
        static_cast<int32_t>(timeout)
    );

    // If successful, update the Java object
    if (memx_status_no_error(status)) {
         if (!SetMemxFmapBuf(env, fmap_buf, c_fmap_buf)) {
             // Failed to set fields back
             // return static_cast<jint>(MEMX_STATUS_OTHERS); // Optional: override status
         }
         // See comment in dequeue_ifmap_buf regarding the 'data' pointer.
         // Assumes dequeue populates a buffer previously provided via enqueue_ofmap_buf.
    }

    return static_cast<jint>(status);
}


// Helper to convert C++ std::vector<int32_t> to Java jintArray
jintArray vectorToJavaIntArray(JNIEnv* env, const std::vector<int32_t>& vec) {
    if (!env) return nullptr;
    jintArray out = env->NewIntArray(vec.size());
    if (out == nullptr) {
        ALOGE("vectorToJavaIntArray: Failed to allocate Java int array.");
        return nullptr;
    }
    // Use const_cast if SetIntArrayRegion expects non-const pointer, though usually okay with const data()
    env->SetIntArrayRegion(out, 0, JSIZE_CAST(vec.size()), JINT_PTR(vec.data()));
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

JNIEXPORT jobject JNICALL
Java_com_memryx_jnilib_MemxJni_loadDfpInfo(
    JNIEnv* env,
    jclass clazz MAYBE_UNUSED,
    jstring dfpPath) {

    const char* pathChars = env->GetStringUTFChars(dfpPath, nullptr);
    if (!pathChars) {
        ALOGE("loadDfpInfo: Failed to get path string chars");
        return nullptr;
    }
    std::string pathStr = pathChars;
    env->ReleaseStringUTFChars(dfpPath, pathChars); // Release as soon as copied

    ALOGD("loadDfpInfo: Attempting to load DFP from %s", pathStr.c_str());

    // Use unique_ptr for exception safety with DfpObject allocation
    std::unique_ptr<Dfp::DfpObject> dfp;
    try {
         dfp = std::make_unique<Dfp::DfpObject>(pathStr);
         // Check validity after construction
         if (!dfp) {
              ALOGE("loadDfpInfo: Failed to load or invalid DFP object for path: %s", pathStr.c_str());
              // DfpObject constructor might print its own error via printf
              return nullptr;
         }
    } catch (const std::exception& e) {
         ALOGE("loadDfpInfo: Exception during DfpObject creation for %s: %s", pathStr.c_str(), e.what());
         return nullptr;
    } catch (...) {
         ALOGE("loadDfpInfo: Unknown exception during DfpObject creation for %s", pathStr.c_str());
         return nullptr;
    }

    // --- Create and Populate Java DfpInfo object ---
    jclass dfpInfoClass = env->FindClass("com/memryx/jnilib/MemxJni$DfpInfo");
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
    jclass portInfoClass = env->FindClass("com/memryx/jnilib/MemxJni$PortInfo");
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
             ALOGW("loadDfpInfo: Got null for input port %d", i);
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
             ALOGW("loadDfpInfo: Got null for output port %d", i);
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

    ALOGD("loadDfpInfo: Successfully loaded and parsed DFP: %s", pathStr.c_str());
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


JNIEXPORT jint JNICALL
Java_com_memryx_jnilib_MemxJni_calculateFormattedSize(
    JNIEnv* env, jclass clazz, jint floatCount, jint format, jint h, jint w, jint z, jint c) {

    ALOGD("calculateFormattedSize(floatCount=%d, format=%d, h=%d, w=%d, z=%d, c=%d)",
          floatCount, format, h, w, z, c);

    // Call the internal calculation logic
    uint32_t calculated_size = calculateFormattedSizeInternal(floatCount, format, h, w, z, c);

    if (calculated_size == 0 && format != 2) { // Allow 0 size only if format might be weird like RGB888
         ALOGE("calculateFormattedSize: Calculated size is zero for format %d", format);
         return -1; // Indicate error
    }

    ALOGD("calculateFormattedSize: Calculated size = %u", calculated_size);
    return static_cast<jint>(calculated_size);
}


// JNI wrapper for C++ convert_data logic
JNIEXPORT jboolean JNICALL
Java_com_memryx_jnilib_MemxJni_convertFloatToFormattedBytes(
    JNIEnv* env, jclass clazz, jfloatArray inputFloatData, jbyteArray outputFormattedBytes,
    jint h, jint w, jint z, jint c, jint format) {

    jfloat* inputFloats = env->GetFloatArrayElements(inputFloatData, nullptr);
    jbyte* outputBytes = env->GetByteArrayElements(outputFormattedBytes, nullptr);
    jsize inputLen = env->GetArrayLength(inputFloatData); // Number of floats
    // jsize outputByteLen = env->GetArrayLength(outputFormattedBytes); // Size in bytes

    ALOGD("convertFloatToFormattedBytes(format=%d, h=%d, w=%d, z=%d, c=%d, inputLen=%d)",
          format, h, w, z, c, (int)inputLen);

    if (!inputFloats || !outputBytes) {
        ALOGE("convertFloatToFormattedBytes: Failed to get array elements.");
        if (inputFloats) env->ReleaseFloatArrayElements(inputFloatData, inputFloats, JNI_ABORT);
        if (outputBytes) env->ReleaseByteArrayElements(outputFormattedBytes, outputBytes, JNI_ABORT);
        return JNI_FALSE;
    }

    bool success = true;
    try {
        // Call the appropriate C++ conversion function based on format
        switch(format) {
            case 2: // MX_FMT_RGB888 - Needs specific conversion logic if input is float
                 ALOGE("convertFloatToFormattedBytes: Conversion for RGB888 from float not implemented.");
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
                 ALOGE("convertFloatToFormattedBytes: Invalid format code %d", format);
                 success = false;
                 break;
        }
    } catch (const std::exception& e) {
        ALOGE("convertFloatToFormattedBytes: Exception during conversion: %s", e.what());
        success = false;
    } catch (...) {
         ALOGE("convertFloatToFormattedBytes: Unknown exception during conversion");
         success = false;
    }

    // Release arrays. Mode 0 copies back changes for outputBytes (which we wrote to).
    env->ReleaseFloatArrayElements(inputFloatData, inputFloats, JNI_ABORT); // Input buffer wasn't changed
    env->ReleaseByteArrayElements(outputFormattedBytes, outputBytes, success ? 0 : JNI_ABORT); // Copy back if successful

    ALOGD("convertFloatToFormattedBytes finished (success=%d)", success);
    return success ? JNI_TRUE : JNI_FALSE;
}

// JNI wrapper for C++ unconvert_data logic
JNIEXPORT jboolean JNICALL
Java_com_memryx_jnilib_MemxJni_unconvertFormattedBytesToFloat(
    JNIEnv* env, jclass clazz, jbyteArray inputFormattedBytes, jfloatArray outputFloatData,
    jint h, jint w, jint z, jint c, jint format,
    jboolean hpocEnabled, jint hpocSize, jintArray hpocIndices, jboolean rowPad) {

    jbyte* inputBytes = env->GetByteArrayElements(inputFormattedBytes, nullptr);
    jfloat* outputFloats = env->GetFloatArrayElements(outputFloatData, nullptr);
    // jsize inputByteLen = env->GetArrayLength(inputFormattedBytes);
    jsize outputFloatLen = env->GetArrayLength(outputFloatData); // Number of floats

    ALOGD("unconvertFormattedBytesToFloat(format=%d, h=%d, w=%d, z=%d, c=%d, outputFloatLen=%d, hpoc=%d)",
          format, h, w, z, c, (int)outputFloatLen, (int)hpocEnabled);

    if (!inputBytes || !outputFloats) {
         ALOGE("unconvertFormattedBytesToFloat: Failed to get array elements.");
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
                 ALOGD("unconvertFormattedBytesToFloat: Using HPOC indices (count=%d, size=%d)", (int)hpocLen, hpocSize);
             } else {
                  ALOGW("unconvertFormattedBytesToFloat: Failed to convert HPOC indices, disabling HPOC.");
                  hpocEnabled = JNI_FALSE;
             }
        } else {
             ALOGW("unconvertFormattedBytesToFloat: HPOC enabled but index array size (%d) or hpocSize (%d) is zero.", (int)hpocLen, hpocSize);
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
                 ALOGE("unconvertFormattedBytesToFloat: Unconversion for RGB888 not implemented.");
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
                 ALOGE("unconvertFormattedBytesToFloat: Invalid format code %d", format);
                 success = false;
                 break;
        }
    } catch (const std::exception& e) {
        ALOGE("unconvertFormattedBytesToFloat: Exception during unconversion: %s", e.what());
        success = false;
    } catch (...) {
         ALOGE("unconvertFormattedBytesToFloat: Unknown exception during unconversion");
         success = false;
    }

    // Release arrays. Mode 0 copies back changes for outputFloats.
    env->ReleaseByteArrayElements(inputFormattedBytes, inputBytes, JNI_ABORT); // Input buffer wasn't changed
    env->ReleaseFloatArrayElements(outputFloatData, outputFloats, success ? 0 : JNI_ABORT); // Copy back results if successful

    ALOGD("unconvertFormattedBytesToFloat finished (success=%d)", success);
    return success ? JNI_TRUE : JNI_FALSE;
}

#ifdef __cplusplus
} // extern "C"
#endif