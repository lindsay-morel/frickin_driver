/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/

/***************************************************************************//**
 * mpu related headers
 ******************************************************************************/
#include "memx_global_config.h"
#include "memx_mpu.h"
#include "memx_mpu_comm.h"
#include "memx_device_manager.h"
/***************************************************************************//**
 * utility and common related headers
 ******************************************************************************/
#include "memx_log.h"
#include "memx.h"
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>
/***************************************************************************//**
 * version
 ******************************************************************************/
/**
 * @brief Library version string which can be obtained on linux with command:
 * $ strings libmemx.so | grep "libmemx version".
 */
const char *LIB_VER = "libmemx_version: " MEMX_LIBRARY_VERSION;

/***************************************************************************//**
 * structure
 ******************************************************************************/
/**
 * @brief Each model should be binded with one specific driver instance to
 * perform both configuration and data streaming. However, multiple models can
 * share the same driver instance while they can all use the same MPU device
 * group to run inference. Mutex should be performed by user using pause/resume
 * function to prevent driver data conflict.
 */
typedef struct _MemxModel {
    MemxMpu *mpu;           // model instance for specific model
    unsigned int ref_count; // records how many users are currently using the model instance
    unsigned int group_id;  // driver instance ID binded to run configuration and inference
} MemxModel, *pMemxModel;

/***************************************************************************//**
 * static object
 ******************************************************************************/

/**
 * @brief Lock for instance changes.
 */
static platform_share_mutex_t g_mpu_models_lock_ptr;

/**
 * @brief Model instances.
 */
static pMemxModel g_mpu_models[MEMX_MODEL_MAX_NUMBER] = {NULL};

/**
 * @brief Stored last model ID for skip download model for same model,
 * assign an invalid model ID to prevent skip model that haven't been download.
 */
static uint8_t g_last_model_id = 0xff;

/***************************************************************************//**
 * constructor
 ******************************************************************************/
static void         __memx_model_sharelock_create();
static void         __memx_model_sharelock_destory();
static void         __memx_model_decease_refcount(uint8_t model_id);
static void         __memx_model_increase_refcount(uint8_t model_id);
static pMemxModel   __memx_model_get_model(uint8_t model_id);
static memx_status  __memx_model_set_model(uint8_t model_id, pMemxModel model);
static memx_status  __memx_model_create(uint8_t model_id, uint8_t group_id);
static memx_status  __memx_model_destory(uint8_t model_id, uint8_t force);
static memx_status  __memx_model_checkid(uint8_t model_id);
static pMemxMpu     __memx_model_get_mpu(uint8_t model_id);

static void __memx_model_sharelock_create()
{
#if _WIN32
    g_mpu_models_lock_ptr = CreateSemaphore(NULL, 1, 1, NULL);
    if (g_mpu_models_lock_ptr == NULL)
    {
        printf("%s: g_mpu_models_lock_ptr failed\r\n", __FUNCTION__);
    }
#endif
}

static void __memx_model_sharelock_destory()
{
#if _WIN32
    if(g_mpu_models_lock_ptr != NULL)
    {
        platform_close(&g_mpu_models_lock_ptr);
        g_mpu_models_lock_ptr = NULL;
    }
#endif
}

static pMemxModel __memx_model_get_model(uint8_t model_id)
{
    pMemxModel result = (model_id < MEMX_MODEL_MAX_NUMBER) ? g_mpu_models[model_id] : NULL;
    return result;
}

static memx_status __memx_model_set_model(uint8_t model_id, pMemxModel model)
{
    memx_status result = MEMX_STATUS_OK;
    if (model_id < MEMX_MODEL_MAX_NUMBER) {
        g_mpu_models[model_id] = model;
    }
    else {
        result = MEMX_STATUS_MODEL_INVALID_ID;
    }

    return result;
}

static void __memx_model_decease_refcount(uint8_t model_id)
{
    pMemxModel pModel = __memx_model_get_model(model_id);
    if (pModel) {
        platform_share_mutex_lock(&g_mpu_models_lock_ptr);
        if (pModel->ref_count) pModel->ref_count--;
        platform_share_mutex_unlock(&g_mpu_models_lock_ptr);
    }
}

static void __memx_model_increase_refcount(uint8_t model_id)
{
    pMemxModel pModel = __memx_model_get_model(model_id);
    if (pModel) {
        platform_share_mutex_lock(&g_mpu_models_lock_ptr);
        if (pModel->ref_count < UINT_MAX) pModel->ref_count++;
        platform_share_mutex_unlock(&g_mpu_models_lock_ptr);
    }
}

static memx_status __memx_model_checkid(uint8_t model_id)
{
    memx_status status = MEMX_STATUS_OK;
    if (model_id >= MEMX_MODEL_MAX_NUMBER) {
        status = MEMX_STATUS_MODEL_INVALID_ID;
    } else if (!__memx_model_get_model(model_id)) {
        status = MEMX_STATUS_MODEL_NOT_OPEN;
    }

    return status;
}

static pMemxMpu __memx_model_get_mpu(uint8_t model_id)
{
    pMemxMpu result = (model_id < MEMX_MODEL_MAX_NUMBER) ? g_mpu_models[model_id]->mpu : NULL;
    return result;
}

/**
 * @brief Only destroys model instance in case no other one refers to it.
 * Otherwise, decreases reference count and leaves.
 *
 * @param model_id            model ID
 *
 * @return 0 on success, otherwise error code
 */
static memx_status __memx_model_destory(uint8_t model_id, uint8_t force)
{
    memx_status status = MEMX_STATUS_OK;

    status = __memx_model_checkid(model_id);
    if(status == MEMX_STATUS_OK){
        // do not modify mpu_device_group here
        pMemxModel pModel = __memx_model_get_model(model_id);
        // gets binded MPU device group id from model
        uint8_t group_id = (uint8_t)pModel->group_id;

        if (!force) {
            // decreases reference count
            __memx_model_decease_refcount(model_id);
            if (pModel->ref_count == 0){
                force = 1;// destroys mpu and model if reference count goes down to zero
            }
        }

        if (force) {
            memx_mpu_destroy(pModel->mpu);
            free(pModel);
            status = __memx_model_set_model(model_id, NULL);
            status = memx_device_group_destory(group_id); // destroys MPU device group
        }
    }

    return status;
}

/**
 * @brief This function should only create new instance if specified model
 * instance has not been created before and be recorded. Otherwise should add
 * reference to the instance and return is instead.
 *
 * @param model_id            model ID
 * @param group_id            MPU device group ID
 *
 * @return 0 on success, otherwise error code
 */
static memx_status __memx_model_create(uint8_t model_id, uint8_t group_id)
{
    memx_status status          = MEMX_STATUS_OK;
    //group_id will be check at memx_device_group_create()

    pMemxModel pModel = __memx_model_get_model(model_id);
    if (pModel) {//Model exist case
        if (pModel->group_id != group_id) { // check if MPU device group instance conflict
            status = MEMX_STATUS_MODEL_IN_USE;
        } else {     // otherwise, add reference to MPU device group binded and model
            status = memx_device_group_create(group_id); //should success here since model has created
            __memx_model_increase_refcount(model_id);
        }
    } else { //Model not exist case
        pModel = (pMemxModel)malloc(sizeof(MemxModel)); //create new model
        if (!pModel) {
            printf("create_mpu_model: malloc forg_mpu_models[%u] fail\n", model_id);
            status = MEMX_STATUS_OUT_OF_MEMORY;
        } else {
            status = memx_device_group_create(group_id);  // open device group before model
            if (status != MEMX_STATUS_OK){
                free(pModel);
            } else {
                memset(pModel, 0, sizeof(MemxModel));
                __memx_model_set_model(model_id, pModel);
                pMemxDeviceGroup pDevice    = memx_device_group_get_device(group_id); //Should success since just created device
                pMemxDeviceMeta  pDevMeta   = memx_device_group_get_devicemeta(group_id);

                pModel->mpu = memx_mpu_create(pDevice->mpuio, pDevMeta->chip_gen);
                if (!pModel->mpu) {
                    __memx_model_destory(model_id, 0);
                    printf("create_mpu_model: pModel[%u]->mpu create fail\n", model_id);
                    status = MEMX_STATUS_MPU_OPEN_FAIL;
                } else {
                    pModel->mpu->model_id = model_id;  // save self's model ID
                    pModel->group_id = group_id;       // records driver instance ID binded
                    // add reference to driver if open model success
                    // do not modify mpu_device_group, we only modify mpu_model in this function
                    __memx_model_increase_refcount(model_id);
                }
            }
        }
    }

    return status;
}

#ifdef __linux__
static void __memx_init__(void) __attribute__((constructor));
static void __memx_del__(void) __attribute__((destructor));
#endif

static void __memx_init__(void)
{
    __memx_model_sharelock_create();
    memx_device_group_init_all();
    // read log level from env
    memx_log_init();
}

static void __memx_del__(void)
{
    // clean up all resource before exit
    for (uint8_t model_idx = 0; model_idx < MEMX_MODEL_MAX_NUMBER; ++model_idx) {
        __memx_model_destory(model_idx, 1);
    }

    memx_device_group_del_all();
    __memx_model_sharelock_destory();
}

#ifdef _WIN32
BOOL WINAPI DllMain(
    HINSTANCE hinstDLL,  // handle to DLL module
    DWORD fdwReason,     // reason for calling function
    LPVOID lpReserved)  // reserved
{
    UNREFERENCED_PARAMETER(hinstDLL);
    UNREFERENCED_PARAMETER(lpReserved);
    // Perform actions based on the reason for calling.
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        // Initialize once for each new process.
        // Return FALSE to fail DLL load.
        __memx_init__();
        printf("DLL_PROCESS_ATTACH\n");
        break;

    case DLL_THREAD_ATTACH:
        // Do thread-specific initialization.
        break;

    case DLL_THREAD_DETACH:
        // Do thread-specific cleanup.
        break;

    case DLL_PROCESS_DETACH:
        __memx_del__();
        // Perform any necessary cleanup.
        printf("DLL_PROCESS_DETACH\n");
        break;
    }
    return TRUE;  // Successful DLL_PROCESS_ATTACH.
}
#endif

/***************************************************************************//**
 * helper
 ******************************************************************************/

/***************************************************************************//**
 * implementation
 ******************************************************************************/
memx_status memx_lock(uint8_t group_id)
{
    return memx_device_group_lock(group_id);
}

memx_status memx_trylock(uint8_t group_id)
{
    return memx_device_group_trylock(group_id);
}

memx_status memx_unlock(uint8_t group_id)
{
    return memx_device_group_unlock(group_id);
}

memx_status memx_open(uint8_t model_id, uint8_t group_id, float chip_gen)
{
    unused(chip_gen);
    memx_status status = MEMX_STATUS_OK;

    if (model_id >= MEMX_MODEL_MAX_NUMBER) {
        status = MEMX_STATUS_MODEL_INVALID_ID;
    } else {
        status = memx_device_antirollback_check(group_id);
        if (status == MEMX_STATUS_OK) {
            status = __memx_model_create(model_id, group_id);
        }
    }

    return status;
}

memx_status memx_close(uint8_t model_id)
{
    memx_status status;

    // Reset global last model ID
    g_last_model_id = 0xff;
    status = __memx_model_destory(model_id, 0);

    return status;
}

memx_status memx_operation(uint8_t model_id, uint32_t cmd_id, void *data, uint32_t size)
{
    memx_status status = __memx_model_checkid(model_id);
    if(status == MEMX_STATUS_OK){
        if (!data) {
            status = MEMX_STATUS_NULL_POINTER;
        } else {
            status = memx_mpu_operation(__memx_model_get_mpu(model_id), cmd_id, data, size, 0);
        }
    }

    return status;
}

memx_status memx_download_firmware(uint8_t group_id, const char *data, uint8_t type)
{
    return memx_device_download_fw(group_id, data, type);
}

memx_status memx_download_model_config(uint8_t model_id, const char *file_path, uint8_t model_idx)
{
    memx_status status      = __memx_model_checkid(model_id);
    pDfpContext pContext    = NULL;
    if(status == MEMX_STATUS_OK){
        pContext = memx_dfp_get_dfp_mata(model_id);
        status = memx_dfp_parsing_context(file_path, MEMX_MPU_DOWNLOAD_MODEL_TYPE_MPU_CONFIG, model_idx, pContext);
        if(status == MEMX_STATUS_OK){
            status = memx_mpu_download_model(__memx_model_get_mpu(model_id), pContext, model_idx, MEMX_MPU_DOWNLOAD_MODEL_TYPE_MPU_CONFIG, 0);
        }
        memx_free_dfp_meta(model_id);
    }

    return status;
}

memx_status memx_download_model_wtmem(uint8_t model_id, const char *file_path)
{
    memx_status status      = __memx_model_checkid(model_id);
    pDfpContext pContext    = NULL;
    if(status == MEMX_STATUS_OK){
        pContext = memx_dfp_get_dfp_mata(model_id);
        status = memx_dfp_parsing_context(file_path, MEMX_MPU_DOWNLOAD_MODEL_TYPE_WEIGHT_MEMORY, 0, pContext);
        if(status == MEMX_STATUS_OK){
            status = memx_mpu_download_model(__memx_model_get_mpu(model_id), pContext, 0, MEMX_MPU_DOWNLOAD_MODEL_TYPE_WEIGHT_MEMORY, 0);
        }
        memx_free_dfp_meta(model_id);
    }

    return status;
}

memx_status memx_download_model(uint8_t model_id, const char *file_path, uint8_t model_idx, int type)
{
    memx_status status      = __memx_model_checkid(model_id);
    pDfpContext pContext    = NULL;
    if(status == MEMX_STATUS_OK){
        if (g_last_model_id == model_id) {
            status = MEMX_STATUS_OK;
        } else {
            g_last_model_id = model_id;
            pContext = memx_dfp_get_dfp_mata(model_id);
            status = memx_dfp_parsing_context(file_path, type, model_idx, pContext);
            if(status == MEMX_STATUS_OK){
                status = memx_mpu_download_model(__memx_model_get_mpu(model_id), pContext, model_idx, type, 0);
            }
            memx_free_dfp_meta(model_id);
        }
    }

    return status;
}

memx_status memx_download_model_from_cahce(uint8_t model_id, void *pContext, uint8_t model_idx, int type)
{
    memx_status status      = __memx_model_checkid(model_id);
    if(status == MEMX_STATUS_OK){
        status = memx_dfp_check_cache_entry((pDfpContext) pContext, type);
        if(status == MEMX_STATUS_OK){
            status = memx_mpu_download_model(__memx_model_get_mpu(model_id), pContext, model_idx, type, 0);
            if(status == MEMX_STATUS_OK){
                g_last_model_id = model_id;
            }
        } else {
            status = MEMX_STATUS_NULL_POINTER;
        }
    }

    return status;
}

memx_status memx_set_stream_enable(uint8_t model_id, int wait)
{
    memx_status status = __memx_model_checkid(model_id);
    if (status == MEMX_STATUS_OK) {
        status =  memx_mpu_set_stream_enable(__memx_model_get_mpu(model_id), wait, 0);
    }

    return status;
}

memx_status memx_set_stream_disable(uint8_t model_id, int wait)
{
    memx_status status = __memx_model_checkid(model_id);
    if (status == MEMX_STATUS_OK) {
        status =  memx_mpu_set_stream_disable(__memx_model_get_mpu(model_id), wait, 0);
    }

    return status;
}

memx_status memx_set_ifmap_queue_size(uint8_t model_id, int size)
{
    memx_status status = __memx_model_checkid(model_id);
    if (status == MEMX_STATUS_OK) {
        status =  memx_mpu_set_ifmap_queue_size(__memx_model_get_mpu(model_id), size, 0);
    }

    return status;
}

memx_status memx_set_ofmap_queue_size(uint8_t model_id, int size)
{
    memx_status status = __memx_model_checkid(model_id);
    if (status == MEMX_STATUS_OK) {
        status =  memx_mpu_set_ofmap_queue_size(__memx_model_get_mpu(model_id), size, 0);
    }

    return status;
}

memx_status memx_get_ifmap_size(uint8_t model_id, uint8_t flow_id, int *height, int *width, int *z, int *channel_number, int *format)
{
    return memx_mpu_get_ifmap_size(__memx_model_get_mpu(model_id), flow_id, height, width, z, channel_number, format, 0);
}

memx_status memx_get_ifmap_range_convert(uint8_t model_id, uint8_t flow_id, int *enable, float *shift, float *scale)
{
    return memx_mpu_get_ifmap_range_convert(__memx_model_get_mpu(model_id), flow_id, enable, shift, scale, 0);
}

memx_status memx_get_ofmap_size(uint8_t model_id, uint8_t flow_id, int *height, int *width, int *z, int *channel_number, int *format)
{
    return memx_mpu_get_ofmap_size(__memx_model_get_mpu(model_id), flow_id, height, width, z, channel_number, format, 0);
}

memx_status memx_get_ofmap_hpoc(uint8_t model_id, uint8_t flow_id, int *hpoc_size, int **hpoc_indexes)
{
    return memx_mpu_get_ofmap_hpoc(__memx_model_get_mpu(model_id), flow_id, hpoc_size, hpoc_indexes, 0);
}

memx_status memx_operation_get_device_count(void *pData)
{
    return memx_get_device_count(pData);
}

memx_status memx_operation_get_mpu_group_count(uint8_t group_id, void *pData)
{
    return memx_get_mpu_group_count(group_id, pData);
}

memx_status memx_stream_ifmap(uint8_t model_id, uint8_t flow_id, void *ifmap, int timeout)
{
    memx_status status = __memx_model_checkid(model_id);
    if (status == MEMX_STATUS_OK) {
        status =  memx_mpu_stream_ifmap(__memx_model_get_mpu(model_id), flow_id, ifmap, timeout);
    }

    return status;
}

memx_status memx_stream_ofmap(uint8_t model_id, uint8_t flow_id, void *ofmap, int timeout)
{
    memx_status status = __memx_model_checkid(model_id);
    if (status == MEMX_STATUS_OK) {
        status =  memx_mpu_stream_ofmap(__memx_model_get_mpu(model_id), flow_id, ofmap, timeout);
    }

    return status;
}

memx_status memx_config_mpu_group(uint8_t group_id, uint8_t mpu_group_config)
{
    g_last_model_id = 0xff;
    return memx_device_config_mpu_group(group_id, mpu_group_config);
}

memx_status memx_get_chip_gen(uint8_t model_id, uint8_t* chip_gen)
{
    memx_status status = __memx_model_checkid(model_id);
    if (status == MEMX_STATUS_OK) {
        if(chip_gen){
            status =  memx_mpu_get_chip_gen(__memx_model_get_mpu(model_id), chip_gen, 0);
        } else {
            status = MEMX_STATUS_NULL_POINTER;
        }
    }

    return status;
}

memx_status memx_set_powerstate(uint8_t model_id, uint8_t state){
    memx_status status = __memx_model_checkid(model_id);

    if (status == MEMX_STATUS_OK) {
        pMemxModel pModel   = __memx_model_get_model(model_id);
        pMemxMpu   pMpu     = __memx_model_get_mpu(model_id);
        if (!pModel || !pMpu) {
            status = MEMX_STATUS_INTERNAL_ERROR;
        } else {
            if(state == MEMX_PS3){
                g_last_model_id = 0xff;
                status = memx_mpu_set_powerstate(pMpu, (uint8_t) pModel->group_id, state);
            } else if (state < MEMX_PS3){
                status = memx_mpu_set_powerstate(pMpu, (uint8_t) pModel->group_id, state);
            } else {
                status = MEMX_STATUS_INVALID_PARAMETER; //PS4 should use explict enter/exit API
            }
        }
    }

    return status;
}

memx_status memx_enter_device_deep_sleep(uint8_t group_id){
    memx_status status      = MEMX_STATUS_OK;
    g_last_model_id = 0xff;
    status = memx_set_feature(group_id, 0, OPCODE_SET_POWERMANAGEMENT, MEMX_PS4);
    return status;
}

memx_status memx_exit_device_deep_sleep(uint8_t group_id){
    memx_status status = MEMX_STATUS_OK;
    g_last_model_id = 0xff;
    status = memx_set_feature(group_id, 0, OPCODE_SET_POWERMANAGEMENT, MEMX_PS3);
    return status;
}

memx_status memx_get_total_chip_count(uint8_t group_id, uint8_t* chip_count)
{
    return memx_device_get_total_chip_count(group_id, chip_count);
}

memx_status memx_get_feature(uint8_t group_id, uint8_t chip_id, memx_get_feature_opcode opcode, void* buffer)
{
    return memx_device_get_feature(group_id, chip_id, opcode, buffer);
}

memx_status memx_set_feature(uint8_t group_id, uint8_t chip_id, memx_set_feature_opcode opcode, uint16_t parameter)
{
    return memx_device_set_feature(group_id, chip_id, opcode, parameter);
}

memx_status memx_self_test(uint8_t group_id, uint8_t chip_id, memx_selftest_opcode opcode, void* buffer)
{
    return memx_device_admincmd(group_id, chip_id, MEMX_ADMIN_CMD_SELFTEST, opcode, buffer);
}

memx_status memx_write_vendor_id(uint8_t group_id, uint32_t length, void* buffer)
{
    memx_status status = MEMX_STATUS_OK;
    struct transport_cmd cmd = {0};

    if ((length > MEMX_VENDOR_ID_FLASH_SIZE_MAX) || (length == 0)) {
        printf("%s: Parameter error, length(%d) > MEMX_VENDOR_ID_FLASH_SIZE_MAX(%d)\r\n", __FUNCTION__, length, MEMX_VENDOR_ID_FLASH_SIZE_MAX);
        status = MEMX_STATUS_INVALID_PARAMETER;
    } else {
        cmd.SQ_flash.cdw3 = 0; // user_data_sector_index
        cmd.SQ_flash.cdw4 = length;
        cmd.SQ_flash.data_ptr = buffer;

        status = memx_device_flash_access(group_id, 0, OPCODE_FLASH_ACCESS_WRITE, &cmd);
    }

    if (cmd.CQ.status != ERROR_STATUS_NO_ERROR) {
        printf("%s: Command CQ status error %d\r\n", __FUNCTION__, cmd.CQ.status);
        status = MEMX_STATUS_OTHERS;
    }

    return status;
}

memx_status memx_read_vendor_id(uint8_t group_id, uint32_t length, void* buffer)
{
    memx_status status = MEMX_STATUS_OK;
    struct transport_cmd cmd = {0};

    if ((length > MEMX_VENDOR_ID_FLASH_SIZE_MAX) || (length == 0)) {
        printf("%s: Parameter error, length(%d) > MEMX_VENDOR_ID_FLASH_SIZE_MAX(%d)\r\n", __FUNCTION__, length, MEMX_VENDOR_ID_FLASH_SIZE_MAX);
        status = MEMX_STATUS_INVALID_PARAMETER;
    } else {
        cmd.SQ.cdw3 = 0; // user_data_sector_index
        cmd.SQ.cdw4 = length;
        cmd.SQ_flash.data_ptr = buffer;

        status = memx_device_flash_access(group_id, 0, OPCODE_FLASH_ACCESS_READ, &cmd);
    }

    if (cmd.CQ.status != ERROR_STATUS_NO_ERROR) {
        printf("%s: Command CQ status error %d\r\n", __FUNCTION__, cmd.CQ.status);
        status = MEMX_STATUS_OTHERS;
    }

    return status;
}

memx_status memx_erase_vendor_id(uint8_t group_id, uint32_t length)
{
    memx_status status = MEMX_STATUS_OK;
    struct transport_cmd cmd = {0};

    if ((length > MEMX_VENDOR_ID_FLASH_SIZE_MAX) || (length == 0)) {
        printf("%s: Parameter error, length(%d) > MEMX_VENDOR_ID_FLASH_SIZE_MAX(%d)\r\n", __FUNCTION__, length, MEMX_VENDOR_ID_FLASH_SIZE_MAX);
        status = MEMX_STATUS_INVALID_PARAMETER;
    } else {
        cmd.SQ.cdw3 = 0; // user_data_sector_index
        cmd.SQ.cdw4 = length;

        status = memx_device_flash_access(group_id, 0, OPCODE_FLASH_ACCESS_ERASE, &cmd);
    }

    if (cmd.CQ.status != ERROR_STATUS_NO_ERROR) {
        printf("%s: Command CQ status error %d\r\n", __FUNCTION__, cmd.CQ.status);
        status = MEMX_STATUS_OTHERS;
    }

    return status;
}

memx_status memx_devio_control(uint8_t group_id, uint8_t chip_id, memx_devioctrl_opcode opcode, void* buffer)
{
    uint8_t sz = 0, *pbuf = (uint8_t *) buffer;
    struct transport_cmd cmd = {0};
    memx_status ret;

    if ((opcode == OPCODE_DEVIOCTRL_I2C_RW) && (pbuf[0] <= 16) && (pbuf[0] >= 2)) {
        sz = pbuf[0];
        cmd.SQ.opCode      = MEMX_ADMIN_CMD_DEVIOCTRL;
        cmd.SQ.cmdLen      = sizeof(struct transport_cmd);
        cmd.SQ.subOpCode   = FID_DEVICE_I2C_TRANSCEIVE;
        cmd.SQ.reqLen      = sz >> 1;
        cmd.SQ.cdw2        = chip_id;
        memcpy(&(cmd.SQ.cdw3), &pbuf[1], sz);
    } else if (opcode == OPCODE_DEVIOCTRL_GPIO_R) {
        cmd.SQ.opCode      = MEMX_ADMIN_CMD_GET_FEATURE;
        cmd.SQ.subOpCode   = FID_DEVICE_GPIO;
        cmd.SQ.cdw2        = chip_id;
        cmd.CQ.data[0]     = pbuf[0]; 
    } else if (opcode == OPCODE_DEVIOCTRL_GPIO_W) {
        cmd.SQ.opCode      = MEMX_ADMIN_CMD_SET_FEATURE;
        cmd.SQ.subOpCode   = FID_DEVICE_GPIO;
        cmd.SQ.cdw2        = chip_id;
        cmd.SQ.cdw3        = pbuf[0];
        cmd.SQ.cdw4        = pbuf[1];
    } else {
        return MEMX_STATUS_INVALID_PARAMETER;
    }

    ret = memx_device_admincmd(group_id, chip_id, cmd.SQ.opCode, cmd.SQ.subOpCode, (void *)&cmd);

    if(opcode == OPCODE_DEVIOCTRL_I2C_RW) {
        memcpy(&pbuf[1], &(cmd.CQ.data[3]), sz);
    } else if(opcode == OPCODE_DEVIOCTRL_GPIO_R) {
        memcpy(&pbuf[1], &(cmd.CQ.data[1]), sizeof(uint8_t));
    }

    return ret;
}

memx_status memx_enqueue_ifmap_buf(uint8_t model_id, uint8_t flow_id, memx_fmap_buf_t* fmap_buf, int32_t timeout)
{
    memx_status status = __memx_model_checkid(model_id);
    if (status == MEMX_STATUS_OK) {
        if (g_last_model_id == 0xff) {
            status = MEMX_STATUS_MODEL_NOT_CONFIG;
        } else {
            status = memx_mpu_enqueue_ifmap_buf(__memx_model_get_mpu(model_id), flow_id, fmap_buf, timeout);
        }
    }

    return status;
}

memx_status memx_enqueue_ofmap_buf(uint8_t model_id, uint8_t flow_id, memx_fmap_buf_t* fmap_buf, int32_t timeout)
{
    memx_status status = __memx_model_checkid(model_id);
    if (status == MEMX_STATUS_OK) {
        if (g_last_model_id == 0xff) {
            status = MEMX_STATUS_MODEL_NOT_CONFIG;
        } else {
            status = memx_mpu_enqueue_ofmap_buf(__memx_model_get_mpu(model_id), flow_id, fmap_buf, timeout);
        }
    }

    return status;
}

memx_status memx_dequeue_ifmap_buf(uint8_t model_id, uint8_t flow_id, memx_fmap_buf_t* fmap_buf, int32_t timeout)
{
    memx_status status = __memx_model_checkid(model_id);
    if (status == MEMX_STATUS_OK) {
        if (g_last_model_id == 0xff) {
            status = MEMX_STATUS_MODEL_NOT_CONFIG;
        } else {
            status = memx_mpu_dequeue_ifmap_buf(__memx_model_get_mpu(model_id), flow_id, fmap_buf, timeout);
        }
    }

    return status;
}

memx_status memx_dequeue_ofmap_buf(uint8_t model_id, uint8_t flow_id, memx_fmap_buf_t* fmap_buf, int32_t timeout)
{
    memx_status status = __memx_model_checkid(model_id);
    if (status == MEMX_STATUS_OK) {
        if (g_last_model_id == 0xff) {
            status = MEMX_STATUS_MODEL_NOT_CONFIG;
        } else {
            status = memx_mpu_dequeue_ofmap_buf(__memx_model_get_mpu(model_id), flow_id, fmap_buf, timeout);
        }
    }

    return status;
}

memx_status memx_set_abort_read(uint8_t model_id)
{
    memx_status status = MEMX_STATUS_OK;
    pMemxMpu pMpu = null;

    status = __memx_model_checkid(model_id);
    if (status == MEMX_STATUS_OK) {
        pMpu = __memx_model_get_mpu(model_id);
        status = memx_mpu_set_read_abort(pMpu);
    }

    return status;
}