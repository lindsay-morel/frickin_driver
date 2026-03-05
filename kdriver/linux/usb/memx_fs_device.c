// SPDX-License-Identifier: GPL-2.0+
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/firmware.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include "../include/memx_ioctl.h"
#include "memx_cascade_usb.h"
#include "memx_fs.h"
#include "memx_fs_device.h"
#include "memx_cascade_debugfs.h"

extern int memx_admin_trigger(struct transport_cmd *pCmd, struct memx_data *data);

static char *g_usage[19] = {
    "Usage: echo \"fwlog chip_id[0-7] [hex_addr] [hex_val]\" > cmd\n",
    "==========================================================================================\n",
    "Ex: Dump chip(0) firmware log\n",
    "echo \"fwlog 0\" > cmd; sudo dmesg\n",
    "Ex: Dump chip(1) firmware log\n",
    "echo \"fwlog 1\" > cmd; sudo dmesg\n\n",
    "Ex: Read chip(0) address 0x400A0000\n",
    "echo \"read 0 0x400A0000\" > debug; sleep 1; echo \"fwlog 0\" > cmd; sudo dmesg\n",
    "Ex: Write chip(0) address [0x400A0000]=0x12345678\n",
    "echo \"write 0 0x400A0000 0x12345678\" > debug; sleep 1; echo \"fwlog 0\" > cmd; sudo dmesg\n\n",
    "Ex: Issue chip(0) memx uart command\n",
    "echo \"memx0 0\" > cmd;sleep 1; echo \"fwlog 0\" > cmd; sudo dmesg\n",
    "echo \"memx1 0\" > cmd;sleep 1; echo \"fwlog 0\" > cmd; sudo dmesg\n",
    "echo \"memx2 0\" > cmd;sleep 1; echo \"fwlog 0\" > cmd; sudo dmesg\n",
    "echo \"memx3 0\" > cmd;sleep 1; echo \"fwlog 0\" > cmd; sudo dmesg\n",
    "echo \"memx6 0\" > cmd;sleep 1; echo \"fwlog 0\" > cmd; sudo dmesg\n",
    "echo \"memx7 0\" > cmd;sleep 1; echo \"fwlog 0\" > cmd; sudo dmesg\n",
    "echo \"memx8 0\" > cmd;sleep 1; echo \"fwlog 0\" > cmd; sudo dmesg\n",
    "==========================================================================================\n"
};

static char *debug_usage[5] = {
    "Usage: echo \"rmtcmd chip_id[0-7] [hex_cmdcode] [hex_cmdparam]\" > debug\n",
    "==========================================================================================\n",
    "Ex: Issue chip(0) command memx0\n",
    "echo \"rmtcmd 0 0x6d656d30 0x0\" > debug; sleep 1; echo \"fwlog 0\" > cmd; sudo dmesg\n",
    "==========================================================================================\n"
};

static char *i2ctrl_usage[20] = {
    "\nUsage: echo \"[rw-byte-cnt] data0 data0-param data1 data1-param ... dataN dataN-param \" > i2ctrl\n",
    "[rw-byte-cnt] must be even number because each data byte must specified related paramter to assign such as I2C-START,STOP,NACK on i2c bus transaction\n",
    "parameter byte definition: bit[0]-START / bit[1]-STOP / bit[2]-NACK / bit[4]=0 means WRITE-data / bit[4]=1 means READ-data\n",
    "Ex: you need to set i2c slave address byte with START bit asserted to match i2c protocol\n",
    "=================================================================================================\n",
    "For example : 8bits-Salve address 0xC0 and you wait to read address 0x1234 and data length 2 byte\n",
    "Then the command should like this: echo \"12 0xC0 0x01 0x12 0x00 0x34 0x00 0xC1 0x01 0x00 0x10 0x00 0x16\" > i2ctrl; sudo dmesg | tail -n 10\n",
    "(12): there are 12 bytes in this console command follows\n",
    "(0xC0 0x01): This means i2c bus transmit BYTE[0]=0xC0 with START bit is set, This is WRITE data byte\n",
    "(0x12 0x00): This means i2c bus transmit BYTE[1]=0x12 ,no START/STOP/NACK should sned, This is WRITE data byte\n",
    "(0x34 0x00): This means i2c bus transmit BYTE[2]=0x34 ,no START/STOP/NACK should sned, This is WRITE data byte\n",
    "(0xC1 0x01): This means i2c bus transmit BYTE[3]=0xC1 with START bit is set, This is WRITE data byte, bit[0]=1 means read in the following data\n",
    "(0x00 0x10): This means i2c bus transmit BYTE[4] is READ data, the data field 0x00 here is dont care.\n",
    "(0x00 0x16): This means i2c bus transmit BYTE[5] is READ data, the data field 0x00 here is dont care. and also send NACK/STOP when this byte completed\n",
    "after completed, the BYTE[4]BYTE[5]read data value would shown on kernel messages to check\n",
    "=================================================================================================\n",
    "For example : 8bits-Salve address 0xB4 and you want to send PMBUS_VOUT_COMMAND(0x21) with value 0x0D99\n",
    "Then the command should like this: echo \"8 0xb4 0x01 0x21 0x00 0x99 0x00 0x0d 0x02\" > i2ctrl; sudo dmesg | tail -n 10\n",
    "This command can read back to confirm: echo \"10 0xb4 0x01 0x21 0x00 0xb5 0x01 0x00 0x10 0x00 0x16\" > i2ctrl; sudo dmesg | tail -n 10\n",
    "=================================================================================================\n"
};

/* ========================================================================= */
/* Sysfs Show / Store Functions                                              */
/* ========================================================================= */

static ssize_t cmd_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    s32 res = 0;
    int i;
    for (i = 0; i < 19; i++)
        res += scnprintf(buf + res, PAGE_SIZE - res, "%s", g_usage[i]);
    return res;
}

static ssize_t debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    s32 res = 0;
    int i;
    for (i = 0; i < 5; i++)
        res += scnprintf(buf + res, PAGE_SIZE - res, "%s", debug_usage[i]);
    return res;
}

static ssize_t i2ctrl_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    s32 res = 0;
    int i;
    for (i = 0; i < 20; i++)
        res += scnprintf(buf + res, PAGE_SIZE - res, "%s", i2ctrl_usage[i]);
    return res;
}

static ssize_t gpioctrl_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct memx_data *memx_dev = dev_get_drvdata(dev);
    struct transport_cmd cmd = {0};

    if (!memx_dev) return -ENODEV;

    cmd.SQ.opCode    = MEMX_ADMIN_CMD_GET_FEATURE;
    cmd.SQ.subOpCode = FID_DEVICE_GPIO;
    cmd.SQ.cdw2      = ((memx_dev->gpio_r >> 8) & 0xF); // chip id
    cmd.CQ.data[0]   = memx_dev->gpio_r & 0xFF;

    mutex_lock(&memx_dev->cfglock);
    memx_admin_trigger(&cmd, memx_dev);
    mutex_unlock(&memx_dev->cfglock);

    return scnprintf(buf, PAGE_SIZE, "%d (chip%d io%d)\n",
                     cmd.CQ.data[1], ((memx_dev->gpio_r >> 8) & 0xF), ((memx_dev->gpio_r >> 0) & 0xFF));
}

static ssize_t update_flash_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    s32 res = 0;
    struct memx_data *memx_dev = dev_get_drvdata(dev);
    const struct firmware *firmware = NULL;
    u32 *firmware_buffer_pos = NULL;
    u32 firmware_size = 0;
    u32 i, base_addr = MXCNST_DATASRAM_BASE;
    unsigned long timeout;
    uint32_t buffer[1];
    int ret;

    if (!memx_dev) return -ENODEV;

    // Release qspi rst
    memx_send_memxcmd(memx_dev, 0, MXCNST_MEMXW_CMD, 0x20000208, 0x700E003E);

    res += scnprintf(buf + res, PAGE_SIZE - res, "================================================================================================\n");

    if (request_firmware(&firmware, FIRMWARE_BIN_NAME, &memx_dev->udev->dev) < 0) {
        res += scnprintf(buf + res, PAGE_SIZE - res, "downlaod_fw: request_firmware for cascade.bin failed\n");
        return res;
    }
    firmware_buffer_pos = (u32 *)firmware->data;
    firmware_size = firmware->size;

    for (i = 0; i < firmware_size; i += 4)
        memx_send_memxcmd(memx_dev, 0, MXCNST_MEMXW_CMD, base_addr+i, firmware_buffer_pos[i>>2]);

    memx_send_memxcmd(memx_dev, 0, MXCNST_MEMXW_CMD, MXCNST_RMTCMD_PARAM, 0);
    memx_send_memxcmd(memx_dev, 0, MXCNST_MEMXW_CMD, MXCNST_RMTCMD_COMMD, MXCNST_MEMXQ_CMD);

    timeout = jiffies + (HZ*15);
    do {
        ret = memx_read_chip0(memx_dev, buffer, MXCNST_RMTCMD_PARAM, 4);
        msleep(20);

        if (time_after(jiffies, timeout) || (ret != 0)) {
            res += scnprintf(buf + res, PAGE_SIZE - res, "Update QSPI FLASH TIMEOUT FAILED!!\n");
            release_firmware(firmware);
            return res;
        }
    } while (buffer[0] == 0);

    if (buffer[0] == 1) {
        res += scnprintf(buf + res, PAGE_SIZE - res, "Update QSPI FLASH PASS!! Verion:0x%08X DateCode:0x%08X (Please reboot to activate new fw)\n",
                         firmware_buffer_pos[0x6F0C>>2], firmware_buffer_pos[0x6F10>>2]);
    } else {
        res += scnprintf(buf + res, PAGE_SIZE - res, "Update QSPI FLASH FAILED!!\n");
    }

    res += scnprintf(buf + res, PAGE_SIZE - res, "================================================================================================\n");
    release_firmware(firmware);
    return res;
}

static ssize_t verinfo_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    s32 res = 0;
    struct memx_data *memx_dev = dev_get_drvdata(dev);
    uint32_t buffer[2];
    int ret;
    char chip_version[4] = "N/A";

    if (!memx_dev) return -ENODEV;

    ret = memx_read_chip0(memx_dev, buffer, MXCNST_COMMITID, 8);

    res += scnprintf(buf + res, PAGE_SIZE - res, "usb intf device:\n");
    res += scnprintf(buf + res, PAGE_SIZE - res, "SDK version: %s\n", SDK_RELEASE_VERSION);
    res += scnprintf(buf + res, PAGE_SIZE - res, "kdriver version: %s\n", VERSION);

    if (!ret)
        res += scnprintf(buf + res, PAGE_SIZE - res, "FW_CommitID=0x%08x DateCode=0x%08x\n", buffer[0], buffer[1]);

    ret = memx_read_chip0(memx_dev, buffer, MXCNST_MANUFACTID, 8);
    if (!ret)
        res += scnprintf(buf + res, PAGE_SIZE - res, "ManufacturerID=0x%08x%08x\n", buffer[1], buffer[0]);

    ret = memx_read_chip0(memx_dev, buffer, MXCNST_COLDRSTCNT_ADDR, 8);
    if (!ret)
        res += scnprintf(buf + res, PAGE_SIZE - res, "Cold+Warm-RebootCnt=%d  Warm-RebootCnt=%d\n",
                         ((buffer[0]&0xFFFF0000) == 0x9ABC0000) ? (buffer[0]&0xFFFF) : 0,
                         ((buffer[1]&0xFFFF0000) == 0x9ABC0000) ? (buffer[1]&0xFFFF) : 0);

    ret = memx_read_chip0(memx_dev, buffer, MXCNST_CHIP_VERSION, 8);
    if (!ret) {
        if ((buffer[0] & 0xF) == 5) snprintf(chip_version, 4, "A1");
        else snprintf(chip_version, 4, "A0");
    }

    ret = memx_read_chip0(memx_dev, buffer, MXCNST_BOOT_MODE, 8);
    if (!ret) {
        switch ((buffer[0] >> 7) & 0x3) {
        case 0: res += scnprintf(buf + res, PAGE_SIZE - res, "BootMode=QSPI  Chip=%s\n", chip_version); break;
        case 1: res += scnprintf(buf + res, PAGE_SIZE - res, "BootMode=USB  Chip=%s\n", chip_version); break;
        case 2: res += scnprintf(buf + res, PAGE_SIZE - res, "BootMode=PCIe  Chip=%s\n", chip_version); break;
        case 3: res += scnprintf(buf + res, PAGE_SIZE - res, "BootMode=UART  Chip=%s\n", chip_version); break;
        default: res += scnprintf(buf + res, PAGE_SIZE - res, "BootMode=N/A  Chip=%s\n", chip_version); break;
        }
    }
    return res;
}

static ssize_t utilization_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    s32 res = 0;
    u8 chip_id, grpid = 0;
    struct memx_data *memx_dev = dev_get_drvdata(dev);
    uint32_t buffer[16];
    int ret;

    if (!memx_dev) return -ENODEV;

    ret = memx_read_chip0(memx_dev, buffer, MXCNST_MPUUTIL_BASE, 64);
    if (!ret) {
        for (chip_id = 0; chip_id < memx_dev->chipcnt; chip_id++) {
            if (buffer[chip_id] != 0xFF) {
                res += scnprintf(buf + res, PAGE_SIZE - res, "chip%d(group%d):%u%% ", chip_id, grpid, buffer[chip_id]);
                grpid++;
            }
        }
    }
    res += scnprintf(buf + res, PAGE_SIZE - res, "\n");
    return res;
}

static ssize_t temperature_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    s32 res = 0;
    u8 chip_id;
    struct memx_data *memx_dev = dev_get_drvdata(dev);
    u32 data;
    int ret;
    uint32_t buffer[16];
    s16 temp_Celsius = 0;

    if (!memx_dev) return -ENODEV;

    ret = memx_read_chip0(memx_dev, buffer, MXCNST_TEMP_BASE, 64);
    if (!ret) {
        for (chip_id = 0; chip_id < memx_dev->chipcnt; chip_id++) {
            data = buffer[chip_id];
            temp_Celsius = (data&0xFFFF) - 273;
            res += scnprintf(buf + res, PAGE_SIZE - res, "CHIP(%d) PVT%d Temperature: %d C  (%u Kelvin) (ThermalThrottlingState: %d)\n",
                             chip_id, (data>>16)&0xF, temp_Celsius, (data&0xFFFF), (data>>20)&0xF);
        }
    }
    return res;
}

static ssize_t thermalthrottling_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct memx_data *memx_dev = dev_get_drvdata(dev);
    if (!memx_dev) return -ENODEV;
    return scnprintf(buf, PAGE_SIZE, "%s\n", memx_dev->ThermalThrottlingDisable ? "Disable":"Enable");
}

static ssize_t throughput_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    s32 res = 0;
    u32 tx_size_kb = tx_size / 1024;
    u32 rx_size_kb = rx_size / 1024;
    u32 udrv_w_quotient = udrv_throughput_info.stream_write_us ? (udrv_throughput_info.stream_write_kb * 976 / udrv_throughput_info.stream_write_us) : 0;
    u32 udrv_w_decimal = udrv_throughput_info.stream_write_us ? (udrv_throughput_info.stream_write_kb * 976 % udrv_throughput_info.stream_write_us) * 1000 / udrv_throughput_info.stream_write_us : 0;
    u32 udrv_r_quotient = udrv_throughput_info.stream_read_us ? (udrv_throughput_info.stream_read_kb * 976 / udrv_throughput_info.stream_read_us) : 0;
    u32 udrv_r_decimal = udrv_throughput_info.stream_read_us ? (udrv_throughput_info.stream_read_kb * 976 % udrv_throughput_info.stream_read_us) * 1000 / udrv_throughput_info.stream_read_us : 0;
    u32 kdrv_w_quotient = tx_time_us ? (tx_size_kb * 976 / tx_time_us) : 0;
    u32 kdrv_w_decimal = tx_time_us ? (tx_size_kb * 976 % tx_time_us) * 1000 / tx_time_us : 0;
    u32 kdrv_r_quotient = rx_time_us ? (rx_size_kb * 976 / rx_time_us) : 0;
    u32 kdrv_r_decimal = rx_time_us ? (rx_size_kb * 976 % rx_time_us) * 1000 / rx_time_us : 0;
    u32 kdrv_w_value = kdrv_w_quotient * 1000 + kdrv_w_decimal;
    u32 kdrv_r_value = kdrv_r_quotient * 1000 + kdrv_r_decimal;
    u32 udrv_w_value = udrv_w_quotient * 1000 + udrv_w_decimal;
    u32 udrv_r_value = udrv_r_quotient * 1000 + udrv_r_decimal;
    u32 write_quotient = udrv_w_value ? (kdrv_w_value * 100 / udrv_w_value) : 0;
    u32 write_decimal = udrv_w_value ? (kdrv_w_value * 100 % udrv_w_value) * 1000 / udrv_w_value : 0;
    u32 read_quotient = udrv_r_value ? (kdrv_r_value * 100 / udrv_r_value) : 0;
    u32 read_decimal = udrv_r_value ? (kdrv_r_value * 100 % udrv_r_value) * 1000 / udrv_r_value : 0;

    res += scnprintf(buf + res, PAGE_SIZE - res, "  Item  |  Period(us)  |   Data(KB)   |   TP(MB/s)   | Kdrv/Udrv\n");
    res += scnprintf(buf + res, PAGE_SIZE - res, "--------+--------------+--------------+--------------+-------------\n");
    res += scnprintf(buf + res, PAGE_SIZE - res, " Kdrv_W |  %#10x  |  %#10x  | %6u.%03u\n", tx_time_us, tx_size_kb, kdrv_w_quotient, kdrv_w_decimal);
    res += scnprintf(buf + res, PAGE_SIZE - res, " Udrv_W |  %#10x  |  %#10x  | %6u.%03u   |  %3u.%03u %%\n",
                     udrv_throughput_info.stream_write_us, udrv_throughput_info.stream_write_kb, udrv_w_quotient, udrv_w_decimal, write_quotient, write_decimal);
    res += scnprintf(buf + res, PAGE_SIZE - res, " Kdrv_R |  %#10x  |  %#10x  | %6u.%03u\n", rx_time_us, rx_size_kb, kdrv_r_quotient, kdrv_r_decimal);
    res += scnprintf(buf + res, PAGE_SIZE - res, " Udrv_R |  %#10x  |  %#10x  | %6u.%03u   |  %3u.%03u %%\n",
                     udrv_throughput_info.stream_read_us, udrv_throughput_info.stream_read_kb, udrv_r_quotient, udrv_r_decimal, read_quotient, read_decimal);

    udrv_throughput_info.stream_write_kb = 0;
    udrv_throughput_info.stream_read_kb = 0;
    udrv_throughput_info.stream_write_us = 0;
    udrv_throughput_info.stream_read_us = 0;
    tx_time_us = 0;
    tx_size = 0;
    rx_time_us = 0;
    rx_size = 0;

    return res;
}

static ssize_t frequency_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    s32 res = 0;
    u8 chip_id;
    struct memx_data *memx_dev = dev_get_drvdata(dev);
    u32 data[2];

    if (!memx_dev) return -ENODEV;

    for (chip_id = 0; chip_id < memx_dev->chipcnt; chip_id++) {
        memx_fs_get_frequency(memx_dev, &data[0], chip_id);
        res += scnprintf(buf + res, PAGE_SIZE - res, "CHIP(%d) Frequency %d MHz, %d MHz\n", chip_id, data[1], data[0]);
    }

    return res;
}

static ssize_t cmd_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    struct memx_data *memx_dev = dev_get_drvdata(dev);
    s32 ret = -EINVAL;

    if (!memx_dev || !buf || size == 0) return ret;

    ret = memx_fs_parse_cmd_and_exec(memx_dev, buf, size);
    return (ret == 0) ? size : ret;
}

static ssize_t debug_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    struct memx_data *memx_dev = dev_get_drvdata(dev);
    s32 ret = -EINVAL;

    if (!memx_dev || !buf || size == 0) return ret;

    ret = memx_fs_parse_cmd_and_exec(memx_dev, buf, size);
    return (ret == 0) ? size : ret;
}

static ssize_t i2ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    struct memx_data *memx_dev = dev_get_drvdata(dev);
    s32 ret = -EINVAL;

    if (!memx_dev || !buf || size == 0) return ret;

    ret = memx_fs_parse_i2ctrl_and_exec(memx_dev, buf, size);
    return (ret == 0) ? size : ret;
}

static ssize_t gpioctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    struct memx_data *memx_dev = dev_get_drvdata(dev);
    s32 ret = -EINVAL;

    if (!memx_dev || !buf || size == 0) return ret;

    ret = memx_fs_parse_gpioctrl_and_exec(memx_dev, buf, size);
    return (ret == 0) ? size : ret;
}

static ssize_t thermalthrottling_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    struct memx_data *memx_dev = dev_get_drvdata(dev);
    u32 chip_id;
    char *input_parser_buffer_ptr = NULL;

    if (!memx_dev || !buf || size == 0) return -EINVAL;

    input_parser_buffer_ptr = kstrdup(buf, GFP_KERNEL);
    if (!input_parser_buffer_ptr) return -ENOMEM;

    if (strncmp(input_parser_buffer_ptr, "Enable", 6) == 0) {
        pr_err("Set thermal throttling Enable\n");
        for (chip_id = 0; chip_id < memx_dev->chipcnt; chip_id++)
            memx_send_memxcmd(memx_dev, chip_id, MXCNST_MEMXt_CMD, 1, 0);

        memx_dev->ThermalThrottlingDisable = 0;
    } else if (strncmp(input_parser_buffer_ptr, "Disable", 7) == 0) {
        pr_err("Set thermal throttling Disable\n");
        for (chip_id = 0; chip_id < memx_dev->chipcnt; chip_id++)
            memx_send_memxcmd(memx_dev, chip_id, MXCNST_MEMXt_CMD, 0, 0);

        memx_dev->ThermalThrottlingDisable = 1;
    } else {
        pr_err("Not Support cmd:  %s(Only \"Enable\" and \"Disable\" are valid)\n", input_parser_buffer_ptr);
    }

    kfree(input_parser_buffer_ptr);
    return size;
}

/* ========================================================================= */
/* Sysfs Attribute Definitions & Groups                                      */
/* ========================================================================= */

static DEVICE_ATTR_RW(cmd);
static DEVICE_ATTR_RW(debug);
static DEVICE_ATTR_RW(i2ctrl);
static DEVICE_ATTR_RW(gpioctrl);
static DEVICE_ATTR_RO(update_flash);
static DEVICE_ATTR_RO(verinfo);
static DEVICE_ATTR_RO(utilization);
static DEVICE_ATTR_RO(temperature);
static DEVICE_ATTR_RW(thermalthrottling);
static DEVICE_ATTR_RO(throughput);
static DEVICE_ATTR_RO(frequency);

static struct attribute *memx_sysfs_attrs[] = {
    &dev_attr_cmd.attr,
    &dev_attr_verinfo.attr,
    &dev_attr_utilization.attr,
    &dev_attr_temperature.attr,
    &dev_attr_frequency.attr,
    &dev_attr_debug.attr,
    &dev_attr_i2ctrl.attr,
    &dev_attr_gpioctrl.attr,
    &dev_attr_update_flash.attr,
    &dev_attr_thermalthrottling.attr,
    &dev_attr_throughput.attr,
    NULL,
};

static umode_t memx_attr_is_visible(struct kobject *kobj, struct attribute *attr, int n)
{
    struct device *dev = kobj_to_dev(kobj);
    struct memx_data *memx_dev = dev_get_drvdata(dev);

    if (!memx_dev) return 0;

    if (attr == &dev_attr_debug.attr ||
        attr == &dev_attr_i2ctrl.attr ||
        attr == &dev_attr_gpioctrl.attr ||
        attr == &dev_attr_update_flash.attr ||
        attr == &dev_attr_thermalthrottling.attr ||
        attr == &dev_attr_throughput.attr) {

        if (!memx_dev->fs.debug_en)
            return 0;
    }
    return attr->mode;
}

static const struct attribute_group memx_sysfs_group = {
    .attrs = memx_sysfs_attrs,
    .is_visible = memx_attr_is_visible,
};

s32 memx_fs_device_init(struct memx_data *memx_dev)
{
    if (!memx_dev || !memx_dev->pDevice) {
        pr_err("%s: Invalid parameters (pDevice is NULL)\n", __func__);
        return -EINVAL;
    }
    return sysfs_create_group(&memx_dev->pDevice->kobj, &memx_sysfs_group);
}

void memx_fs_device_deinit(struct memx_data *memx_dev)
{
    if (!memx_dev || !memx_dev->pDevice) return;
    sysfs_remove_group(&memx_dev->pDevice->kobj, &memx_sysfs_group);
}