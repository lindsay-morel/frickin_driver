/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#include "memx_device_manager.h"
#include <stdlib.h>

#ifdef __linux__
    #include <errno.h>
    #define LOCK_NAME_LEN 32
    static int g_mpu_device_group_fds[MEMX_DEVICE_MAX_NUMBER] = {0};
    static platform_mutex_t g_mpu_device_group_guards[MEMX_DEVICE_MAX_NUMBER] = {0};
#elif _WIN32
    #include <tchar.h>
    #define SYSTEN_REGKEY_BASEPATH "SYSTEM\\CurrentControlSet\\Control\\Class\\"
    static platform_share_mutex_t g_mpu_device_group_guards[MEMX_DEVICE_MAX_NUMBER] = {NULL};
#endif
static platform_share_mutex_t g_mpu_device_groups_lock_ptr;

/**
 * @brief Driver instances.
 */
static pMemxDeviceGroup g_mpu_device_groups[MEMX_DEVICE_MAX_NUMBER] = {NULL};
static MemxDeviceMeta   g_mpu_devicemeta[MEMX_DEVICE_MAX_NUMBER];
static uint32_t g_usb_device_major[MEMX_DEVICE_MAX_NUMBER] = {0};
static uint32_t g_usb_device_minor[MEMX_DEVICE_MAX_NUMBER] = {0};

static void         __device_qspi_reset_release(uint8_t group_id);
static void         __device_group_decease_refcount(uint8_t group_id);
static void         __device_group_increase_refcount(uint8_t group_id);
static void         __device_group_guard_init_all();
static void         __device_group_guard_del_all();
static void         __device_group_sharelock_create();
static void         __device_group_sharelock_destory();
static memx_status  __device_group_list();
static void         __device_group_clean();
static void         __device_group_set_device(uint8_t group_id, pMemxDeviceGroup device);
static memx_status  __device_group_check_group_id(uint8_t group_id);
static memx_status  __device_group_check_chip_gen_isvalid(uint8_t chip_gen);
static int          __device_group_usb_recognize(char* file_name, uint32_t* usb_device_counter, uint32_t devindex, memx_device_type device_type);
static memx_status  __device_group_fw_image_check(platform_uint32_t * pData, platform_uint32_t * pSize);

static memx_status __device_group_fw_image_check(platform_uint32_t * pData, platform_uint32_t * pSize){
    memx_status     status = MEMX_STATUS_OK;

    if (pData[(MEMX_FW_IMGFMT_OFFSET >> 2)] == 1) {
        *pSize = MEMX_FSBL_SECTION_SIZE + MEMX_FW_IMGSIZE_LEN + pData[(MEMX_FW_IMGSIZE_OFFSET >> 2)] + MEMX_FW_IMGCRC_LEN;
        MEMX_LOG(MEMX_LOG_GENERAL, "Total Size %d FSBL Size %d FW Size %d CRC 0x%x\r\n", *pSize, pData[0], pData[(MEMX_FW_IMGSIZE_OFFSET >> 2)], pData[(*pSize - 4) >> 2]);
    } else if (pData[(MEMX_FW_IMGFMT_OFFSET >> 2)] == 0) {
        *pSize = pData[0] + MEMX_IMG_TOTAL_SIZE_LEN; //ToalImgeSize + TotalImageSize(4B)
        MEMX_LOG(MEMX_LOG_GENERAL, "Total Size %d ImgSize %d CRC 0x%x\r\n", *pSize, pData[0], pData[(*pSize - 8) >> 2]);
    } else {
        status = MEMX_STATUS_OTHERS;
    }

    if ((*pSize == 0) || (*pSize) > MAX_FW_SIZE) {
        status = MEMX_STATUS_OTHERS;
    }

    return status;
}

memx_status memx_device_flash_access(uint8_t group_id, uint8_t chip_id, memx_flash_access_opcode flash_access_opcode, struct transport_cmd* cmd)
{
    memx_status status = MEMX_STATUS_OK;

    if (flash_access_opcode >= OPCODE_FLASH_ACCESS_MAX) {
        status = MEMX_STATUS_INVALID_PARAMETER;
        printf("memryx: _admin_command: open device error opcode(%u)\n", flash_access_opcode);
    } else {
        status = memx_device_admincmd(group_id, chip_id, MEMX_ADMIN_CMD_FLASH_RW, flash_access_opcode, (void *)cmd);
    }

    return status;
}

#ifdef __linux__
static void __device_group_guard_init_all()
{
    for (int group_idx = 0; group_idx < MEMX_DEVICE_MAX_NUMBER; ++group_idx) {
        platform_mutex_create(&g_mpu_device_group_guards[group_idx], NULL);
    }
}

static void __device_group_guard_del_all()
{
    for (int group_idx = 0; group_idx < MEMX_DEVICE_MAX_NUMBER; ++group_idx) {
        platform_mutex_destory(&g_mpu_device_group_guards[group_idx]);
    }
}

static void __device_group_sharelock_create()
{

}

static void __device_group_sharelock_destory()
{

}

static memx_status __device_group_list_split()
{
    uint32_t    devFoundNum     = 0;
    memx_status result          = MEMX_STATUS_OK;
    char        file_name[256]  = {0};
    int         fd[3]           = {0};

    memset(file_name, 0 , sizeof(file_name));
    for(uint32_t devindex = 0; devindex < MEMX_DEVICE_MAX_NUMBER; devindex++)
    {
        sprintf(file_name, "%s%d", MEMX_USB_SPLIT_IN_DEV_FILE, devindex);
        fd[0] =  platform_open((const void *)file_name,  0);
        sprintf(file_name, "%s%d", MEMX_USB_SPLIT_OUT_DEV_FILE, devindex);
        fd[1] =  platform_open((const void *)file_name,  0);

        if((fd[0] > 0) && (fd[1] > 0))
        {
            platform_close(&fd[0]);
            platform_close(&fd[1]);
            g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS] = (uint8_t*)malloc(sizeof(file_name));
            g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS_SPLIT] = (uint8_t*)malloc(sizeof(file_name));

            if(g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS] && g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS_SPLIT])
            {
                sprintf(file_name, "%s%d", MEMX_USB_SPLIT_IN_DEV_FILE, devindex);
                memcpy(g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS], file_name, sizeof(file_name));
                sprintf(file_name, "%s%d", MEMX_USB_SPLIT_OUT_DEV_FILE, devindex);
                memcpy(g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS_SPLIT], file_name, sizeof(file_name));
                //printf("%d Device USBi %d found! %s\n", MEMX_MPU_CHIP_GEN_CASCADE, devFoundNum, (unsigned char*) g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS]);
                //printf("%d Device USBo %d found! %s\n", MEMX_MPU_CHIP_GEN_CASCADE, devFoundNum, (unsigned char*) g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS_SPLIT]);
                g_mpu_devicemeta[devFoundNum].hif_type = MEMX_MPUIO_INTERFACE_USB;
                g_mpu_devicemeta[devFoundNum].chip_gen = MEMX_MPU_CHIP_GEN_CASCADE_PLUS;
                g_mpu_devicemeta[devFoundNum].role = MEMX_MPU_TYPE_SPLIT;
                devFoundNum++;
            }
            else
            { //garbage clean and keep scan
                free(g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS]);
                free(g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS_SPLIT]);
                memset(&g_mpu_devicemeta[devFoundNum], 0, sizeof(MemxDeviceMeta));
                result = MEMX_STATUS_DEVICE_OPEN_FAIL;
                break;
            }
        }
        else
        {
            if (fd[0] > 0) { platform_close(&fd[0]); }
            if (fd[1] > 0) { platform_close(&fd[1]); }
        }
    }

    return result;
}

static memx_status __device_group_list()
{
    uint32_t    usb_device_minor_counter   = 0;
    uint32_t    usb_device_major_counter   = 0;
    uint32_t    devFoundNum                = 0;
    memx_status result                     = MEMX_STATUS_OK;
    char        file_name[256]             = {0};
    int         fd[MEMX_DEVICE_TYPE_COUNT] = {0};

    for (uint32_t index = 0; index < 2; index++)
    {
        memset(file_name, 0 , sizeof(file_name));
        usb_device_major_counter = 0;
        usb_device_minor_counter = 0;
        for (uint32_t devindex = 0; devindex < MEMX_DEVICE_MAX_NUMBER; devindex++)
        {
            if (index) {
                fd[MEMX_DEVICE_TYPE_CASCADE_PLUS] = __device_group_usb_recognize(file_name, &usb_device_major_counter, devindex, MEMX_DEVICE_TYPE_CASCADE_PLUS);
                fd[MEMX_DEVICE_TYPE_FEATURE] = __device_group_usb_recognize(file_name, &usb_device_minor_counter, devindex, MEMX_DEVICE_TYPE_FEATURE);
            } else {
                sprintf(file_name, "%s%d", MEMX_PCIE_SINGLE_DEV_FILE, devindex);
                fd[MEMX_DEVICE_TYPE_CASCADE_PLUS] = platform_open((const void *)file_name, 0);
                sprintf(file_name, "%s%d_feature", MEMX_PCIE_SINGLE_DEV_FILE, devindex);
                fd[MEMX_DEVICE_TYPE_FEATURE] = platform_open((const void *)file_name, 0);
            }

            if((fd[MEMX_DEVICE_TYPE_CASCADE_PLUS] > 0) && (fd[MEMX_DEVICE_TYPE_FEATURE] > 0)) {
                platform_close(&fd[MEMX_DEVICE_TYPE_CASCADE_PLUS]);
                platform_close(&fd[MEMX_DEVICE_TYPE_FEATURE]);
                g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS] = (uint8_t*)malloc(sizeof(file_name));
                g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_FEATURE] = (uint8_t*)malloc(sizeof(file_name));
                if (g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS] &&
                    g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_FEATURE])
                {
                    struct transport_cmd cmd;
                    memset(&cmd, 0 , sizeof(struct transport_cmd));

                    sprintf(file_name, "%s%d", (index == 0) ? MEMX_PCIE_SINGLE_DEV_FILE : MEMX_USB_SINGLE_DEV_FILE, devindex);
                    memcpy(g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS], file_name, sizeof(file_name));

                    if (index) {
                        sprintf(file_name, "%s%d", MEMX_USB_SINGLE_DEV_FILE, g_usb_device_major[devindex]);
                        memcpy(g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS], file_name, sizeof(file_name));
                        sprintf(file_name, "%s%d_feature",  MEMX_USB_SINGLE_DEV_FILE, g_usb_device_minor[devindex]);
                    } else {
                        sprintf(file_name, "%s%d", MEMX_PCIE_SINGLE_DEV_FILE, devindex);
                        memcpy(g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS], file_name, sizeof(file_name));
                        sprintf(file_name, "%s%d_feature", MEMX_PCIE_SINGLE_DEV_FILE, devindex);
                    }
                    memcpy(g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_FEATURE], file_name, sizeof(file_name));
                    g_mpu_devicemeta[devFoundNum].hif_type = (index == 0) ? MEMX_MPUIO_INTERFACE_PCIE : MEMX_MPUIO_INTERFACE_USB;
                    g_mpu_devicemeta[devFoundNum].chip_gen = MEMX_MPU_CHIP_GEN_CASCADE_PLUS;
                    g_mpu_devicemeta[devFoundNum].role = MEMX_MPU_TYPE_SINGLE;
                    result = memx_device_submit_get_feature(devFoundNum, FID_DEVICE_FW_INFO, &cmd);
                    if (result == MEMX_STATUS_OK) {
                        g_mpu_devicemeta[devFoundNum].fw_rollback_cnt = (cmd.CQ.data[0] & 0x0000FFFF);
                        g_mpu_devicemeta[devFoundNum].fw_version      = (cmd.CQ.data[1]);
                        devFoundNum++;
                    } else {
                        free(g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS]);
                        free(g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_FEATURE]);
                        memset(&g_mpu_devicemeta[devFoundNum], 0, sizeof(MemxDeviceMeta));
                    }
                }
                else
                {
                    free(g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS]);
                    free(g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_FEATURE]);
                    result = MEMX_STATUS_DEVICE_OPEN_FAIL;
                    break;
                }
            }
            else
            {
                if (fd[MEMX_DEVICE_TYPE_CASCADE_PLUS] > 0) { platform_close(&fd[MEMX_DEVICE_TYPE_CASCADE_PLUS]); }
                if (fd[MEMX_DEVICE_TYPE_FEATURE] > 0) { platform_close(&fd[MEMX_DEVICE_TYPE_FEATURE]); }
            }
        }

        if (result != MEMX_STATUS_OK) { break; }
        if (devFoundNum == MEMX_DEVICE_MAX_NUMBER) { break; }
    }

    if((result == MEMX_STATUS_OK) && (devFoundNum == 0)) //no cascade+ single device found, try cascade+ split
    {
        __device_group_list_split();
    }

    return result;
}

static int __device_group_usb_recognize(char* file_name, uint32_t* usb_device_counter, uint32_t device_index, memx_device_type device_type)
{
    int fd = 0;

    while ((*usb_device_counter < MEMX_MAX_USB_MINORS) && (fd <= 0)) {
        if (device_type == MEMX_DEVICE_TYPE_CASCADE_PLUS) {
            sprintf(file_name, "%s%d", MEMX_USB_SINGLE_DEV_FILE, *usb_device_counter);
        } else {
            sprintf(file_name, "%s%d_feature", MEMX_USB_SINGLE_DEV_FILE, *usb_device_counter);
        }

        fd = platform_open((const void *)file_name, 0);

        if (fd > 0) {
            if (device_type == MEMX_DEVICE_TYPE_CASCADE_PLUS) {
                g_usb_device_major[device_index] = *usb_device_counter;
            } else {
                g_usb_device_minor[device_index] = *usb_device_counter;
            }
        }

        (*usb_device_counter)++;
    }

    return fd;
}

memx_status memx_device_group_lock(uint8_t group_id)
{
    char lock_str[LOCK_NAME_LEN] = {0};
    if (__device_group_check_group_id(group_id) != MEMX_STATUS_OK) { return MEMX_STATUS_DEVICE_INVALID_ID; }

    platform_mutex_lock(&g_mpu_device_group_guards[group_id]);
    snprintf(lock_str, LOCK_NAME_LEN, "/var/lock/memx_group%02d_lock", group_id);
    g_mpu_device_group_fds[group_id] = open(lock_str, O_CREAT | O_RDWR, 0666);
    if (g_mpu_device_group_fds[group_id] < 0) {
        return MEMX_STATUS_DEVICE_NOT_OPENED;
    }
    while (lockf(g_mpu_device_group_fds[group_id], F_LOCK, 0) == -1) {
        if ((errno != EACCES) && (errno != EAGAIN)) {
            close(g_mpu_device_group_fds[group_id]);
            return MEMX_STATUS_DEVICE_LOCK_FAIL;
        }
        usleep(10000); // sleep for 10 ms to avoid busy-waiting
    }

    return MEMX_STATUS_OK;
}

memx_status memx_device_group_trylock(uint8_t group_id)
{
    char lock_str[LOCK_NAME_LEN] = {0};
    if (__device_group_check_group_id(group_id) != MEMX_STATUS_OK) { return MEMX_STATUS_DEVICE_INVALID_ID; }

    if (platform_mutex_trylock(&g_mpu_device_group_guards[group_id]) != 0) { return MEMX_STATUS_DEVICE_IN_USE; }
    snprintf(lock_str, LOCK_NAME_LEN, "/var/lock/memx_group%02d_lock", group_id);
    g_mpu_device_group_fds[group_id] = open(lock_str, O_CREAT | O_RDWR, 0666);
    if (g_mpu_device_group_fds[group_id] < 0) {
        return MEMX_STATUS_DEVICE_NOT_OPENED;
    }
    if (lockf(g_mpu_device_group_fds[group_id], F_TLOCK, 0) == -1) {
        if ((errno == EACCES) || (errno == EAGAIN)) {
            close(g_mpu_device_group_fds[group_id]);
            return MEMX_STATUS_DEVICE_IN_USE;
        } else {
            close(g_mpu_device_group_fds[group_id]);
            return MEMX_STATUS_DEVICE_LOCK_FAIL;
        }
    }

    return MEMX_STATUS_OK;
}

memx_status memx_device_group_unlock(uint8_t group_id)
{
    if (__device_group_check_group_id(group_id) != MEMX_STATUS_OK) { return MEMX_STATUS_DEVICE_INVALID_ID; }
    if (g_mpu_device_group_fds[group_id] >= 0) {
        if (lockf(g_mpu_device_group_fds[group_id], F_ULOCK, 0) == -1) {
            perror("memx_device_group_unlock");
        }
        close(g_mpu_device_group_fds[group_id]);
        g_mpu_device_group_fds[group_id] = -1;
    }
    platform_mutex_unlock(&g_mpu_device_group_guards[group_id]);
    return MEMX_STATUS_OK;
}

memx_status memx_device_submit_get_feature(uint8_t group_id, uint8_t feature_id, void* data)
{
    memx_status         status      = MEMX_STATUS_OK;
    pMemxDeviceMeta     pDevMeta    = NULL;
    platform_handle_t   fd;
    platform_handle_t   event;
    int32_t ret                     = 0;
    struct transport_cmd *      cmd         = (struct transport_cmd *) data;
    do{
        pDevMeta = memx_device_group_get_devicemeta(group_id);
        if(!pDevMeta){
            status = MEMX_STATUS_DEVICE_INVALID_ID;
            break;
        }

        event = 0;
        cmd->SQ.opCode      = MEMX_ADMIN_CMD_GET_FEATURE;
        cmd->SQ.cmdLen      = sizeof(struct transport_cmd);
        cmd->SQ.subOpCode   = feature_id;
        cmd->SQ.reqLen      = sizeof(struct transport_cq);
        fd  = platform_open(pDevMeta->device_path[MEMX_DEVICE_TYPE_FEATURE], 0);
        ret = platform_ioctl(event, &fd, MEMX_GET_DEVICE_FEATURE, data, cmd->SQ.cmdLen);

        platform_close(&fd);

        if(ret != 0){
            status = MEMX_STATUS_OTHERS;
            break;
        }
    }while(0);

    return status;
}

memx_status memx_device_submit_set_feature(uint8_t group_id, uint8_t feature_id, void* data)
{
    memx_status         status   = MEMX_STATUS_OK;
    pMemxDeviceMeta     pDevMeta = NULL;
    platform_handle_t   fd;
    platform_handle_t   event;
    int32_t             ret      = 0;
    struct transport_cmd *      cmd      = (struct transport_cmd *) data;
    do{
        pDevMeta = memx_device_group_get_devicemeta(group_id);
        if(!pDevMeta){
            status = MEMX_STATUS_DEVICE_INVALID_ID;
            break;
        }

        event = 0;
        cmd->SQ.opCode      = MEMX_ADMIN_CMD_SET_FEATURE;
        cmd->SQ.cmdLen      = sizeof(struct transport_cmd);
        cmd->SQ.subOpCode   = feature_id;
        cmd->SQ.reqLen      = sizeof(struct transport_cq);
        if (feature_id == FID_DEVICE_FREQUENCY) {
            cmd->SQ.cdw4 = 0x6d656d66;
        }
        fd  = platform_open(pDevMeta->device_path[MEMX_DEVICE_TYPE_FEATURE], 0);
        ret = platform_ioctl(event, &fd, MEMX_SET_DEVICE_FEATURE, data, cmd->SQ.cmdLen);

        platform_close(&fd);

        if(ret != 0){
            status = MEMX_STATUS_OTHERS;
            break;
        }
    } while(0);

    return status;
}

memx_status memx_device_download_fw(uint8_t group_id, const char *fw_data, uint8_t type)
{
    memx_status         status      = MEMX_STATUS_OK;
    pMemxDeviceMeta     pDevMeta    = NULL;
    platform_handle_t   fd          = 0;
    platform_handle_t   event       = 0;
    int32_t             ret         = 0;
    platform_uint32_t   *data       = NULL;
    platform_uint32_t   size        = 0;

    do{
        pDevMeta = memx_device_group_get_devicemeta(group_id);
        if(!pDevMeta){
            status = MEMX_STATUS_DEVICE_INVALID_ID;
            break;
        }

        if(type == MEMX_DOWNLOAD_TYPE_FROM_BUFFER){
            if(fw_data == NULL){
                status = MEMX_STATUS_NULL_POINTER;
                break;
            } else {
                data = (platform_uint32_t *)fw_data;
            }

            status = __device_group_fw_image_check(data, &size);
            if(status){
                break;
            }
        } else if (type == MEMX_DOWNLOAD_TYPE_FROM_FILE){
            data = (platform_uint32_t *) malloc(MAX_FW_SIZE);
            FILE *fp = fopen(fw_data, "rb");
            if(data && fp) {
                size = (platform_uint32_t) fread(data, 1, MAX_FW_SIZE, fp);
                fclose(fp);

                status = __device_group_fw_image_check(data, &size);
                if(status){
                    break;
                }
            } else {
                status = MEMX_STATUS_FILE_NOT_FOUND;
                break;
            }
        } else {
                status = MEMX_STATUS_INVALID_PARAMETER;
                break;
        }

        __device_qspi_reset_release(group_id);

        struct memx_firmware_bin memx_fw_bin;
        memset(&memx_fw_bin, 0, sizeof(struct memx_firmware_bin));
        memx_fw_bin.buffer = (unsigned char *) data;
        memx_fw_bin.size = size;

        fd  = platform_open(pDevMeta->device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS], 0);
        ret = platform_ioctl(event, &fd, MEMX_DOWNLOAD_FIRMWARE, &memx_fw_bin, sizeof(struct memx_firmware_bin));

        if(ret != 0){
            status = MEMX_STATUS_PLATFORM_IOCTL_FAIL;
            break;
        }
    } while(0);

    if(data && (type == MEMX_DOWNLOAD_TYPE_FROM_FILE)){
        free(data);
    }

    if(fd > 0){
        platform_close(&fd);
    }

    return status;
}

memx_status memx_device_admincmd(uint8_t group_id, uint8_t chip_id, uint32_t opCode, uint32_t subOpCode, void* data)
{
    memx_status         status      = MEMX_STATUS_OK;
    pMemxDeviceMeta     pDevMeta    = NULL;
    platform_handle_t   fd;
    platform_handle_t   event;
    int32_t ret                     = 0;
    struct transport_cmd *      cmd         = (struct transport_cmd *) data;
    do{
        pDevMeta = memx_device_group_get_devicemeta(group_id);
        if(!pDevMeta){
            printf("%s: device meta error\r\n", __FUNCTION__);
            status = MEMX_STATUS_DEVICE_INVALID_ID;
            break;
        }

        event = 0;
        if(opCode != MEMX_ADMIN_CMD_DEVIOCTRL) {
            cmd->SQ.opCode      = opCode;
            cmd->SQ.cmdLen      = sizeof(struct transport_cmd);
            cmd->SQ.subOpCode   = subOpCode;
            cmd->SQ.reqLen      = sizeof(struct transport_cq);
            cmd->SQ.cdw2        = chip_id;
        }
        fd  = platform_open(pDevMeta->device_path[MEMX_DEVICE_TYPE_FEATURE], 0);
        ret = platform_ioctl(event, &fd, MEMX_ADMIN_COMMAND, data, cmd->SQ.cmdLen);

        platform_close(&fd);

        if(ret != 0){
            printf("%s: Admin command ret %d\r\n", __FUNCTION__, ret);
            status = MEMX_STATUS_OTHERS;
            break;
        }
    }while(0);

    return status;
}
#elif _WIN32
static void __device_group_guard_init_all()
{
    LPCWSTR mutex_name[MEMX_DEVICE_MAX_NUMBER] = {L"MEMX_LOCK_GROUP_0", L"MEMX_LOCK_GROUP_1", L"MEMX_LOCK_GROUP_2", L"MEMX_LOCK_GROUP_3",
                                                L"MEMX_LOCK_GROUP_4", L"MEMX_LOCK_GROUP_5", L"MEMX_LOCK_GROUP_6", L"MEMX_LOCK_GROUP_7",
                                                L"MEMX_LOCK_GROUP_8", L"MEMX_LOCK_GROUP_9", L"MEMX_LOCK_GROUP_10", L"MEMX_LOCK_GROUP_11",
                                                L"MEMX_LOCK_GROUP_12", L"MEMX_LOCK_GROUP_13", L"MEMX_LOCK_GROUP_14", L"MEMX_LOCK_GROUP_15",
                                                L"MEMX_LOCK_GROUP_16", L"MEMX_LOCK_GROUP_17", L"MEMX_LOCK_GROUP_18", L"MEMX_LOCK_GROUP_19",
                                                L"MEMX_LOCK_GROUP_20", L"MEMX_LOCK_GROUP_21", L"MEMX_LOCK_GROUP_22", L"MEMX_LOCK_GROUP_23",
                                                L"MEMX_LOCK_GROUP_24", L"MEMX_LOCK_GROUP_25", L"MEMX_LOCK_GROUP_26", L"MEMX_LOCK_GROUP_27",
                                                L"MEMX_LOCK_GROUP_28", L"MEMX_LOCK_GROUP_29", L"MEMX_LOCK_GROUP_30", L"MEMX_LOCK_GROUP_31",
                                                L"MEMX_LOCK_GROUP_32", L"MEMX_LOCK_GROUP_33", L"MEMX_LOCK_GROUP_34", L"MEMX_LOCK_GROUP_35",
                                                L"MEMX_LOCK_GROUP_36", L"MEMX_LOCK_GROUP_37", L"MEMX_LOCK_GROUP_38", L"MEMX_LOCK_GROUP_39",
                                                L"MEMX_LOCK_GROUP_40", L"MEMX_LOCK_GROUP_41", L"MEMX_LOCK_GROUP_42", L"MEMX_LOCK_GROUP_43",
                                                L"MEMX_LOCK_GROUP_44", L"MEMX_LOCK_GROUP_45", L"MEMX_LOCK_GROUP_46", L"MEMX_LOCK_GROUP_47",
                                                L"MEMX_LOCK_GROUP_48", L"MEMX_LOCK_GROUP_49", L"MEMX_LOCK_GROUP_50", L"MEMX_LOCK_GROUP_51",
                                                L"MEMX_LOCK_GROUP_52", L"MEMX_LOCK_GROUP_53", L"MEMX_LOCK_GROUP_54", L"MEMX_LOCK_GROUP_55",
                                                L"MEMX_LOCK_GROUP_56", L"MEMX_LOCK_GROUP_57", L"MEMX_LOCK_GROUP_58", L"MEMX_LOCK_GROUP_59",
                                                L"MEMX_LOCK_GROUP_60", L"MEMX_LOCK_GROUP_61", L"MEMX_LOCK_GROUP_62", L"MEMX_LOCK_GROUP_63",
                                                L"MEMX_LOCK_GROUP_64", L"MEMX_LOCK_GROUP_65", L"MEMX_LOCK_GROUP_66", L"MEMX_LOCK_GROUP_67",
                                                L"MEMX_LOCK_GROUP_68", L"MEMX_LOCK_GROUP_69", L"MEMX_LOCK_GROUP_70", L"MEMX_LOCK_GROUP_71",
                                                L"MEMX_LOCK_GROUP_72", L"MEMX_LOCK_GROUP_73", L"MEMX_LOCK_GROUP_74", L"MEMX_LOCK_GROUP_75",
                                                L"MEMX_LOCK_GROUP_76", L"MEMX_LOCK_GROUP_77", L"MEMX_LOCK_GROUP_78", L"MEMX_LOCK_GROUP_79",
                                                L"MEMX_LOCK_GROUP_80", L"MEMX_LOCK_GROUP_81", L"MEMX_LOCK_GROUP_82", L"MEMX_LOCK_GROUP_83",
                                                L"MEMX_LOCK_GROUP_84", L"MEMX_LOCK_GROUP_85", L"MEMX_LOCK_GROUP_86", L"MEMX_LOCK_GROUP_87",
                                                L"MEMX_LOCK_GROUP_88", L"MEMX_LOCK_GROUP_89", L"MEMX_LOCK_GROUP_90", L"MEMX_LOCK_GROUP_91",
                                                L"MEMX_LOCK_GROUP_92", L"MEMX_LOCK_GROUP_93", L"MEMX_LOCK_GROUP_94", L"MEMX_LOCK_GROUP_95",
                                                L"MEMX_LOCK_GROUP_96", L"MEMX_LOCK_GROUP_97", L"MEMX_LOCK_GROUP_98", L"MEMX_LOCK_GROUP_99",
                                                L"MEMX_LOCK_GROUP_100", L"MEMX_LOCK_GROUP_101", L"MEMX_LOCK_GROUP_102", L"MEMX_LOCK_GROUP_103",
                                                L"MEMX_LOCK_GROUP_104", L"MEMX_LOCK_GROUP_105", L"MEMX_LOCK_GROUP_106", L"MEMX_LOCK_GROUP_107",
                                                L"MEMX_LOCK_GROUP_108", L"MEMX_LOCK_GROUP_109", L"MEMX_LOCK_GROUP_110", L"MEMX_LOCK_GROUP_111",
                                                L"MEMX_LOCK_GROUP_112", L"MEMX_LOCK_GROUP_113", L"MEMX_LOCK_GROUP_114", L"MEMX_LOCK_GROUP_115",
                                                L"MEMX_LOCK_GROUP_116", L"MEMX_LOCK_GROUP_117", L"MEMX_LOCK_GROUP_118", L"MEMX_LOCK_GROUP_119",
                                                L"MEMX_LOCK_GROUP_120", L"MEMX_LOCK_GROUP_121", L"MEMX_LOCK_GROUP_122", L"MEMX_LOCK_GROUP_123",
                                                L"MEMX_LOCK_GROUP_124", L"MEMX_LOCK_GROUP_125", L"MEMX_LOCK_GROUP_126", L"MEMX_LOCK_GROUP_127"};
    for (int group_idx = 0; group_idx < MEMX_DEVICE_MAX_NUMBER; ++group_idx) {
        g_mpu_device_group_guards[group_idx] = CreateSemaphore(NULL, 1, 1, mutex_name[group_idx]);
        if(g_mpu_device_group_guards[group_idx] == NULL)
        {
             printf("%s: g_mpu_device_group_guards[%d]  failed\r\n", __FUNCTION__, group_idx);
        }
    }
}

static void __device_group_guard_del_all()
{
    for (int group_idx = 0; group_idx < MEMX_DEVICE_MAX_NUMBER; ++group_idx) {
        if(g_mpu_device_group_guards[group_idx] != NULL)
        {
            platform_close(&g_mpu_device_group_guards[group_idx]);
            g_mpu_device_group_guards[group_idx] = NULL;
        }
    }
}

static void __device_group_sharelock_create()
{
    g_mpu_device_groups_lock_ptr = CreateSemaphore(NULL, 1, 1, NULL);
    if(g_mpu_device_groups_lock_ptr == NULL)
    {
        printf("%s: g_mpu_device_groups_lock_ptr failed\r\n", __FUNCTION__);
    }
}

static void __device_group_sharelock_destory()
{
    if(g_mpu_device_groups_lock_ptr != NULL)
    {
        platform_close(&g_mpu_device_groups_lock_ptr);
        g_mpu_device_groups_lock_ptr = NULL;
    }
}

static memx_status __device_group_list_split()
{
    HDEVINFO                            devInfo = INVALID_HANDLE_VALUE;
    SP_DEVICE_INTERFACE_DATA            devInterfaceData = { 0 };
    PSP_DEVICE_INTERFACE_DETAIL_DATA    devInterfaceDetailData = NULL;
    ULONG                               strSize = 0;
    ULONG                               requiredSize = 0;
    ULONG                               error = 0;
    ULONG                               devFoundNum = 0;
    memx_status                         result = MEMX_STATUS_OK;
    const GUID* pGuidi = NULL;
    const GUID* pGuido = NULL;
    for (ULONG index = 0; index < MEMX_DEVICE_MAX_NUMBER; index++)
    {
        switch (index) {
            case 0:
                pGuidi = &GUID_CLASS_MEMX_CASCADE_MUTLI_G0_FIRST_USB;
                pGuido = &GUID_CLASS_MEMX_CASCADE_MUTLI_G0_LAST_USB;
                break;
            case 1:
                pGuidi = &GUID_CLASS_MEMX_CASCADE_MUTLI_G1_FIRST_USB;
                pGuido = &GUID_CLASS_MEMX_CASCADE_MUTLI_G1_LAST_USB;
                break;
            case 2:
                pGuidi = &GUID_CLASS_MEMX_CASCADE_MUTLI_G2_FIRST_USB;
                pGuido = &GUID_CLASS_MEMX_CASCADE_MUTLI_G2_LAST_USB;
                break;
            case 3:
                pGuidi = &GUID_CLASS_MEMX_CASCADE_MUTLI_G3_FIRST_USB;
                pGuido = &GUID_CLASS_MEMX_CASCADE_MUTLI_G3_LAST_USB;
                break;
        }

        for (int i = 0; i < 2; i++) // 0 : in 1 : out
        {
            //1 Build a list of all devices that are present in the system that have enabled an interface from the pGuid interface class.
            devInfo = SetupDiGetClassDevs((i == 0) ? pGuidi : pGuido, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
            if (devInfo == INVALID_HANDLE_VALUE) {
                printf("SetupDiGetClassDevs %d failed with error 0x%x\n", index, GetLastError());
            }
            else //Get a list
            {
                //2 Enumerates the device interfaces that are contained in a device information set.
                devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
                if (FALSE == SetupDiEnumDeviceInterfaces(devInfo, NULL, (i == 0) ? pGuidi : pGuido, 0, &devInterfaceData))
                {
                    error = GetLastError();
                    if (error == ERROR_NO_MORE_ITEMS)
                    {
                        error = 0;
                        //check next group since we only expect one device in one class
                    }
                    else
                    {
                        printf("SetupDiEnumDeviceInterfaces failed with error 0x%x\n", error);
                    }
                    break;
                }

                //3 To get detailed information about an interface
                if (!SetupDiGetDeviceInterfaceDetail(devInfo, &devInterfaceData, NULL, 0, &requiredSize, NULL))
                {//3.1 Receives the required size of the DeviceInterfaceDetailData buffer.
                    error = GetLastError();
                    if (error == ERROR_INSUFFICIENT_BUFFER)
                    {
                        error = 0; //treat as non-error
                    }
                    else
                    {
                        printf("1 SetupDiGetDeviceInterfaceDetail failed with error 0x%x\n", error);
                        break;
                    }
                }

                devInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredSize);
                if (devInterfaceDetailData == NULL) {
                    printf("Unable to allocate resources for devInterfaceDetailData\n");
                    error = ERROR_NOT_ENOUGH_MEMORY;
                    break;
                }

                devInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
                if (!SetupDiGetDeviceInterfaceDetail(devInfo, &devInterfaceData, devInterfaceDetailData, requiredSize, &requiredSize, NULL))
                {//3.2 If the cbSize member is not set correctly for an input parameter, the function will fail
                    error = GetLastError();
                    printf("2 SetupDiGetDeviceInterfaceDetail failed with error 0x%x\n", error);
                }
                else
                {
                    //Record the device info here
                    g_mpu_devicemeta[devFoundNum].device_path[i] = (uint8_t*)malloc(requiredSize);
                    if (!g_mpu_devicemeta[devFoundNum].device_path[i])
                    {
                        error = MEMX_STATUS_DEVICE_OPEN_FAIL;
                    }
                    else
                    {
                        strSize = (ULONG)(wcslen(devInterfaceDetailData->DevicePath) * sizeof(WCHAR));
                        memset(g_mpu_devicemeta[devFoundNum].device_path[i], 0, requiredSize);
                        memcpy(g_mpu_devicemeta[devFoundNum].device_path[i], devInterfaceDetailData->DevicePath, strSize);
                    }
                }
                free(devInterfaceDetailData);
            }

            if (devInfo != INVALID_HANDLE_VALUE)
            {
                SetupDiDestroyDeviceInfoList(devInfo);
            }
        }

        if (g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS] && g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS_SPLIT])
        {
            g_mpu_devicemeta[devFoundNum].hif_type = MEMX_MPUIO_INTERFACE_USB;
            g_mpu_devicemeta[devFoundNum].chip_gen = MEMX_MPU_CHIP_GEN_CASCADE_PLUS;
            g_mpu_devicemeta[devFoundNum].role = MEMX_MPU_TYPE_SPLIT;
            //printf("%d Device %s %d found! %ls\n", MEMX_MPU_CHIP_GEN_CASCADE_PLUS, "USBi", devFoundNum , (PWCHAR)g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS]);
            //printf("%d Device %s %d found! %ls\n", MEMX_MPU_CHIP_GEN_CASCADE_PLUS, "USBo", devFoundNum , (PWCHAR)g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS_SPLIT]);
            devFoundNum++;
        }
        else
        {
            free(g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS]);
            free(g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS_SPLIT]);
            memset(&g_mpu_devicemeta[devFoundNum], 0, sizeof(MemxDeviceMeta));
        }
    }

    return result;
}

static LONG __device_get_kernel_version(HDEVINFO hDevInfo, ULONG device_number)
{
    LONG    result                    = -1;
    TCHAR   basePath[]                = _T(SYSTEN_REGKEY_BASEPATH);
    HKEY    hKey;
    DWORD   size;
    TCHAR   regPath[MAX_PATH];
    TCHAR   fullPath[MAX_PATH];
    TCHAR   driverVersion[DEF_BYTE(16)];
    SP_DEVINFO_DATA devInfoData;

    do {
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        if (!SetupDiEnumDeviceInfo(hDevInfo, 0, &devInfoData))
        {
            printf("SetupDiEnumDeviceInfo failed. Error: %lu\n", GetLastError());
            break;
        }

        if (!SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_DRIVER, NULL, (PBYTE)regPath, sizeof(regPath), NULL)) {
            printf("SetupDiGetDeviceRegistryProperty failed. Error: %lu\n", GetLastError());
            break;
        }

        _stprintf(fullPath, _T("%s%s"), basePath, regPath);
        result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, fullPath, 0, KEY_READ, &hKey);
        if (result != ERROR_SUCCESS) {
            printf("Failed to open registry key. Error: %ld\n", result);
        }
        else {
            size = sizeof(driverVersion);
            result = RegQueryValueEx(hKey, _T("DriverVersion"), NULL, NULL, (LPBYTE)driverVersion, &size);
            if (result != ERROR_SUCCESS) {
                printf("RegQueryValueEx get DriverVersion. Error: %ld\n", result);
            }
            else {
                memset(g_mpu_devicemeta[device_number].mpu_device_driver_version, 0, DEF_BYTE(16));
                WideCharToMultiByte(CP_ACP, 0, driverVersion, -1, (char*)g_mpu_devicemeta[device_number].mpu_device_driver_version, DEF_BYTE(16), NULL, NULL);
            }

            result = RegCloseKey(hKey);
        }
    } while (0);

    return result;
}

static memx_status __device_group_list()
{
    HDEVINFO                            devInfo                 = INVALID_HANDLE_VALUE;
    SP_DEVICE_INTERFACE_DATA            devInterfaceData        = {0};
    PSP_DEVICE_INTERFACE_DETAIL_DATA    devInterfaceDetailData  = NULL;
    ULONG                               strSize                 = 0;
    ULONG                               requiredSize            = 0;
    ULONG                               error                   = 0;
    ULONG                               devFoundNum             = 0;
    memx_status                         result                  = MEMX_STATUS_OK;

    for(ULONG index = 0; index < 2; index++) // 0 : Pcie 1 : Usb
    {
        ULONG devIndex = 0;
        const GUID *pGuid = (index == 0) ? &GUID_CLASS_MEMX_CASCADE_SINGLE_PCIE : &GUID_CLASS_MEMX_CASCADE_SINGLE_USB;

        //1 Build a list of all devices that are present in the system that have enabled an interface from the pGuid interface class.
        devInfo = SetupDiGetClassDevs(pGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (devInfo == INVALID_HANDLE_VALUE) {
            printf("SetupDiGetClassDevs %d failed with error 0x%x\n", index, GetLastError());
        }
        else //Get a list
        {
            while(devFoundNum < MEMX_DEVICE_MAX_NUMBER) //find at most MEMX_DEVICE_MAX_NUMBER device
            {
                //2 Enumerates the device interfaces that are contained in a device information set.
                devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
                if (FALSE == SetupDiEnumDeviceInterfaces(devInfo, NULL, pGuid, devIndex, &devInterfaceData))
                {
                    error = GetLastError();
                    if (error == ERROR_NO_MORE_ITEMS)
                    {
                        error = 0;  //treat as non-error
                        //printf("SetupDiEnumDeviceInterfaces %d ERROR_NO_MORE_ITEMS\n", index);
                    }
                    else
                    {
                        printf("SetupDiEnumDeviceInterfaces failed with error 0x%x\n", error);
                    }
                    break;
                }

                //3 To get detailed information about an interface
                if (!SetupDiGetDeviceInterfaceDetail(devInfo, &devInterfaceData, NULL, 0, &requiredSize, NULL))
                {//3.1 Receives the required size of the DeviceInterfaceDetailData buffer.
                    error = GetLastError();
                    if (error == ERROR_INSUFFICIENT_BUFFER)
                    {
                        error = 0; //treat as non-error
                    }
                    else
                    {
                        printf("1 SetupDiGetDeviceInterfaceDetail failed with error 0x%x\n", error);
                        break;
                    }
                }

                devInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredSize);
                if (devInterfaceDetailData == NULL) {
                    printf("Unable to allocate resources for devInterfaceDetailData\n");
                    error = ERROR_NOT_ENOUGH_MEMORY;
                    break;
                }

                devInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
                if (!SetupDiGetDeviceInterfaceDetail(devInfo, &devInterfaceData, devInterfaceDetailData, requiredSize, &requiredSize, NULL))
                {//3.2 If the cbSize member is not set correctly for an input parameter, the function will fail
                    error = GetLastError();
                    printf("2 SetupDiGetDeviceInterfaceDetail failed with error 0x%x\n", error);
                    break;
                }
                else
                {//4 The DevicePath of the device interface that can be passed to platform_open
                    //Record the device info here
                    g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS] = (uint8_t*)malloc(requiredSize);
                    if (!g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS])
                    {
                        error = MEMX_STATUS_DEVICE_OPEN_FAIL;
                    }
                    else
                    {
                        struct transport_cmd cmd = { 0 };
                        strSize = (ULONG) (wcslen(devInterfaceDetailData->DevicePath) * sizeof(WCHAR));
                        memset(g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS], 0, requiredSize);
                        memcpy(g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS], devInterfaceDetailData->DevicePath, strSize);
                        //printf("%d Device %s %d found! %ls\n", MEMX_MPU_CHIP_GEN_CASCADE_PLUS, (index == 0) ? "PCIe" : "USB ", devFoundNum , (PWCHAR)g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS]);
                        g_mpu_devicemeta[devFoundNum].hif_type = (index == 0) ? MEMX_MPUIO_INTERFACE_PCIE : MEMX_MPUIO_INTERFACE_USB;
                        g_mpu_devicemeta[devFoundNum].chip_gen = MEMX_MPU_CHIP_GEN_CASCADE_PLUS;
                        g_mpu_devicemeta[devFoundNum].role = MEMX_MPU_TYPE_SINGLE;
                        result = memx_device_submit_get_feature((uint8_t) devFoundNum, (uint8_t) FID_DEVICE_FW_INFO, &cmd);
                        if (result == MEMX_STATUS_OK) {
                            g_mpu_devicemeta[devFoundNum].fw_rollback_cnt = (cmd.CQ.data[0] & 0x0000FFFF);
                            g_mpu_devicemeta[devFoundNum].fw_version      = (cmd.CQ.data[1]);
                            __device_get_kernel_version(devInfo, devFoundNum);
                            devFoundNum++;
                            devIndex++;
                        } else {
                            free(g_mpu_devicemeta[devFoundNum].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS]);
                            memset(&g_mpu_devicemeta[devFoundNum], 0 , sizeof(MemxDeviceMeta));
                        }
                    }

                    free(devInterfaceDetailData);
                    devInterfaceDetailData = NULL;
                    if(error) {break;}
                }
            }
        } //while

        if (devInfo != INVALID_HANDLE_VALUE)
        {
            SetupDiDestroyDeviceInfoList(devInfo);
        }
        if(error)
        {
            result = MEMX_STATUS_OTHERS;
            break;
        }
        else
        {
            if(devFoundNum == MEMX_DEVICE_MAX_NUMBER)
            {
                break;
            }
        }
    } //for

    if((result == MEMX_STATUS_OK) && (devFoundNum == 0)) //no cascade+ single device found, try cascade+
    {
        __device_group_list_split();
    }

    return result;
}

memx_status memx_device_group_lock(uint8_t group_id)
{
    if (__device_group_check_group_id(group_id) != MEMX_STATUS_OK) { return MEMX_STATUS_DEVICE_INVALID_ID; }
    if (platform_share_mutex_lock(&g_mpu_device_group_guards[group_id])) { return MEMX_STATUS_DEVICE_LOCK_FAIL; }
    return MEMX_STATUS_OK;
}

memx_status memx_device_group_trylock(uint8_t group_id)
{
    if (__device_group_check_group_id(group_id) != MEMX_STATUS_OK) { return MEMX_STATUS_DEVICE_INVALID_ID; }
    if (platform_share_mutex_trylock(&g_mpu_device_group_guards[group_id])) { return MEMX_STATUS_DEVICE_LOCK_FAIL; }
    return MEMX_STATUS_OK;
}

memx_status memx_device_group_unlock(uint8_t group_id)
{
    if (__device_group_check_group_id(group_id) != MEMX_STATUS_OK) { return MEMX_STATUS_DEVICE_INVALID_ID; }
    if (platform_share_mutex_unlock(&g_mpu_device_group_guards[group_id])) { return MEMX_STATUS_DEVICE_LOCK_FAIL; }
    return MEMX_STATUS_OK;
}

memx_status memx_device_admincmd(uint8_t group_id, uint8_t chip_id, uint32_t opCode, uint32_t subOpCode, void* data)
{
    memx_status         status = MEMX_STATUS_OK;
    pMemxDeviceMeta     pDevMeta = NULL;
    platform_handle_t   fd;
    platform_handle_t   event;
    int32_t ret = 0;
    struct transport_cmd *      cmd = (struct transport_cmd *)data;
    do {
        pDevMeta = memx_device_group_get_devicemeta(group_id);
        if (!pDevMeta) {
            status = MEMX_STATUS_DEVICE_INVALID_ID;
            break;
        }

        event = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (!event) {
            status = MEMX_STATUS_OTHERS;
            break;
        }

        if(opCode != MEMX_ADMIN_CMD_DEVIOCTRL) {
            cmd->SQ.opCode      = opCode;
            cmd->SQ.cmdLen      = sizeof(struct transport_cmd);
            cmd->SQ.subOpCode   = subOpCode;
            cmd->SQ.reqLen      = sizeof(struct transport_cq);
            cmd->SQ.cdw2        = chip_id;
        }
        fd  = platform_open(pDevMeta->device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS], 0);
        ret = platform_ioctl(event, &fd, MEMX_ADMIN_COMMAND, data, cmd->SQ.cmdLen);

        platform_close(&event);
        platform_close(&fd);

        if (ret != 0) {
            status = MEMX_STATUS_OTHERS;
            break;
        }
    } while (0);

    return status;
}

memx_status memx_device_submit_get_feature(uint8_t group_id, uint8_t feature_id, void* data)
{
    memx_status         status      = MEMX_STATUS_OK;
    pMemxDeviceMeta     pDevMeta    = NULL;
    platform_handle_t   fd;
    platform_handle_t   event;
    int32_t ret                     = 0;
    struct transport_cmd *      cmd         = (struct transport_cmd *) data;
    do{
        pDevMeta = memx_device_group_get_devicemeta(group_id);
        if(!pDevMeta){
            status = MEMX_STATUS_DEVICE_INVALID_ID;
            break;
        }

        event = CreateEvent(NULL, FALSE, FALSE, NULL);
        if(!event){
            status = MEMX_STATUS_OTHERS;
            break;
        }

        cmd->SQ.opCode    = MEMX_ADMIN_CMD_GET_FEATURE;
        cmd->SQ.cmdLen    = sizeof(struct transport_cmd);
        cmd->SQ.subOpCode = feature_id;
        cmd->SQ.reqLen    = sizeof(struct transport_cq);
        fd  = platform_open(pDevMeta->device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS], 0);
        ret = platform_ioctl(event, &fd, MEMX_GET_DEVICE_FEATURE, data, cmd->SQ.cmdLen);

        platform_close(&event);
        platform_close(&fd);

        if(ret != 0){
            status = MEMX_STATUS_OTHERS;
            break;
        }

        if(feature_id == FID_DEVICE_INFO){
            if(pDevMeta){ //Windows kernel driver version fill here
                memcpy(&cmd->CQ.data[7], &pDevMeta->mpu_device_driver_version[0], sizeof(unsigned int));
                memcpy(&cmd->CQ.data[8], &pDevMeta->mpu_device_driver_version[4], sizeof(unsigned int));
            }
        }
    }while(0);

    return status;
}

memx_status memx_device_submit_set_feature(uint8_t group_id, uint8_t feature_id, void* data)
{
    memx_status         status      = MEMX_STATUS_OK;
    pMemxDeviceMeta     pDevMeta    = NULL;
    platform_handle_t   fd;
    platform_handle_t   event;
    int32_t ret                     = 0;
    struct transport_cmd *      cmd         = (struct transport_cmd *) data;
    do{
        pDevMeta = memx_device_group_get_devicemeta(group_id);
        if(!pDevMeta){
            status = MEMX_STATUS_DEVICE_INVALID_ID;
            break;
        }

        event = CreateEvent(NULL, FALSE, FALSE, NULL);
        if(!event){
            status = MEMX_STATUS_OTHERS;
            break;
        }

        cmd->SQ.opCode      = MEMX_ADMIN_CMD_SET_FEATURE;
        cmd->SQ.cmdLen      = sizeof(struct transport_cmd);
        cmd->SQ.subOpCode   = feature_id;
        cmd->SQ.reqLen      = sizeof(struct transport_cq);
        if (feature_id == FID_DEVICE_FREQUENCY) {
            cmd->SQ.cdw4 = 0x6d656d66;
        }
        fd  = platform_open(pDevMeta->device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS], 0);
        ret = platform_ioctl(event, &fd, MEMX_SET_DEVICE_FEATURE, data, cmd->SQ.cmdLen);

        platform_close(&event);
        platform_close(&fd);

        if(ret != 0){
            status = MEMX_STATUS_OTHERS;
            break;
        }
    }while(0);

    return status;
}

memx_status memx_device_download_fw(uint8_t group_id, const char *fw_data, uint8_t type)
{
    memx_status         status      = MEMX_STATUS_OK;
    pMemxDeviceMeta     pDevMeta    = NULL;
    platform_handle_t   fd          = INVALID_HANDLE_VALUE;
    platform_handle_t   event       = INVALID_HANDLE_VALUE;
    int32_t             ret         = 0;
    platform_uint32_t   *data       = NULL;
    platform_uint32_t   size        = 0;

    do{
        pDevMeta = memx_device_group_get_devicemeta(group_id);
        if(!pDevMeta){
            status = MEMX_STATUS_DEVICE_INVALID_ID;
            break;
        }

        if(type == MEMX_DOWNLOAD_TYPE_FROM_BUFFER){
            if(fw_data == NULL){
                status = MEMX_STATUS_NULL_POINTER;
                break;
            } else {
                data = (platform_uint32_t *)fw_data;
            }

            status = __device_group_fw_image_check(data, &size);
            if(status){
                break;
            }
        } else if (type == MEMX_DOWNLOAD_TYPE_FROM_FILE){
            data = (platform_uint32_t *) malloc(MAX_FW_SIZE);
            FILE *fp = fopen(fw_data, "rb");
            if(data && fp) {
                size = (platform_uint32_t) fread(data, 1, MAX_FW_SIZE, fp);
                fclose(fp);

                status = __device_group_fw_image_check(data, &size);
                if(status){
                    break;
                }
            } else {
                status = MEMX_STATUS_FILE_NOT_FOUND;
                break;
            }
        } else {
                status = MEMX_STATUS_INVALID_PARAMETER;
                break;
        }

        event = CreateEvent(NULL, FALSE, FALSE, NULL);
        if(!event){
            status = MEMX_STATUS_OTHERS;
            break;
        }

        __device_qspi_reset_release(group_id);

        fd  = platform_open(pDevMeta->device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS], 0);
        ret = platform_ioctl(event, &fd, MEMX_DOWNLOAD_FIRMWARE, data, size);

        if(ret != 0){
            status = MEMX_STATUS_PLATFORM_IOCTL_FAIL;
            break;
        }
    } while(0);

    if(data && (type == MEMX_DOWNLOAD_TYPE_FROM_FILE)){
        free(data);
    }

    if(event){
        platform_close(&event);
    }

    if(fd != INVALID_HANDLE_VALUE){
        platform_close(&fd);
    }

    return status;
}
#endif

static void __device_qspi_reset_release(uint8_t group_id)
{
    memx_device_set_feature(group_id, 0, OPCODE_SET_QSPI_RESET_RELEASE, 1);
}

static void __device_group_clean()
{
    for (uint32_t devindex = 0; devindex < MEMX_DEVICE_MAX_NUMBER; devindex++)
    {
        free(g_mpu_devicemeta[devindex].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS]);
        free(g_mpu_devicemeta[devindex].device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS_SPLIT]);
        free(g_mpu_devicemeta[devindex].device_path[MEMX_DEVICE_TYPE_FEATURE]);
    }
    memset(g_mpu_devicemeta, 0 , sizeof(g_mpu_devicemeta));
}

static void __device_group_decease_refcount(uint8_t group_id)
{
    pMemxDeviceGroup pDevice = memx_device_group_get_device(group_id);
    if (pDevice) {
        platform_share_mutex_lock(&g_mpu_device_groups_lock_ptr);
        if (pDevice->ref_count) pDevice->ref_count--;
        platform_share_mutex_unlock(&g_mpu_device_groups_lock_ptr);
    }
}

static void __device_group_increase_refcount(uint8_t group_id)
{
    pMemxDeviceGroup pDevice = memx_device_group_get_device(group_id);
    if (pDevice) {
        platform_share_mutex_lock(&g_mpu_device_groups_lock_ptr);
        if (pDevice->ref_count < UINT_MAX) pDevice->ref_count++;
        platform_share_mutex_unlock(&g_mpu_device_groups_lock_ptr);
    }
}

static void __device_group_set_device(uint8_t group_id, pMemxDeviceGroup device)
{
    if (__device_group_check_group_id(group_id) == MEMX_STATUS_OK) {
        g_mpu_device_groups[group_id] = device;
    }
}

static memx_status __device_group_check_group_id(uint8_t group_id)
{   //Check device group id is valid or not  1. < MAX_Number  2. detect by __device_group_list
    //even group_id is valid g_mpu_device_groups[group_id] may not be created yet (NULL)
    memx_status result = ((group_id < MEMX_DEVICE_MAX_NUMBER) && g_mpu_devicemeta[group_id].hif_type != 0) ? MEMX_STATUS_OK : MEMX_STATUS_DEVICE_INVALID_ID;
    return result;
}

static memx_status __device_group_check_chip_gen_isvalid(uint8_t chip_gen)
{
    memx_status result = MEMX_STATUS_OK;
    switch(chip_gen)
    {
        case MEMX_MPU_CHIP_GEN_CASCADE_PLUS:
            break;
        default:
            result = MEMX_STATUS_MPU_INVALID_CHIP_GEN;
            break;
    }

    return result;
}

pMemxDeviceMeta memx_device_group_get_devicemeta(uint8_t group_id)
{
    pMemxDeviceMeta pDevMeta = (__device_group_check_group_id(group_id) == MEMX_STATUS_OK) ?  &g_mpu_devicemeta[group_id] : NULL;
    if(pDevMeta){
        if(!pDevMeta->device_path[MEMX_DEVICE_TYPE_CASCADE_PLUS]){
            pDevMeta = NULL;
        }
    }

    return pDevMeta;
}

pMemxDeviceGroup memx_device_group_get_device(uint8_t group_id)
{   //Check device group structure has been create or not
    pMemxDeviceGroup result = (__device_group_check_group_id(group_id) == MEMX_STATUS_OK) ? g_mpu_device_groups[group_id] : NULL;
    return result;
}

void memx_device_group_init_all()
{
    __device_group_list();
    __device_group_sharelock_create();
    __device_group_guard_init_all();
}

void memx_device_group_del_all()
{
    for (uint8_t group_idx = 0; group_idx < MEMX_DEVICE_MAX_NUMBER; ++group_idx) {
        pMemxDeviceGroup pDevice = memx_device_group_get_device(group_idx);
        if (pDevice) { //Dont consider device reference count
            memx_mpuio_destroy(pDevice->mpuio);
            free(pDevice);
            __device_group_set_device(group_idx, NULL);
        }
    }
    __device_group_guard_del_all();
    __device_group_sharelock_destory();
    __device_group_clean();
}

/**
 * @brief This function should only create new instance if specified device
 * group instance has not been created before and be recorded. Otherwise should
 * add reference to the instance and return it instead.
 *
 * @param group_id            MPU device group ID
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_device_group_create(uint8_t group_id)
{
    memx_status status = MEMX_STATUS_OK;

    do{
        status = __device_group_check_group_id(group_id);
        if (status != MEMX_STATUS_OK){
            break;
        }

        // add reference to device group if it already exists
        if (memx_device_group_get_device(group_id)) {
            __device_group_increase_refcount(group_id);
        }
        else
        {
            // otherwise, try to create new device group
            pMemxDeviceGroup pDevice = (pMemxDeviceGroup) malloc(sizeof(MemxDeviceGroup));
            if (!pDevice) {
                status = MEMX_STATUS_OUT_OF_MEMORY;
            } else {
                memset(pDevice, 0, sizeof(MemxDeviceGroup));
                __device_group_set_device(group_id, pDevice);
                __device_group_increase_refcount(group_id); // add reference to driver if open interface success

                if(__device_group_check_chip_gen_isvalid(g_mpu_devicemeta[group_id].chip_gen) == MEMX_STATUS_OK){
                    pDevice->mpuio = memx_mpuio_init(group_id, g_mpu_devicemeta[group_id].hif_type, g_mpu_devicemeta[group_id].chip_gen);
                }else{
                    pDevice->mpuio = NULL;
                }

                if (!pDevice->mpuio) {
                    memx_device_group_destory(group_id); // minus reference here if open unsuccessfully
                    status = MEMX_STATUS_MPUIO_OPEN_FAIL;
                }
            }
        }
    }while(0);

    return status;
}

/**
 * @brief Only destroys driver instance in case no other one refers to it.
 * Otherwise, decreases reference count and leaves.
 *
 * @param group_id            MPU device group ID
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_device_group_destory(uint8_t group_id)
{
    memx_status status = MEMX_STATUS_OK;
    do{
        status = __device_group_check_group_id(group_id);
        if (status != MEMX_STATUS_OK){
            break;
        }

        pMemxDeviceGroup pDevice = memx_device_group_get_device(group_id);
        if (pDevice) { // skips if instance is already empty
            // decreases reference count
            __device_group_decease_refcount(group_id);

            // destroys mpuio and device group if reference count goes down to zero
            if (pDevice->ref_count == 0) {
                memx_mpuio_destroy(pDevice->mpuio);
                free(pDevice);
                __device_group_set_device(group_id, NULL);
            }
        }
    }while(0);

    return status;
}

/**
 * @brief Get device count by checking MEMX_DEVICE_MAX_NUMBER
 *
 * @param pData Buffer from host to get device count
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_get_device_count(void *pData)
{
    memx_status status = MEMX_STATUS_OK;
    uint32_t count = 0;

    if (!pData) {
        status = MEMX_STATUS_NULL_POINTER;
    }
    else {
        for (uint8_t device = 0; device < MEMX_DEVICE_MAX_NUMBER; device++) {
            if (__device_group_check_group_id(device) == MEMX_STATUS_OK) {
                count++;
            }
        }
        *(uint32_t*)pData = count;
    }

    return status;
}

/**
 * @brief Get mpu group count by checking device group
 *
 * @param[in] group_id Device group index
 * @param[in] pData    Data buffer
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_get_mpu_group_count(int8_t group_id, void *pData)
{
    memx_status status = memx_device_group_create(group_id);
    pMemxDeviceGroup pDevice = memx_device_group_get_device(group_id);

    if (pDevice == NULL) {
        status = MEMX_STATUS_DEVICE_INVALID_ID;
    } else {
        if (!pData) {
            status = MEMX_STATUS_NULL_POINTER;
        } else {
            *(uint32_t*)pData = pDevice->mpuio->hw_info.chip.group_count;
        }
        memx_device_group_destory(group_id);
    }

    return status;
}

/**
 * @brief Get mpu group count by checking device group
 *
 * @param[in] group_id            Device group index
 * @param[in] mpu_group_config    MPU group config
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_device_config_mpu_group(uint8_t group_id, uint8_t mpu_group_config)
{
    memx_status status = memx_device_group_create(group_id);
    pMemxDeviceGroup pDevice = memx_device_group_get_device(group_id);
    if (status == MEMX_STATUS_OK && pDevice) {
        status = memx_mpuio_operation(pDevice->mpuio, 0, MEMX_CMD_CONFIG_MPU_GROUP, &mpu_group_config, sizeof(uint8_t), 0);
    }
    memx_device_group_destory(group_id);

    return status;
}

/**
 * @brief Get mpu chip count by checking device group
 *
 * @param[in] group_id      Device group index
 * @param[in] chip_count    Chip count data
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_device_get_total_chip_count(uint8_t group_id, uint8_t* chip_count)
{
    memx_status status = memx_device_group_create(group_id);
    pMemxDeviceGroup pDevice = memx_device_group_get_device(group_id);

    if (status == MEMX_STATUS_OK && pDevice) {
        if(chip_count){
            status = memx_mpuio_operation(pDevice->mpuio, 0, MEMX_CMD_READ_TOTAL_CHIP_COUNT, chip_count, sizeof(uint8_t), 0);
        } else {
            status =  MEMX_STATUS_NULL_POINTER;
        }
    }

    memx_device_group_destory(group_id);

    return status;
}

memx_status memx_device_antirollback_check(uint8_t group_id){
    memx_status         status      = MEMX_STATUS_OK;
    pMemxDeviceMeta     pDevMeta    = NULL;

    pDevMeta = memx_device_group_get_devicemeta(group_id);
    if (!pDevMeta) {
        status = MEMX_STATUS_DEVICE_INVALID_ID;
    } else {
        switch(pDevMeta->chip_gen){
            case MEMX_MPU_CHIP_GEN_CASCADE_PLUS:
                if(MIN_FW_ANTIROLLBACK_CNT > pDevMeta->fw_rollback_cnt){
                    status = MEMX_STATUS_DEVICE_OPEN_FAIL;
                    printf("Driver reqruied firmware anti_rollback cnt >= %ld\r\n", MIN_FW_ANTIROLLBACK_CNT);
                    printf("Cur firmware: cnt %d ver 0x%x\r\n", pDevMeta->fw_rollback_cnt, pDevMeta->fw_version);
                }
                break;
            default:
                printf("Non-Support Device Gen 0x%x\r\n", pDevMeta->chip_gen);
                status = MEMX_STATUS_DEVICE_OPEN_FAIL;
                break;
        }
    }

    return status;
}

memx_status memx_device_get_feature(uint8_t group_id, uint8_t chip_id, memx_get_feature_opcode opcode, void* buffer)
{
    memx_status status = MEMX_STATUS_OK;
    struct transport_cmd cmd;
    uint8_t feature_id = 0;

    status = __device_group_check_group_id(group_id);
    if(memx_status_no_error(status)) {
        if (buffer == null) {
            status = MEMX_STATUS_NULL_POINTER;
        } else {
            if (MEMX_IS_GET_FEATURE_OPERATION_OPCODE(opcode)) {
                status = memx_device_group_create(group_id);
                pMemxDeviceGroup pDevice = memx_device_group_get_device(group_id);

                if ((status == MEMX_STATUS_OK) && pDevice && pDevice->mpuio) {
                    switch (opcode) {
                    case OPCODE_GET_IFMAP_CONTROL:
                        *((uint32_t *)buffer) = pDevice->mpuio->ifmap_control;
                        break;
                    case OPCODE_GET_QSPI_RESET_RELEASE:
                        status = memx_mpuio_operation(pDevice->mpuio, 0, MEMX_CMD_GET_QSPI_RESET_RELEASE, buffer, sizeof(uint32_t), 0);
                        break;
                    default:
                        break;
                    }
                }

                memx_device_group_destory(group_id);
            } else {
                status = memx_device_get_feature_opcode_to_feature_id(opcode, &feature_id);

                if (status == MEMX_STATUS_OK) {
                    memset(&cmd, 0 , sizeof(struct transport_cmd));
                    cmd.SQ.cdw2 = chip_id;
                    status = memx_device_submit_get_feature(group_id, feature_id, &cmd);

                    if (status == MEMX_STATUS_OK) {
                        if(cmd.CQ.status == ERROR_STATUS_NO_ERROR){
                            memx_device_fillup_feature_data(cmd, opcode, buffer, group_id);
                        } else {
                            status = memx_device_feature_error_code_transform(chip_id, feature_id, cmd);
                        }
                    }
                }
            }
        }
    }

    return status;
}

memx_status memx_device_get_feature_opcode_to_feature_id(memx_get_feature_opcode opcode, uint8_t* feature_id)
{
    memx_status status = MEMX_STATUS_OK;

    if ((opcode >= OPCODE_GET_MANUFACTURERID) && (opcode <= OPCODE_GET_KDRIVER_VERSION)) {
        *feature_id = FID_DEVICE_INFO;
    } else if ((opcode >= OPCODE_GET_TEMPERATURE) && (opcode <= OPCODE_GET_THERMAL_THRESHOLD)) {
        *feature_id = FID_DEVICE_TEMPERATURE;
    } else if (opcode == OPCODE_GET_FREQUENCY) {
        *feature_id = FID_DEVICE_FREQUENCY;
    } else if (opcode == OPCODE_GET_VOLTAGE) {
        *feature_id = FID_DEVICE_VOLTAGE;
    } else if (opcode == OPCODE_GET_THROUGHPUT) {
        *feature_id = FID_DEVICE_THROUGHPUT;
    } else if (opcode == OPCODE_GET_POWER) {
        *feature_id = FID_DEVICE_POWER;
    } else if (opcode == OPCODE_GET_POWERMANAGEMENT) {
        *feature_id = FID_DEVICE_POWERMANAGEMENT;
    } else if (opcode == OPCODE_GET_POWER_ALERT) {
        *feature_id = FID_DEVICE_POWER_ALERT;
    } else if (opcode == OPCODE_GET_MODULE_INFORMATION) {
        *feature_id = FID_DEVICE_FW_INFO;
    } else if (opcode == OPCODE_GET_INTERFACE_INFO) {
        *feature_id = FID_DEVICE_INTERFACE_INFO;
    } else if (opcode == OPCODE_GET_HW_INFO) {
        *feature_id = FID_DEVICE_HW_INFO;
    } else if (opcode == OPCODE_GET_MPU_UTILIZATION) {
        *feature_id = FID_DEVICE_MPU_UTILIZATION;
    } else if (opcode == OPCODE_GET_FREQUENCY_EFFECTIVE) {
        *feature_id = FID_DEVICE_FREQUENCY_EFFECTIVE;
    } else if (opcode == OPCDOE_GET_DEVICE_DMA_TRIGGER_TYPE) {
        *feature_id = FID_DEVICE_DMA_TRIGGER_TYPE;
    } else {
        status = MEMX_STATUS_INVALID_PARAMETER;
    }

    return status;
}

void memx_device_fillup_feature_data(struct transport_cmd cmd, memx_get_feature_opcode opcode, uint64_t* buffer, uint8_t group_id)
{
    if ((opcode == OPCODE_GET_FREQUENCY) || (opcode == OPCODE_GET_VOLTAGE) || (opcode == OPCODE_GET_POWERMANAGEMENT) ||
        (opcode == OPCODE_GET_POWER_ALERT) || (opcode == OPCODE_GET_MPU_UTILIZATION) || (opcode == OPCODE_GET_FREQUENCY_EFFECTIVE) ||
        (opcode == OPCDOE_GET_DEVICE_DMA_TRIGGER_TYPE)) {
        *buffer = cmd.CQ.data[0];
    } else if (opcode == OPCODE_GET_TEMPERATURE) {
        *buffer = (cmd.CQ.data[30]) ? (cmd.CQ.data[4] & 0xFFFF) : (cmd.CQ.data[0]);
    } else if (opcode == OPCODE_GET_POWER) {
        *buffer = (cmd.CQ.data[0] < MEMX_INVALID_DATA) ? ((uint64_t)POWER_CORRECTION(cmd.CQ.data[0])) : ((uint64_t)cmd.CQ.data[0]);
    } else if (opcode == OPCODE_GET_FW_COMMIT) {
        *buffer = cmd.CQ.data[1];
    } else if (opcode == OPCODE_GET_THERMAL_STATE) {
        *buffer = (cmd.CQ.data[30]) ? ((cmd.CQ.data[4] >> 20) & 0xF) : (cmd.CQ.data[1]);
    } else if (opcode == OPCODE_GET_DATE_CODE) {
        *buffer = cmd.CQ.data[2];
    } else if (opcode == OPCODE_GET_THERMAL_THRESHOLD) {
        *buffer = (cmd.CQ.data[30]) ? (cmd.CQ.data[5] & 0xFF) : (cmd.CQ.data[2]);
    } else if (opcode == OPCODE_GET_COLD_WARM_REBOOT_COUNT) {
        *buffer = cmd.CQ.data[3];
    } else if (opcode == OPCODE_GET_WARM_REBOOT_COUNT) {
        *buffer = cmd.CQ.data[4];
    } else if (opcode == OPCODE_GET_MANUFACTURERID) {
        *buffer = cmd.CQ.data[6];
        *buffer = (*buffer << 32);
        *buffer |= cmd.CQ.data[5];
    } else if (opcode == OPCODE_GET_KDRIVER_VERSION) {
        char version[8];
        memcpy(version, &cmd.CQ.data[7], sizeof(unsigned int));
        memcpy(&version[sizeof(unsigned int)], &cmd.CQ.data[8], sizeof(unsigned int));
        memcpy(buffer, version, sizeof(uint64_t));
    } else if (opcode == OPCODE_GET_THROUGHPUT) {
        memcpy(buffer, cmd.CQ.data, sizeof(unsigned int) * 16);
    } else if (opcode == OPCODE_GET_MODULE_INFORMATION) {
        *buffer = (cmd.CQ.data[3] >> 7) & 0x3;  // boot source, 0:QSPI, 1:USB, 2:PCIe, 3:UART
        *buffer = (*buffer << 32);
        *buffer |= (cmd.CQ.data[4] & 0xF);      // chip version of device, 0:a0, 5:a1
    } else if (opcode == OPCODE_GET_INTERFACE_INFO) {
        *buffer = 0;
        *buffer |= (cmd.CQ.data[0] & 0x0F);
        *buffer |= ((cmd.CQ.data[0] & 0x3F0) << 4); // PCI_EXP_LNKCAP
        *buffer = (*buffer << 16);
        *buffer |= (cmd.CQ.data[1] & 0x0F);
        *buffer |= ((cmd.CQ.data[1] & 0x3F0) << 4); // PCI_EXP_LNKSTA
        *buffer = (*buffer << 32);
        *buffer |= g_mpu_devicemeta[group_id].hif_type;
    } else if (opcode == OPCODE_GET_HW_INFO) {
        *buffer = (uint64_t)(cmd.CQ.data[0] & 0xFF) |        // hw_info.chip.generation
                 ((uint64_t)(cmd.CQ.data[1] & 0xFF) << 16) | // hw_info.chip.total_chip_cnt
                 ((uint64_t)(cmd.CQ.data[2] & 0xFF) << 32) | // hw_info.chip.curr_config_chip_count
                 ((uint64_t)(cmd.CQ.data[3] & 0xFF) << 48);  // hw_info.chip.group_count
    }
}

memx_status memx_device_set_feature(uint8_t group_id, uint8_t chip_id, memx_set_feature_opcode opcode, uint16_t parameter)
{
    memx_status status = MEMX_STATUS_OK;
    struct transport_cmd cmd = {0};
    uint8_t feature_id = 0;
    uint8_t max_loop = 1;
    uint8_t loop = 0;

    do{
        status = __device_group_check_group_id(group_id);
        if(memx_status_error(status)) {
            break;
        }

        if (opcode == OPCODE_SET_IFMAP_CONTROL) {
            pMemxDeviceGroup pDevice = memx_device_group_get_device(group_id);
            if (pDevice && pDevice->mpuio) {
                pDevice->mpuio->ifmap_control = parameter;
            } else {
                status = MEMX_STATUS_DEVICE_NOT_OPENED;
            }

        } else if (opcode == OPCODE_SET_QSPI_RESET_RELEASE) {
            status = memx_device_group_create(group_id);
            pMemxDeviceGroup pDevice = memx_device_group_get_device(group_id);

            if ((status == MEMX_STATUS_OK) && pDevice && pDevice->mpuio) {
                status = memx_mpuio_operation(pDevice->mpuio, 0, MEMX_CMD_SET_QSPI_RESET_RELEASE, &parameter, sizeof(uint32_t), 0);
            }

            memx_device_group_destory(group_id);
        } else {
            status = memx_device_set_feature_opcode_to_feature_id(opcode, &feature_id);
            if(memx_status_no_error(status)) {
                if ((feature_id == FID_DEVICE_FREQUENCY) && (chip_id == MEMX_DEVICE_FEATURE_ALL_CHIP)) {
                    max_loop = MEMX_MPU_MAX_CHIP_ID;
                }

                cmd.CQ.status = ERROR_STATUS_NO_ERROR;
                for (loop = 0; loop < max_loop; loop++) {
                    if (cmd.CQ.status == ERROR_STATUS_NO_ERROR) {
                        memset(&cmd, 0 , sizeof(struct transport_cmd));
                        cmd.SQ.cdw2 = (max_loop == 1) ? chip_id : loop;
                        cmd.SQ.cdw3 = parameter;

                        status = memx_device_submit_set_feature(group_id, feature_id, &cmd);
                        if(memx_status_error(status)){
                            break;
                        }
                    } else {
                        break;
                    }
                }

                if(memx_status_no_error(status)){
                     status = memx_device_feature_error_code_transform(chip_id, feature_id, cmd);
                }
            }
        }

    }while(0);

    return status;
}

memx_status memx_device_set_feature_opcode_to_feature_id(memx_set_feature_opcode opcode, uint8_t* feature_id)
{
    memx_status status = MEMX_STATUS_OK;

    if (opcode == OPCODE_SET_THERMAL_THRESHOLD) {
        *feature_id = FID_DEVICE_TEMPERATURE;
    } else if (opcode == OPCODE_SET_FREQUENCY) {
        *feature_id = FID_DEVICE_FREQUENCY;
    } else if (opcode == OPCODE_SET_VOLTAGE) {
        *feature_id = FID_DEVICE_VOLTAGE;
    } else if (opcode == OPCODE_SET_POWERMANAGEMENT) {
        *feature_id = FID_DEVICE_POWERMANAGEMENT;
    } else if (opcode == OPCODE_SET_POWER_THRESHOLD) {
        *feature_id = FID_DEVICE_POWER_THRESHOLD;
    } else if (opcode == OPCODE_SET_POWER_ALERT_FREQUENCY) {
        *feature_id = FID_DEVICE_POWER_ALERT_FREQUENCY;
    } else if (opcode == OPCDOE_SET_DEVICE_DMA_TRIGGER_TYPE) {
        *feature_id = FID_DEVICE_DMA_TRIGGER_TYPE;
    } else {
        status = MEMX_STATUS_INVALID_PARAMETER;
    }

    return status;
}

memx_status memx_device_feature_error_code_transform(uint8_t chip_id, uint8_t feature_id, struct transport_cmd cmd)
{
    memx_status status = MEMX_STATUS_OK;

    if ((cmd.CQ.status > ERROR_STATUS_NO_ERROR) && (cmd.CQ.status < ERROR_STATUS_UNKNOWN_FAIL)) {
        status = MEMX_STATUS_INVALID_PARAMETER;
    } else if (cmd.CQ.status == ERROR_STATUS_UNKNOWN_FAIL) {
        status = MEMX_STATUS_OTHERS;
    } else if (cmd.CQ.status == ERROR_STATUS_TIMEOUT_FAIL) {
        status = MEMX_STATUS_TIMEOUT;
    }

    if ((feature_id == FID_DEVICE_FREQUENCY) && (chip_id == MEMX_DEVICE_FEATURE_ALL_CHIP) && (cmd.CQ.status == ERROR_STATUS_PARAMETER_FAIL)) {
        status = MEMX_STATUS_OK;
    }

    return status;
}
