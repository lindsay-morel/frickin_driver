/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#ifndef MEMX_GBF_H_
#define MEMX_GBF_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "memx_common.h"
#include "memx_status.h"

/**
 * @brief Get actual channel number with dummy channel added if necessary in
 * GBF80 format. Since each GBF80 entry shares exponent among 8 channels, there
 * will be dummy channel added if given channel number is not mulitple of 8.
 *
 * @param channel_number_reshaped feature map channel number with dummy channel added
 * @param channel_number      feature map channel number
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_gbf_get_gbf80_channel_number_reshaped(int* channel_number_reshaped, int channel_number);

/**
 * @brief Get actual feature map row byte size after being aligned to bus width
 * which should be 4 bytes (32 bits) based on current design.
 *
 * @param row_size_reshaped   feature map row size with dummy bytes padded
 * @param width               feature map width
 * @param z                   feature map z
 * @param channel_number      feature map channel number
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_gbf_get_gbf80_row_size_reshaped(int* row_size_reshaped, int width, int z, int channel_number);

/**
 * @brief Get actual feature map frame byte size after being aligned to bus
 * width which should be 4 bytes (32 bits) based on current design.
 *
 * @param frame_size_reshaped feature map frame size with dummy bytes padded
 * @param height              feature map height
 * @param width               feature map width
 * @param z                   feature map z
 * @param channel_number      feature map channel number
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_gbf_get_gbf80_frame_size_reshaped(int* frame_size_reshaped, int height, int width, int z, int channel_number);

/**
 * @brief Encode from IEEE 754 single-precision 32-bit floating point value
 * to MemryX proprietary Group-Bit-Float-80.
 *
 * @param flt32_buffer        output buffer of IEEE 754 single-precision 32-bit float
 * @param gbf80_buffer        input buffer of GBF80 format data
 * @param length              number of output float point to encode
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_gbf_encode_float32_to_gbf80(float* flt32_buffer, uint8_t* gbf80_buffer, int length);

/**
 * @brief Decode from MemryX proprietary Group-Bit-Float-80 back to IEEE 754
 * single-precision 32-bit floating point value.
 *
 * @param gbf80_buffer        input buffer of GBF80 format data
 * @param flt32_buffer        output buffer of IEEE 754 single-precision 32-bit float
 * @param length              number of output float point to decode
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_gbf_decode_gbf80_to_float32(uint8_t* gbf80_buffer, float* flt32_buffer, int length);


#ifdef __cplusplus
}
#endif

#endif /* MEMX_GBF_H_ */

