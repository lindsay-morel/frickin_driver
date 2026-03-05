/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#include "memx_common.h"
#include "memx_mpuio_comm.h"

char *mpuio_comm_find_file_name(const char *name)
{
    if (!name) { return NULL; }
#ifdef __linux__
    int32_t sep = '/';
#elif _WIN32
    int32_t sep = '\\';
#endif
    char *name_start = strrchr((char *)name, sep);
return (name_start) ? (name_start + 1) : (char *)name;
}

uint32_t mpuio_comm_cal_output_flow_size(int32_t h, int32_t w, int32_t z, int32_t ch, int32_t format, uint8_t frm_pad)
{
    uint32_t ch_trans = 0;
    uint32_t remainder = 0;
    uint32_t total_length = 0;

    // FIXME: For FP32/FP16 format update flow size with diff ch_trans(4/2), we should move this to mpu layer?
    switch (format) {
        case MEMX_MPUIO_OFMAP_FORMAT_GBF80 :
        case MEMX_MPUIO_OFMAP_FORMAT_GBF80_ROW_PAD :  {
            if (ch % 8) { remainder = 1; }
            ch_trans = ((ch / 8) + 1 * remainder) * 10;
        } break;
        case MEMX_MPUIO_OFMAP_FORMAT_FLOAT16 : {
            ch_trans = ch * 2;
        } break;
        case MEMX_MPUIO_OFMAP_FORMAT_FLOAT32 : {
            ch_trans = ch * 4;
        } break;
        default: {
            printf("Unsupport format %d !\n", format);
        }
    }

    total_length = w * z * ch_trans;

    if (!frm_pad) {
        // row pad
        if (total_length & 0x3) {
            total_length &= ~0x3;
            total_length += 4;
        }
    }

    total_length *= h;

    if (frm_pad) {
        if (total_length & 0x3) {
            total_length &= ~0x3;
            total_length += 4;
        }
    }
    return total_length;
}

static uint32_t find_max_prime_factor(uint32_t N)
{
    uint32_t max_prime_factor = 1;
    for (uint32_t i = 2; i * i <= N; i++) {
        if (N % i == 0) {
            while (N % i == 0) { N /= i; }
            if (i > max_prime_factor) { max_prime_factor = i; }
        }
    }
    if (N != 1 && N > max_prime_factor) { max_prime_factor = N; }
    return max_prime_factor;
}

uint32_t mpuio_comm_assign_suitable_buffer_size_for_flow(uint32_t h, uint32_t flow_size, uint32_t remaining_try_count)
{
    uint32_t row_size = flow_size / h;
    uint32_t buffer_size = 0;
    uint32_t max_buf_size = (remaining_try_count * 1024 - MEMX_MPUIO_OFMAP_HEADER_SIZE);

    // frame pad
    if (h * row_size != flow_size || row_size % 4)
        row_size = flow_size;

    if (row_size > max_buf_size) {
        uint32_t max_prime_factor = find_max_prime_factor(row_size);
        uint32_t divider = row_size / max_prime_factor / 4;
        // using maximum number for better performance
        buffer_size = (max_prime_factor > divider) ? max_prime_factor * 4 : divider * 4;

        if (buffer_size > max_buf_size) {
            buffer_size = (max_prime_factor < divider) ? max_prime_factor * 4 : divider * 4;
        }
    } else {
        buffer_size = row_size;
    }

    // invalid case
    if (buffer_size == 0 || buffer_size > max_buf_size) {
        buffer_size = 4;
    } else {
        uint32_t q = max_buf_size / buffer_size + 1;

        if (q != 1) {
            do {
                q--;
                if (0 == (flow_size % (buffer_size * q))) {
                    buffer_size = buffer_size * q;
                    break;
                }
            } while (buffer_size * q <= max_buf_size && q > 1);
        }
    }

    return buffer_size;
}

memx_status mpuio_comm_config_chip_role(uint8_t group_cnt, uint8_t chip_cnt_per_group, struct hw_info *p_hw_info)
{
    memx_status status = MEMX_STATUS_OK;
    if (group_cnt < 1 || chip_cnt_per_group < 1) {
        status = MEMX_STATUS_INTERNAL_ERROR;
    } else if (group_cnt * chip_cnt_per_group > p_hw_info->chip.total_chip_cnt) {
        status = MEMX_STATUS_MPUIO_INSUFFICENT_CHIP;
    } else {
        for (uint8_t chip_id = 0; chip_id < MEMX_MPUIO_MAX_HW_MPU_COUNT; chip_id++) {
            p_hw_info->chip.roles[chip_id] = ROLE_UNCONFIGURED;
        }

        uint8_t chip_id = 0;
        while (group_cnt--) {
            if (chip_cnt_per_group > 1) {
                p_hw_info->chip.roles[chip_id++] = ROLE_MULTI_FIRST;
                for (int i = 0; i < chip_cnt_per_group - 2; i++) {
                    p_hw_info->chip.roles[chip_id++] = ROLE_MULTI_MIDDLE;
                }
                p_hw_info->chip.roles[chip_id++] = ROLE_MULTI_LAST;
            } else {
                p_hw_info->chip.roles[chip_id++] = ROLE_SINGLE;
            }
        }
    }

    return status;
}

memx_status mpuio_comm_config_mpu_group(uint8_t mpu_group_config, struct hw_info *p_hw_info)
{
    memx_status status = MEMX_STATUS_OK;
    switch (mpu_group_config)
    {
    case MEMX_MPUIO_MPU_GROUP_CONFIG_ONE_GROUP_FOUR_MPUS:
        status = mpuio_comm_config_chip_role(1, 4, p_hw_info);
        break;
    case MEMX_MPUIO_MPU_GROUP_CONFIG_TWO_GROUP_TWO_MPUS:
        status = mpuio_comm_config_chip_role(2, 2, p_hw_info);
        break;
    case MEMX_MPUIO_MPU_GROUP_CONFIG_ONE_GROUP_ONE_MPU:
        status = mpuio_comm_config_chip_role(1, 1, p_hw_info);
        break;
    case MEMX_MPUIO_MPU_GROUP_CONFIG_ONE_GROUP_THREE_MPUS:
        status = mpuio_comm_config_chip_role(1, 3, p_hw_info);
        break;
    case MEMX_MPUIO_MPU_GROUP_CONFIG_ONE_GROUP_TWO_MPUS:
        status = mpuio_comm_config_chip_role(1, 2, p_hw_info);
        break;
    case MEMX_MPUIO_MPU_GROUP_CONFIG_ONE_GROUP_EIGHT_MPUS:
        status = mpuio_comm_config_chip_role(1, 8, p_hw_info);
        break;
    case MEMX_MPUIO_MPU_GROUP_CONFIG_ONE_GROUP_TWELVE_MPUS:
        status = mpuio_comm_config_chip_role(1, 12, p_hw_info);
        break;
    case MEMX_MPUIO_MPU_GROUP_CONFIG_ONE_GROUP_SIXTEEN_MPUS:
        status = mpuio_comm_config_chip_role(1, 16, p_hw_info);
        break;
    default:
        status = MEMX_STATUS_INVALID_PARAMETER;
        break;
    }
    return status;
}

#define CLEAR_BIT(var, pos) ((var) & ~(1 << (pos)))
#define SET_BIT(var, pos)   ((var) | (1 << (pos)))
int32_t mpuio_comm_buffer_allocation_for_flow(uint32_t *buffer_size, uint32_t *in_flow_size, uint32_t flow_count, uint32_t *available_ram_size)
{
    uint32_t total_flow_size = 0;
    uint32_t quotient = 0;
    uint32_t remainder = 0;
    uint32_t checking = 0;

    // Calculate the total size of all frames
    for (uint32_t i = 0; i < flow_count; i++) {
        total_flow_size += in_flow_size[i];
        checking = SET_BIT(checking, i);
    }

    // Calculate the quotient of total flow size divided by SRAM size
    quotient = total_flow_size / *available_ram_size;
    remainder = total_flow_size % *available_ram_size;
    if (remainder) {
        quotient++;
    }

    // Find the largest quotient for each frame
    do {
        for (uint32_t i = 0; i < flow_count; i++) {
            remainder = in_flow_size[i] % quotient;
            if (remainder) {
                quotient++;
                break;
            }
            if (quotient == in_flow_size[i]) {
                // Error checking: there is no suitable quotient found.
                return 5;
            }
        }
    } while (remainder);

    // Calculate the initial buffer size with multiply of 4 and remaining SRAM space
    for (uint32_t i = 0; i < flow_count; i++) {
        buffer_size[i] = in_flow_size[i] / quotient;

        // If the buffer_size is not a multiple of 4
        if (buffer_size[i] & 3) {

            // If the buffer_size is not a multiple of 2
            if (buffer_size[i] & 1) {

                // If there is enough remaining RAM space, multiply by 4
                if ((*available_ram_size > (buffer_size[i] * 4)) && (in_flow_size[i] >= (buffer_size[i] * 4)) && ((in_flow_size[i] % (buffer_size[i] * 4)) == 0)) {
                    buffer_size[i] = buffer_size[i] * 4;
                } else {
                    // The remaining RAM space is not enough to multiply the buffer by 4.
                    return 4;
                }

            } else {

                // If there is enough remaining RAM space, multiply by 2
                if ((*available_ram_size > (buffer_size[i] * 2)) && (in_flow_size[i] >= (buffer_size[i] * 2)) && ((in_flow_size[i] % (buffer_size[i] * 2)) == 0)) {
                    buffer_size[i] += buffer_size[i];
                } else {
                    // The remaining RAM space is not enough to multiply the buffer by 2.
                    return 2;
                }
            }
        }

        if (*available_ram_size < buffer_size[i]) {
            // Error checking: The remaining RAM space is not enough.
            return 6;
        }
        *available_ram_size -= buffer_size[i];
    }

    // Fill the buffer as many as possible
    while (checking) {
        for (uint32_t i = 0; i < flow_count; i++) {

            // If the current buffer size is already equal to the flow size
            if (buffer_size[i] == in_flow_size[i]) {
                checking = CLEAR_BIT(checking, i);
            }

            // If there is no more available SRAM
            if (buffer_size[i] > *available_ram_size) {
                checking = CLEAR_BIT(checking, i);
            } else {

                // If the buffer size is smaller than the flow size
                if (in_flow_size[i] > buffer_size[i]) {

                    // If doubling the buffer size results in no remainder
                    if (((in_flow_size[i] % (buffer_size[i] * 2)) == 0)) {
                        *available_ram_size -= buffer_size[i];
                        buffer_size[i] += buffer_size[i];

                    } else {
                        // Nothing can do for this flow
                        checking = CLEAR_BIT(checking, i);
                    }
                }
            }

            if (buffer_size[i] > in_flow_size[i]) {
                // Error checking: The buffer size is over flow size.
                return 3;
            }
        }
    }
    return 0;
}