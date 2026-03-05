#include "memx_pcie_xflow.h"
#include "memx_pcie.h"
#include "memx_ioctl.h"
#include <errno.h>

int32_t memx_xflow_basic_check(struct _MemxPcie *memx_dev, uint8_t chip_id)
{
    if (!memx_dev) {
        printf("xflow_basic_check: No Opened Device!\n");
        return -ENODEV;
    }
#ifdef __linux__
    if (!memx_dev->mmap_mpu_base) {
        printf("xflow_basic_check: mmap_mpu_base is NULL\n");
        return -ENODEV;
    }
    if (((memx_dev->hw_info.chip.pcie_bar_mode == MEMXBAR_3BAR_BAR0VB_BAR2CI_16MB_BAR4SRAM) ||
         (memx_dev->hw_info.chip.pcie_bar_mode == MEMXBAR_3BAR_BAR0VB_BAR2CI_64MB_BAR4SRAM) ||
         (memx_dev->hw_info.chip.pcie_bar_mode == MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM)) &&
         (!memx_dev->mmap_xflow_vbuf_base)) {
        printf("xflow_basic_check: mmap_xflow_vbuf_base is NULL\n");
        return -ENODEV;
    }
    if ((memx_dev->hw_info.chip.pcie_bar_mode == MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM) &&
        (!memx_dev->mmap_device_irq_base)) {
        printf("xflow_basic_check: mmap_device_irq_base is NULL\n");
        return -ENODEV;
    }
#endif
    if ((chip_id >= MAX_SUPPORT_CHIP_NUM) ||
        ((memx_dev->hw_info.chip.pcie_bar_mode == MEMXBAR_3BAR_BAR0VB_BAR2CI_16MB_BAR4SRAM) && (chip_id >= 4))) {
        printf("xflow_basic_check: chip_id(%u) invalid.\n", chip_id);
        return -ENODEV;
    }
    return 0;
}

#ifdef __linux__
static uint32_t memx_get_mmap_offs(struct _MemxPcie *memx_dev, bool is_vbuf_addr) {
    uint32_t mmap_offs = 0;
    if (memx_dev->hw_info.chip.pcie_bar_mode == MEMXBAR_XFLOW128MB64B_SRAM1MB) {
        mmap_offs = XFLOW_VIRTUAL_BUFFER_PREFIX;
    } else if ((memx_dev->hw_info.chip.pcie_bar_mode == MEMXBAR_3BAR_BAR0VB_BAR2CI_16MB_BAR4SRAM) ||
               (memx_dev->hw_info.chip.pcie_bar_mode == MEMXBAR_3BAR_BAR0VB_BAR2CI_64MB_BAR4SRAM) ||
               (memx_dev->hw_info.chip.pcie_bar_mode == MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM)) {
        mmap_offs = (is_vbuf_addr) ? XFLOW_VIRTUAL_BUFFER_PREFIX: XFLOW_CONFIG_REG_PREFIX;
    }
    return mmap_offs;
}

static void memx_xflow_inderect_buffer_operation(struct _MemxPcie *memx_dev, uint8_t chip_id, bool access_mpu, uint32_t address, uint32_t value, xflow_operation_type_t xflow_operation_type)
{
    uint32_t config_register_mmap_offset = memx_get_mmap_offs(memx_dev, false);
    uint32_t virtual_buffer_mmap_offset = memx_get_mmap_offs(memx_dev, true);
    volatile uint32_t *indirect_base_addr_reg_addr = (volatile uint32_t *)(memx_dev->mmap_mpu_base + GET_XFLOW_OFFSET(0, true) + XFLOW_BASE_ADDRESS_REGISTER_OFFSET - config_register_mmap_offset);
    volatile uint32_t *indirect_control_register_addr = (volatile uint32_t *)(memx_dev->mmap_mpu_base + GET_XFLOW_OFFSET(0, true) + XFLOW_CONTROL_REGISTER_OFFSET - config_register_mmap_offset);
    volatile uint32_t *indirect_virtual_buffer_target_address = NULL;

    if (xflow_operation_type == XFLOW_OPERATION_TYPE_SET_MODE) {
        indirect_virtual_buffer_target_address = (volatile uint32_t *)(memx_dev->mmap_xflow_vbuf_base + GET_XFLOW_OFFSET(0, false) - virtual_buffer_mmap_offset);

        *indirect_control_register_addr = 1;
        *indirect_base_addr_reg_addr = MXCNST_RP_XFLOW_ADDR + GET_XFLOW_OFFSET(chip_id, true) + XFLOW_CONTROL_REGISTER_OFFSET;
        *indirect_virtual_buffer_target_address = access_mpu ? 0 : 1;
    } else if (xflow_operation_type == XFLOW_OPERATION_TYPE_SET_ADDRESS) {
        indirect_virtual_buffer_target_address = (volatile uint32_t *)(memx_dev->mmap_xflow_vbuf_base + GET_XFLOW_OFFSET(0, false) - virtual_buffer_mmap_offset);

        *indirect_control_register_addr = 1;
		*indirect_base_addr_reg_addr = MXCNST_RP_XFLOW_ADDR + GET_XFLOW_OFFSET(chip_id, true) + XFLOW_BASE_ADDRESS_REGISTER_OFFSET;
		*indirect_virtual_buffer_target_address = address;
    } else if (xflow_operation_type == XFLOW_OPERATION_TYPE_WRITE_VIRTUAL_BUFFER) {
        indirect_virtual_buffer_target_address = (volatile uint32_t *)(memx_dev->mmap_xflow_vbuf_base + GET_XFLOW_OFFSET(0, false) + address - virtual_buffer_mmap_offset);

        *indirect_control_register_addr = 1;
		*indirect_base_addr_reg_addr = MXCNST_RP_XFLOW_ADDR + GET_XFLOW_OFFSET(chip_id, false);
		*indirect_virtual_buffer_target_address = value;
    }
}

static void memx_xflow_set_access_mode(struct _MemxPcie *memx_dev, uint8_t chip_id, bool access_mpu)
{
    volatile uint32_t *control_register_addr = NULL;
    if (memx_xflow_basic_check(memx_dev, chip_id)) { printf("xflow_set_base_address: basic check fail.\n"); return; }

    if ((chip_id == 0) || (memx_dev->hw_info.chip.pcie_bar_mode != MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM)) {
        uint32_t mmap_offs = memx_get_mmap_offs(memx_dev, false);
        control_register_addr = (volatile uint32_t *)(memx_dev->mmap_mpu_base + GET_XFLOW_OFFSET(chip_id, true) + XFLOW_CONTROL_REGISTER_OFFSET - mmap_offs);
        *control_register_addr = access_mpu ? 0 : 1;
    } else {
        memx_xflow_inderect_buffer_operation(memx_dev, chip_id, access_mpu, 0, 0, XFLOW_OPERATION_TYPE_SET_MODE);
    }
}

static void memx_xflow_set_base_address(struct _MemxPcie *memx_dev, uint8_t chip_id, uint32_t base_addr)
{
    volatile uint32_t *base_addr_reg_addr = NULL;
    uint32_t mmap_offs = memx_get_mmap_offs(memx_dev, false);
    if (memx_xflow_basic_check(memx_dev, chip_id)) { printf("xflow_set_base_address: basic check fail.\n"); return; }

    if ((chip_id == 0) || (memx_dev->hw_info.chip.pcie_bar_mode != MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM)) {
        base_addr_reg_addr = (volatile uint32_t *)(memx_dev->mmap_mpu_base  + GET_XFLOW_OFFSET(chip_id, true) + XFLOW_BASE_ADDRESS_REGISTER_OFFSET - mmap_offs);
        *base_addr_reg_addr = base_addr;
    } else {
        memx_xflow_inderect_buffer_operation(memx_dev, chip_id, 0, base_addr, 0, XFLOW_OPERATION_TYPE_SET_ADDRESS);
    }
}

static void memx_xflow_write_virtual_buffer_address(struct _MemxPcie *memx_dev, uint8_t chip_id, uint32_t base_addr_offset, uint32_t value)
{
    volatile uint32_t *virtual_buffer_target_address = NULL;
    volatile uint8_t *mmap_base = memx_dev->mmap_mpu_base;
    uint32_t mmap_offs = memx_get_mmap_offs(memx_dev, true);
    if (memx_xflow_basic_check(memx_dev, chip_id)) { printf("xflow_write_virtual_buffer: basic check fail.\n"); return; }

    if ((memx_dev->hw_info.chip.pcie_bar_mode == MEMXBAR_3BAR_BAR0VB_BAR2CI_16MB_BAR4SRAM) ||
        (memx_dev->hw_info.chip.pcie_bar_mode == MEMXBAR_3BAR_BAR0VB_BAR2CI_64MB_BAR4SRAM) || ((chip_id == 0) && (memx_dev->hw_info.chip.pcie_bar_mode == MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM))) {
        mmap_base = memx_dev->mmap_xflow_vbuf_base;
    }

    if ((chip_id == 0) || (memx_dev->hw_info.chip.pcie_bar_mode != MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM)) {
        virtual_buffer_target_address = (volatile uint32_t *)(mmap_base + GET_XFLOW_OFFSET(chip_id, false) + base_addr_offset - mmap_offs);
        *virtual_buffer_target_address = value;
    } else {
        memx_xflow_inderect_buffer_operation(memx_dev, chip_id, 0, base_addr_offset, value, XFLOW_OPERATION_TYPE_WRITE_VIRTUAL_BUFFER);
    }
}

static uint32_t memx_xflow_read_virtual_buffer_address(struct _MemxPcie *memx_dev, uint8_t chip_id, uint32_t base_addr_offset)
{
    uint32_t result = 0;
    volatile uint8_t *mmap_base = memx_dev->mmap_mpu_base;
    uint32_t mmap_offs = memx_get_mmap_offs(memx_dev, true);
    volatile uint32_t *virtual_buffer_target_address = NULL;
    if (memx_xflow_basic_check(memx_dev, chip_id)) { printf("xflow_read_virtual_buffer: basic check fail.\n"); return 0; }

    if ((memx_dev->hw_info.chip.pcie_bar_mode == MEMXBAR_3BAR_BAR0VB_BAR2CI_16MB_BAR4SRAM) ||
        (memx_dev->hw_info.chip.pcie_bar_mode == MEMXBAR_3BAR_BAR0VB_BAR2CI_64MB_BAR4SRAM) || ((chip_id == 0) && (memx_dev->hw_info.chip.pcie_bar_mode == MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM))) {
        mmap_base = memx_dev->mmap_xflow_vbuf_base;
    }

    if ((chip_id == 0) || (memx_dev->hw_info.chip.pcie_bar_mode != MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM)) {
        virtual_buffer_target_address = (volatile uint32_t *)(mmap_base + GET_XFLOW_OFFSET(chip_id, false) + base_addr_offset - mmap_offs);
        result = *virtual_buffer_target_address;
    } else {
        uint32_t config_register_mmap_offset = memx_get_mmap_offs(memx_dev, false);
        uint32_t virtual_buffer_mmap_offset = memx_get_mmap_offs(memx_dev, true);

        volatile uint32_t *indirect_base_addr_reg_addr = (volatile uint32_t *)(mmap_base + GET_XFLOW_OFFSET(0, true) + XFLOW_BASE_ADDRESS_REGISTER_OFFSET - config_register_mmap_offset);
        volatile uint32_t *indirect_control_register_addr = (volatile uint32_t *)(mmap_base + GET_XFLOW_OFFSET(0, true) + XFLOW_CONTROL_REGISTER_OFFSET - config_register_mmap_offset);
        volatile uint32_t *indirect_virtual_buffer_target_address = (volatile uint32_t *)(memx_dev->mmap_xflow_vbuf_base + GET_XFLOW_OFFSET(0, false) + base_addr_offset - virtual_buffer_mmap_offset);

        *indirect_control_register_addr = 1;
		*indirect_base_addr_reg_addr = MXCNST_RP_XFLOW_ADDR + GET_XFLOW_OFFSET(chip_id, false);
		result = *indirect_virtual_buffer_target_address;
    }

    return result;
}

static void memx_sram_write(struct _MemxPcie *memx_dev, uint32_t base_addr, uint32_t value)
{
    if (memx_dev->hw_info.chip.pcie_bar_mode != MEMXBAR_SRAM1MB) { printf("%s: wrong bar_mode\n", __FUNCTION__); return;}
	if ((base_addr < (MEMX_CHIP_SRAM_BASE + MEMX_CHIP_SRAM_DATA_SRAM_OFFS)) || (base_addr >= (MEMX_CHIP_SRAM_BASE + MEMX_CHIP_SRAM_MAX_SIZE))) {
		printf("%s: Invalid base_addr!\n", __FUNCTION__);
		return;
	}
	base_addr = base_addr - MEMX_CHIP_SRAM_BASE;

	*((volatile uint32_t *)(memx_dev->mmap_mpu_base +base_addr)) = value;
}

static uint32_t memx_sram_read(struct _MemxPcie *memx_dev, uint32_t base_addr)
{
    if (memx_dev->hw_info.chip.pcie_bar_mode != MEMXBAR_SRAM1MB) { printf("%s: wrong bar_mode\n", __FUNCTION__); return 0;}
	if ((base_addr < (MEMX_CHIP_SRAM_BASE + MEMX_CHIP_SRAM_DATA_SRAM_OFFS)) || (base_addr >= (MEMX_CHIP_SRAM_BASE + MEMX_CHIP_SRAM_MAX_SIZE))) {
		printf("%s: Invalid base_addr!\n", __FUNCTION__);
		return 0;
	}
	base_addr = base_addr - MEMX_CHIP_SRAM_BASE;

	return *((volatile uint32_t *)(memx_dev->mmap_mpu_base +base_addr)) ;
}

static int32_t memx_wait_sram_cmd_complete(struct _MemxPcie *memx_dev, uint32_t cmd_base, uint32_t timeout)
{
    volatile uint32_t timeout_cnt = 0;
    if (memx_dev->hw_info.chip.pcie_bar_mode != MEMXBAR_SRAM1MB) { printf("%s: wrong bar_mode\n", __FUNCTION__); return -1;}
    while(memx_sram_read(memx_dev, cmd_base) != MEMX_EXTCMD_COMPLETE) {
        if(timeout_cnt++ > timeout) {
            printf("ERROR: memx_wait_sram_cmd_complete timeout\n");
            return -1;
        }
    }
    return 0;
}

static int32_t memx_en_buf0_irq(struct _MemxPcie *memx_dev, uint8_t chip_id, uint32_t value)
{
    uint32_t cmd_base = MEMX_EXTINFO_EN_BUF0_IRQ_BASE + chip_id * 4;
    if (memx_dev->hw_info.chip.pcie_bar_mode != MEMXBAR_SRAM1MB) { printf("%s: wrong bar_mode\n", __FUNCTION__); return -1;}

    if (memx_wait_sram_cmd_complete(memx_dev, cmd_base, MEMX_EXTINFO_CMD_TIMEOUT)) {
        printf("%s: wait sram cmd timeout\n", __FUNCTION__);
        return -1;
    }
    memx_sram_write(memx_dev, cmd_base, value);

    if (memx_wait_sram_cmd_complete(memx_dev, cmd_base, MEMX_EXTINFO_CMD_TIMEOUT)) {
        printf("%s: wait sram cmd timeout\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

int32_t memx_xflow_burst_write_wtmem(struct _MemxPcie *memx_dev, uint8_t chip_id, uint32_t base_addr, uint8_t *data, uint32_t write_size, bool access_mpu)
{
    int32_t result = memx_xflow_basic_check(memx_dev, chip_id);
    uint32_t mmap_offs = memx_get_mmap_offs(memx_dev, true);
    if (result != 0) {
        printf("xflow_write: basic check fail. result(%d)\n", result);
        return result;
    }

    if (write_size > XFLOW_MAX_BURST_WRITE_SIZE || write_size % XFLOW_BURST_WRITE_BATCH_SIZE) {
        printf("memx_xflow_burst_write: invalid write size: %d \n", write_size);
        return -1;
    }
    if (memx_dev->hw_info.chip.pcie_bar_mode == MEMXBAR_SRAM1MB) {
        platform_mutex_lock(&memx_dev->xflow_write_guard[0]);
        for (uint32_t i=0 ; i<write_size; i+=4) {
            if (memx_wait_sram_cmd_complete(memx_dev, MEMX_EXTINFO_CMD_BASE, MEMX_EXTINFO_CMD_TIMEOUT)) {
                printf("memx_xflow_burst_write: wait sram cmd timeout\n");
                result = -1;
                break;
            }

            memx_sram_write(memx_dev, MEMX_EXTINFO_DATA_BASE, (base_addr));
            memx_sram_write(memx_dev, MEMX_EXTINFO_DATA_BASE + 4, (uint32_t)*(volatile uint32_t *)(data+i));
            memx_sram_write(memx_dev, MEMX_EXTINFO_CMD_BASE, MEMX_EXTINFO_CMD(chip_id, MEMX_EXTCMD_XFLOW_WRITE_REG, (access_mpu ? 0 : 1)));

            if (memx_wait_sram_cmd_complete(memx_dev, MEMX_EXTINFO_CMD_BASE, MEMX_EXTINFO_CMD_TIMEOUT)) {
                printf("memx_xflow_burst_write: wait sram cmd timeout\n");
                result = -1;
                break;
            }
        }
        platform_mutex_unlock(&memx_dev->xflow_write_guard[0]);
    } else {
        platform_mutex_lock(&memx_dev->xflow_write_guard[chip_id]);
        memx_xflow_set_access_mode(memx_dev, chip_id, access_mpu);
        memx_xflow_set_base_address(memx_dev, chip_id, base_addr);

        // hack: wtmem are always 64 bytes-alignment
        for (uint32_t i=0 ; i<write_size; i+=XFLOW_BURST_WRITE_BATCH_SIZE) {
            void *virtual_buffer_target_address = (void *)(memx_dev->mmap_mpu_base + GET_XFLOW_OFFSET(chip_id, false) + i - mmap_offs);
            {
                platform_uint32_t j;
                volatile uint64_t *src8,*dst8;

                src8 = (volatile uint64_t *)(data+i);
                dst8 = (volatile uint64_t *)virtual_buffer_target_address;
                for(j=0; j<(XFLOW_BURST_WRITE_BATCH_SIZE>>3); j+=1)
                    dst8[j] = src8[j];
            }
        }
        if(!access_mpu) memx_xflow_set_access_mode(memx_dev, chip_id, true);
        platform_mutex_unlock(&memx_dev->xflow_write_guard[chip_id]);
    }
    return result;
}

#endif

int32_t memx_xflow_write(struct _MemxPcie *memx_dev, uint8_t chip_id, uint32_t base_addr, uint32_t base_addr_offset, uint32_t value, bool access_mpu)
{
    int32_t result = memx_xflow_basic_check(memx_dev, chip_id);
    if (result != 0) {
        printf("xflow_write: basic check fail. result(%d)\n", result);
        return result;
    }
#ifdef __linux__
    if (memx_dev->hw_info.chip.pcie_bar_mode == MEMXBAR_SRAM1MB) {
        platform_mutex_lock(&memx_dev->xflow_write_guard[0]);
        // EN BUF0 IRQ requeset
        if ((base_addr + base_addr_offset) == 0x30200010) {
            result = memx_en_buf0_irq(memx_dev, chip_id, value);
        } else {
            if (memx_wait_sram_cmd_complete(memx_dev, MEMX_EXTINFO_CMD_BASE, MEMX_EXTINFO_CMD_TIMEOUT)) {
                printf("memx_xflow_burst_write: wait sram cmd timeout\n");
                result = -1;
            }

            memx_sram_write(memx_dev, MEMX_EXTINFO_DATA_BASE, (base_addr + base_addr_offset));
            memx_sram_write(memx_dev, MEMX_EXTINFO_DATA_BASE + 4, value);
            memx_sram_write(memx_dev, MEMX_EXTINFO_CMD_BASE, MEMX_EXTINFO_CMD(chip_id, MEMX_EXTCMD_XFLOW_WRITE_REG, (access_mpu ? 0 : 1)));

            if (memx_wait_sram_cmd_complete(memx_dev, MEMX_EXTINFO_CMD_BASE, MEMX_EXTINFO_CMD_TIMEOUT)) {
                printf("memx_xflow_burst_write: wait sram cmd timeout\n");
                result = -1;
            }
        }
        platform_mutex_unlock(&memx_dev->xflow_write_guard[0]);
    } else {
        if (memx_dev->hw_info.chip.pcie_bar_mode != MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM) {
            platform_mutex_lock(&memx_dev->xflow_write_guard[chip_id]);
        } else {
            platform_mutex_lock(&memx_dev->xflow_access_guard_first_chip_control);
        }

        memx_xflow_set_access_mode(memx_dev, chip_id, access_mpu);
        memx_xflow_set_base_address(memx_dev, chip_id, base_addr);
        memx_xflow_write_virtual_buffer_address(memx_dev, chip_id, base_addr_offset, value);
        if(!access_mpu) memx_xflow_set_access_mode(memx_dev, chip_id, true);

        if (memx_dev->hw_info.chip.pcie_bar_mode != MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM) {
            platform_mutex_unlock(&memx_dev->xflow_write_guard[chip_id]);
        } else {
            if (chip_id > 0) memx_xflow_set_access_mode(memx_dev, 0, true);
            platform_mutex_unlock(&memx_dev->xflow_access_guard_first_chip_control);
        }
    }
#elif _WIN32
    platform_mutex_lock(&memx_dev->xflow_write_guard[chip_id]);
    struct memx_xflow_param parameter = {0};
    parameter.chip_id = chip_id;
    parameter.access_mpu = access_mpu ? 0 : 1;
    parameter.is_read = false;
    parameter.base_addr = base_addr;
    parameter.base_offset = base_addr_offset;
    parameter.value = value;
    result = platform_ioctl(memx_dev->ctrl_event, &memx_dev->fd, MEMX_XFLOW_ACCESS, &parameter, sizeof(struct memx_xflow_param));
    platform_mutex_unlock(&memx_dev->xflow_write_guard[chip_id]);
#endif
    if (result != 0) { printf("xflow_write: ioctl fail. result(%d)\n", result); }
    return result;
}

int32_t memx_xflow_read(struct _MemxPcie *memx_dev, uint8_t chip_id, uint32_t base_addr, uint32_t base_addr_offset, bool access_mpu)
{
    int32_t result = memx_xflow_basic_check(memx_dev, chip_id);
    if (result != 0) {
        printf("xflow_write: basic check fail. result(%d)\n", result);
        return result;
    }
#ifdef __linux__
    if (memx_dev->hw_info.chip.pcie_bar_mode == MEMXBAR_SRAM1MB) {
        memx_sram_write(memx_dev, MEMX_EXTINFO_DATA_BASE, (base_addr + base_addr_offset));
        memx_sram_write(memx_dev, MEMX_EXTINFO_CMD_BASE, MEMX_EXTINFO_CMD(chip_id, MEMX_EXTCMD_XFLOW_READ_REG, (access_mpu ? 0 : 1)));

        if (memx_wait_sram_cmd_complete(memx_dev, MEMX_EXTINFO_CMD_BASE, MEMX_EXTINFO_CMD_TIMEOUT)) {
            printf("memx_xflow_burst_write: wait sram cmd timeout\n");
            result = -1;
        } else {
            result = memx_sram_read(memx_dev, MEMX_EXTINFO_DATA_BASE + 4);
        }
    } else {
        if (memx_dev->hw_info.chip.pcie_bar_mode != MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM) {
            platform_mutex_lock(&memx_dev->xflow_write_guard[chip_id]);
        } else {
            platform_mutex_lock(&memx_dev->xflow_access_guard_first_chip_control);
        }
        memx_xflow_set_access_mode(memx_dev, chip_id, access_mpu);
        memx_xflow_set_base_address(memx_dev, chip_id, base_addr);
        result = memx_xflow_read_virtual_buffer_address(memx_dev, chip_id, base_addr_offset);
        if(!access_mpu) memx_xflow_set_access_mode(memx_dev, chip_id, true);

        if (memx_dev->hw_info.chip.pcie_bar_mode != MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM) {

            platform_mutex_unlock(&memx_dev->xflow_write_guard[chip_id]);
        } else {
            if (chip_id > 0) memx_xflow_set_access_mode(memx_dev, 0, true);
            platform_mutex_unlock(&memx_dev->xflow_access_guard_first_chip_control);
        }
    }
#elif _WIN32
    struct memx_xflow_param parameter = {0};
    parameter.chip_id = chip_id;
    parameter.access_mpu = (access_mpu) ? 0 : 1;
    parameter.is_read = true;
    parameter.base_addr = base_addr;
    parameter.base_offset = base_addr_offset;
    result = platform_ioctl(memx_dev->ctrl_event, &memx_dev->fd, MEMX_XFLOW_ACCESS, &parameter, sizeof(struct memx_xflow_param));
    if (result < 0) {
        printf("xflow_read: ioctl fail. result(%d)\n", result);
        return result;
    }
    result = parameter.value;
#endif
    return result;
}