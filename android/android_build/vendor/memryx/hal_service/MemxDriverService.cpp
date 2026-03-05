#define LOG_TAG "MemxDriverService" // Log tag for this specific service

#include <vector>
#include <string>
#include <cstdint>
#include <utils/Log.h>
#include <utils/Errors.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>

// Include NDK Binder headers for ScopedAStatus and exception codes
#include <android/binder_process.h>
#include <android/binder_status.h>
#include <android/binder_manager.h>
#include <android/binder_ibinder.h> // For asBinder()
#include <android/binder_interface_utils.h> // For ndk::SharedRefBase

#include <aidl/com/memryx/memxdriver/BnMemxDriverService.h>
#include <aidl/com/memryx/memxdriver/BpMemxDriverService.h>

#include "memx.h"

using android::OK;
using android::status_t;
// using android::binder::Status; // No longer needed for return types
using ::ndk::ScopedAStatus; // Use NDK ScopedAStatus

// Use the fully qualified namespace for the AIDL interface classes
using aidl::com::memryx::memxdriver::BnMemxDriverService;
using aidl::com::memryx::memxdriver::IMemxDriverService;

constexpr int32_t HAL_SUCCESS_CODE = 0;

class MemxDriverService : public BnMemxDriverService {
public:
    MemxDriverService() {
        ALOGI("MemxDriverService instance created (Lazy Instantiation).");
    }
    virtual ~MemxDriverService() {
        ALOGI("MemxDriverService instance destroyed.");
    }

    // --- AIDL Method Implementations with CORRECT return type ---

    ScopedAStatus memxLock(int32_t group_id, int32_t* _aidl_return) override {
        ALOGD("Service::memxLock(group_id=%d) called", group_id);
        if (!_aidl_return) {
             ALOGE("Service::memxLock: _aidl_return is null");
             return ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
        }
        *_aidl_return = memx_lock(static_cast<uint8_t>(group_id));
        if (*_aidl_return != HAL_SUCCESS_CODE) {
            ALOGE("Service::memxLock: memx_lock failed with status %d", *_aidl_return);
            return ScopedAStatus::fromServiceSpecificError(*_aidl_return);
        }
        ALOGD("Service::memxLock(group_id=%d) returning OK (status=%d)", group_id, *_aidl_return);
        return ScopedAStatus::ok(); // Use NDK ok()
    }

    ScopedAStatus memxTrylock(int32_t group_id, int32_t* _aidl_return) override {
        ALOGD("Service::memxTrylock(group_id=%d) called", group_id);
         if (!_aidl_return) {
             ALOGE("Service::memxTrylock: _aidl_return is null");
             return ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
        }
        *_aidl_return = memx_trylock(static_cast<uint8_t>(group_id));
        if (*_aidl_return != HAL_SUCCESS_CODE) {
             ALOGW("Service::memxTrylock: memx_trylock returned status %d (might be busy or error)", *_aidl_return);
             // Decide if this non-zero return should be a Binder error
             // if (*_aidl_return < 0) { // Example: only treat negative as error
             //    return ScopedAStatus::fromServiceSpecificError(*_aidl_return);
             // }
        }
        ALOGD("Service::memxTrylock(group_id=%d) returning status %d", group_id, *_aidl_return);
        return ScopedAStatus::ok(); // Return OK even if trylock failed
    }

    ScopedAStatus memxUnlock(int32_t group_id, int32_t* _aidl_return) override {
        ALOGD("Service::memxUnlock(group_id=%d) called", group_id);
        if (!_aidl_return) {
             ALOGE("Service::memxUnlock: _aidl_return is null");
             return ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
        }
        *_aidl_return = memx_unlock(static_cast<uint8_t>(group_id));
        if (*_aidl_return != HAL_SUCCESS_CODE) {
            ALOGE("Service::memxUnlock: memx_unlock failed with status %d", *_aidl_return);
            return ScopedAStatus::fromServiceSpecificError(*_aidl_return);
        }
        ALOGD("Service::memxUnlock(group_id=%d) returning OK (status=%d)", group_id, *_aidl_return);
        return ScopedAStatus::ok();
    }

    ScopedAStatus memxOpen(int32_t model_id, int32_t group_id, float chip_gen, int32_t* _aidl_return) override {
        ALOGD("Service::memxOpen(model_id=%d, group_id=%d, chip_gen=%.2f) called", model_id, group_id, chip_gen);
         if (!_aidl_return) {
             ALOGE("Service::memxOpen: _aidl_return is null");
             return ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
        }
        *_aidl_return = memx_open(static_cast<uint8_t>(model_id),
                                  static_cast<uint8_t>(group_id),
                                  chip_gen);
        if (*_aidl_return != HAL_SUCCESS_CODE) {
            ALOGE("Service::memxOpen: memx_open failed with status %d", *_aidl_return);
            return ScopedAStatus::fromServiceSpecificError(*_aidl_return);
        }
        ALOGD("Service::memxOpen(model_id=%d, ...) returning OK (status=%d)", model_id, *_aidl_return);
        return ScopedAStatus::ok();
    }

    ScopedAStatus memxClose(int32_t model_id, int32_t* _aidl_return) override {
        ALOGD("Service::memxClose(model_id=%d) called", model_id);
         if (!_aidl_return) {
             ALOGE("Service::memxClose: _aidl_return is null");
             return ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
        }
        *_aidl_return = memx_close(static_cast<uint8_t>(model_id));
         if (*_aidl_return != HAL_SUCCESS_CODE) {
            ALOGE("Service::memxClose: memx_close failed with status %d", *_aidl_return);
            return ScopedAStatus::fromServiceSpecificError(*_aidl_return);
        }
        ALOGD("Service::memxClose(model_id=%d) returning OK (status=%d)", model_id, *_aidl_return);
        return ScopedAStatus::ok();
    }

    ScopedAStatus memxConfigMpuGroup(int32_t group_id, int32_t config, int32_t* _aidl_return) override {
        ALOGD("Service::memxConfigMpuGroup(group_id=%d, config=%d) called", group_id, config);
         if (!_aidl_return) {
             ALOGE("Service::memxConfigMpuGroup: _aidl_return is null");
             return ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
        }
        *_aidl_return = memx_config_mpu_group(static_cast<uint8_t>(group_id),
                                              static_cast<uint8_t>(config));
         if (*_aidl_return != HAL_SUCCESS_CODE) {
            ALOGE("Service::memxConfigMpuGroup: memx_config_mpu_group failed with status %d", *_aidl_return);
            return ScopedAStatus::fromServiceSpecificError(*_aidl_return);
        }
        ALOGD("Service::memxConfigMpuGroup(group_id=%d, ...) returning OK (status=%d)", group_id, *_aidl_return);
        return ScopedAStatus::ok();
    }

    ScopedAStatus memxGetChipGen(int32_t model_id, std::vector<uint8_t>* _aidl_return) override {
        ALOGD("Service::memxGetChipGen(model_id=%d) called", model_id);
        if (!_aidl_return) {
            ALOGE("Service::memxGetChipGen: Output vector pointer (_aidl_return) is null!");
            return ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
        }
        constexpr size_t CHIP_GEN_SIZE = 4;
        _aidl_return->resize(CHIP_GEN_SIZE);
        int ret = memx_get_chip_gen(static_cast<uint8_t>(model_id), _aidl_return->data());
        if (ret != HAL_SUCCESS_CODE) {
             ALOGE("Service::memxGetChipGen: memx_get_chip_gen failed with status %d", ret);
             _aidl_return->clear();
            return ScopedAStatus::fromServiceSpecificError(ret);
        }
        ALOGD("Service::memxGetChipGen(model_id=%d) returning OK (size=%zu)", model_id, _aidl_return->size());
        return ScopedAStatus::ok();
    }

    // Ensure parameter type matches the generated header (const std::string&)
    ScopedAStatus memxDownloadModel(int32_t model_id,
                             const std::string& filePath, // NDK maps 'in String' to this
                             int32_t model_idx,
                             int32_t type,
                             int32_t* _aidl_return) override {
        ALOGD("Service::memxDownloadModel(model_id=%d, path=\"%s\", idx=%d, type=%d) called",
              model_id, filePath.c_str(), model_idx, type);
        if (!_aidl_return) {
             ALOGE("Service::memxDownloadModel: _aidl_return is null");
             return ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
        }
        *_aidl_return = memx_download_model(static_cast<uint8_t>(model_id),
                                            filePath.c_str(),
                                            static_cast<uint8_t>(model_idx),
                                            type);
        if (*_aidl_return != HAL_SUCCESS_CODE) {
            ALOGE("Service::memxDownloadModel: memx_download_model failed for path '%s' with status %d", filePath.c_str(), *_aidl_return);
            return ScopedAStatus::fromServiceSpecificError(*_aidl_return);
        }
        ALOGD("Service::memxDownloadModel(model_id=%d, ...) returning OK (status=%d)", model_id, *_aidl_return);
        return ScopedAStatus::ok();
    }

    ScopedAStatus memxSetStreamEnable(int32_t model_id, int32_t wait, int32_t* _aidl_return) override {
        ALOGD("Service::memxSetStreamEnable(model_id=%d, wait=%d) called", model_id, wait);
        if (!_aidl_return) {
             ALOGE("Service::memxSetStreamEnable: _aidl_return is null");
             return ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
        }
        *_aidl_return = memx_set_stream_enable(static_cast<uint8_t>(model_id), wait);
        if (*_aidl_return != HAL_SUCCESS_CODE) {
            ALOGE("Service::memxSetStreamEnable: memx_set_stream_enable failed with status %d", *_aidl_return);
            return ScopedAStatus::fromServiceSpecificError(*_aidl_return);
        }
        ALOGD("Service::memxSetStreamEnable(model_id=%d, ...) returning OK (status=%d)", model_id, *_aidl_return);
        return ScopedAStatus::ok();
    }

    ScopedAStatus memxSetStreamDisable(int32_t model_id, int32_t wait, int32_t* _aidl_return) override {
        ALOGD("Service::memxSetStreamDisable(model_id=%d, wait=%d) called", model_id, wait);
        if (!_aidl_return) {
             ALOGE("Service::memxSetStreamDisable: _aidl_return is null");
             return ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
        }
        *_aidl_return = memx_set_stream_disable(static_cast<uint8_t>(model_id), wait);
        if (*_aidl_return != HAL_SUCCESS_CODE) {
            ALOGE("Service::memxSetStreamDisable: memx_set_stream_disable failed with status %d", *_aidl_return);
            return ScopedAStatus::fromServiceSpecificError(*_aidl_return);
        }
        ALOGD("Service::memxSetStreamDisable(model_id=%d, ...) returning OK (status=%d)", model_id, *_aidl_return);
        return ScopedAStatus::ok();
    }

    ScopedAStatus memxGetIfmapSize(int32_t model_id,
                            int32_t flow_id,
                            std::vector<int32_t>* _aidl_return) override {
        ALOGD("Service::memxGetIfmapSize(model_id=%d, flow_id=%d) called", model_id, flow_id);
        if (!_aidl_return) {
            ALOGE("Service::memxGetIfmapSize: Output vector pointer (_aidl_return) is null!");
            return ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
        }
        int32_t h = 0, w = 0, z = 0, c = 0, fmt = 0;
        int ret = memx_get_ifmap_size(static_cast<uint8_t>(model_id),
                                      static_cast<uint8_t>(flow_id),
                                      &h, &w, &z, &c, &fmt);
        if (ret != HAL_SUCCESS_CODE) {
             ALOGE("Service::memxGetIfmapSize: memx_get_ifmap_size failed with status %d", ret);
             _aidl_return->clear();
            return ScopedAStatus::fromServiceSpecificError(ret);
        }
        _aidl_return->assign({h, w, z, c, fmt});
        ALOGD("Service::memxGetIfmapSize(model_id=%d, ...) returning OK (size=%zu)", model_id, _aidl_return->size());
        return ScopedAStatus::ok();
    }

    ScopedAStatus memxGetOfmapSize(int32_t model_id,
                            int32_t flow_id,
                            std::vector<int32_t>* _aidl_return) override {
        ALOGD("Service::memxGetOfmapSize(model_id=%d, flow_id=%d) called", model_id, flow_id);
         if (!_aidl_return) {
            ALOGE("Service::memxGetOfmapSize: Output vector pointer (_aidl_return) is null!");
            return ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
        }
        int32_t h = 0, w = 0, z = 0, c = 0, fmt = 0;
        int ret = memx_get_ofmap_size(static_cast<uint8_t>(model_id),
                                      static_cast<uint8_t>(flow_id),
                                      &h, &w, &z, &c, &fmt);
        if (ret != HAL_SUCCESS_CODE) {
             ALOGE("Service::memxGetOfmapSize: memx_get_ofmap_size failed with status %d", ret);
             _aidl_return->clear();
            return ScopedAStatus::fromServiceSpecificError(ret);
        }
        _aidl_return->assign({h, w, z, c, fmt});
        ALOGD("Service::memxGetOfmapSize(model_id=%d, ...) returning OK (size=%zu)", model_id, _aidl_return->size());
        return ScopedAStatus::ok();
    }

    ScopedAStatus memxStreamIfmap(int32_t model_id,
                           int32_t flow_id,
                           const std::vector<uint8_t>& data,
                           int32_t timeout,
                           int32_t* _aidl_return) override {
        ALOGD("Service::memxStreamIfmap(model_id=%d, flow_id=%d, data_size=%zu, timeout=%d) called",
              model_id, flow_id, data.size(), timeout);
        if (!_aidl_return) {
             ALOGE("Service::memxStreamIfmap: _aidl_return is null");
             return ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
        }
        void* buf = const_cast<uint8_t*>(data.data());
        *_aidl_return = memx_stream_ifmap(static_cast<uint8_t>(model_id),
                                          static_cast<uint8_t>(flow_id),
                                          buf,
                                          timeout);
        if (*_aidl_return != HAL_SUCCESS_CODE) {
            ALOGE("Service::memxStreamIfmap: memx_stream_ifmap failed with status %d", *_aidl_return);
            return ScopedAStatus::fromServiceSpecificError(*_aidl_return);
        }
        ALOGD("Service::memxStreamIfmap(model_id=%d, ...) returning OK (status=%d)", model_id, *_aidl_return);
        return ScopedAStatus::ok();
    }

    ScopedAStatus memxStreamOfmap(int32_t model_id,
                           int32_t flow_id,
                           std::vector<uint8_t>* data,
                           int32_t timeout,
                           int32_t* _aidl_return) override {
         if (!_aidl_return) {
             ALOGE("Service::memxStreamOfmap: _aidl_return is null");
             return ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
        }
        if (!data) {
            ALOGE("Service::memxStreamOfmap: Input/Output vector pointer (data) is null!");
            return ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
        }

        int32_t height = 0, width = 0, z = 0, channel_number = 0, format = 0;
        int32_t sizeStatus = memx_get_ofmap_size(
            static_cast<uint8_t>(model_id),
            static_cast<uint8_t>(flow_id),
            &height,
            &width,
            &z,
            &channel_number,
            &format
        );
        if (sizeStatus != HAL_SUCCESS_CODE) {
            ALOGE("memxStreamOfmap: memx_get_ofmap_size failed (status=%d)", sizeStatus);
            return ScopedAStatus::fromServiceSpecificError(sizeStatus);
        }

        size_t bufferSize = 0;
        switch(format)
        {
            case 2: //MX_FMT_RGB888
            bufferSize = height * width * z * channel_number;
                break;
            case 5: //MX_FMT_FP32
            bufferSize = height * width * z * channel_number * 4;
                break;
            case 4: //MX_FMT_BF16
            bufferSize = height * width * z * channel_number * 2;
                if (bufferSize % 2 )
                bufferSize += 2;
                break;
            case 0: { //MX_FMT_GBF80
                size_t num_xyz_pixels = (height * width * z );
                bool any_remainder_chs = ((channel_number % 8) != 0);
                size_t num_gbf_words = (channel_number / 8) + (any_remainder_chs ? 1 : 0);
                bufferSize = (uint32_t)(num_xyz_pixels * num_gbf_words * 10);
                break;
            }
            case 6: { //MX_FMT_GBF80_ROW
                bool any_remainder_chs = ((channel_number % 8) != 0);
                size_t num_gbf_words = (channel_number / 8) + (any_remainder_chs ? 1 : 0);
                // padding row size to 4 bytes-alignment
                bufferSize = height * ((width * z * num_gbf_words * 10 + 3) & ~0x3);
                break;
            }
            default:
                return ScopedAStatus::fromServiceSpecificError(format);
                break;
        }
        ALOGD("memxStreamOfmap: h=%d, w=%d, z=%d, ch=%d, fmt=%d, size=%zu", height, width, z, channel_number, format, bufferSize);

        if (bufferSize == 0) {
            ALOGE("memxStreamOfmap: computed bufferSize is zero!");
            return ScopedAStatus::fromServiceSpecificError(-EINVAL);
        }
        data->resize(bufferSize);
        void* buf = data->data();

        int32_t ret = memx_stream_ofmap(
            static_cast<uint8_t>(model_id),
            static_cast<uint8_t>(flow_id),
            buf,
            timeout
        );
        *_aidl_return = ret;
        if (ret != HAL_SUCCESS_CODE) {
            ALOGE("memxStreamOfmap: memx_stream_ofmap failed (status=%d)", ret);
            return ScopedAStatus::fromServiceSpecificError(ret);
        }

        ALOGD("memxStreamOfmap: success, returned %d, bufferSize=%zu", ret, bufferSize);
        return ScopedAStatus::ok();
    }
};

int main() {
    ALOGI("MemxDriverService (NDK static) starting");

    // configure binder threads
    ABinderProcess_setThreadPoolMaxThreadCount(4);

    // instantiate HAL implementation
    auto service = ndk::SharedRefBase::make<MemxDriverService>();
    if (!service) {
        ALOGE("Failed to create MemxDriverService instance");
        return EXIT_FAILURE;
    }

    const std::string descriptor = BnMemxDriverService::descriptor;
    const std::string instance = "default";
    std::string serviceName = descriptor + "/" + instance;

    ALOGI("DBG: descriptor=%s, instance=%s, serviceName=%s, len=%lu,  binder=%p",
        descriptor.c_str(), instance.c_str(), serviceName.c_str(), serviceName.length(), service->asBinder().get());

    // publish service (Android 14 requires manual publish)
    binder_exception_t ex = AServiceManager_addService(
        service->asBinder().get(),
        serviceName.c_str());
    if (ex != EX_NONE) {
        ALOGE("Failed to publish %s: exception=%d", serviceName.c_str(), ex);
        return EXIT_FAILURE;
    }
    ALOGI("Published service %s", serviceName.c_str());

    // join the binder thread pool
    ABinderProcess_joinThreadPool();
    // should not reach here
    ALOGE("Exiting binder thread pool unexpectedly");
    return EXIT_FAILURE;
}