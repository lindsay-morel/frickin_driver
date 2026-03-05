/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#include "memx_gbf.h"

typedef struct _MemxGbfGbf80Map {
  unsigned int man_0  : 8;
  unsigned int sign_0 : 1;
  unsigned int man_1  : 8;
  unsigned int sign_1 : 1;
  unsigned int man_2  : 8;
  unsigned int sign_2 : 1;
  unsigned int man_3_0: 5; // because of C compiler will align to 4 bytes
  unsigned int man_3_1: 3; // if we declare 9 bits here, it will be placed to bit[32:32+9]
  unsigned int sign_3 : 1;
  unsigned int man_4  : 8;
  unsigned int sign_4 : 1;
  unsigned int man_5  : 8;
  unsigned int sign_5 : 1;
  unsigned int man_6  : 8;
  unsigned int sign_6 : 1;
  unsigned int man_7_0: 1;
  unsigned int man_7_1: 7;
  unsigned int sign_7 : 1;
  unsigned int exp    : 8;
  // to be noticed, this structure will actually be aligned to 12 bytes
  // but do not append dummy padding manually or will cause memory violation
} MemxGbfGbf80Map;

typedef struct _MemxGbfFloat32Map {
  unsigned int zero   : 16; // always zeros field
  unsigned int man    : 7;
  unsigned int exp    : 8;
  unsigned int sign   : 1;
} MemxGbfFloat32Map;


/***************************************************************************//**
 * implementation
 ******************************************************************************/

memx_status memx_gbf_get_gbf80_channel_number_reshaped(int* channel_number_reshaped, int channel_number)
{
  memx_status status = MEMX_STATUS_OK;

  if(memx_status_no_error(status)&&(channel_number_reshaped == NULL))
    status = MEMX_STATUS_GBF_INVALID_DATA;
  if(memx_status_no_error(status)&&(channel_number < 0))
    status = MEMX_STATUS_GBF_INVALID_CHANNEL_NUMBER;

  if(memx_status_no_error(status)) {
    *channel_number_reshaped = (channel_number + 7) & ~0x7; // ceiling to 8
  }

  return status;
}

memx_status memx_gbf_get_gbf80_row_size_reshaped(int* row_size_reshaped, int width, int z, int channel_number)
{
  memx_status status = MEMX_STATUS_OK;
  int channel_number_reshaped = 0;
  int gbf80_entry_number = 0;

  if(memx_status_no_error(status)&&(row_size_reshaped == NULL))
    status = MEMX_STATUS_GBF_INVALID_DATA;
  if(memx_status_no_error(status)&&(width < 0))
    status = MEMX_STATUS_GBF_INVALID_WIDTH;
  if(memx_status_no_error(status)&&(z < 0))
    status = MEMX_STATUS_GBF_INVALID_Z;

  if(memx_status_no_error(status))
    status = memx_gbf_get_gbf80_channel_number_reshaped(&channel_number_reshaped, channel_number);
  if(memx_status_no_error(status)) {
    gbf80_entry_number = channel_number_reshaped >> 3; // divided by 8
    *row_size_reshaped = (width * z * gbf80_entry_number * 10 + 3) & ~0x3; // each gbf80 contains overall 10 bytes and row size should be aligned to 4 bytes
  }

  return status;
}

memx_status memx_gbf_get_gbf80_frame_size_reshaped(int* frame_size_reshaped, int height, int width, int z, int channel_number)
{
  memx_status status = MEMX_STATUS_OK;
  int row_size_reshaped = 0;

  if(memx_status_no_error(status)&&(frame_size_reshaped == NULL))
    status = MEMX_STATUS_GBF_INVALID_DATA;
  if(memx_status_no_error(status)&&(height < 0))
    status = MEMX_STATUS_GBF_INVALID_HEIGHT;
  if(memx_status_no_error(status)&&(z < 0))
    status = MEMX_STATUS_GBF_INVALID_Z;

  if(memx_status_no_error(status))
    status = memx_gbf_get_gbf80_row_size_reshaped(&row_size_reshaped, width, z, channel_number);
  if(memx_status_no_error(status)) {
    *frame_size_reshaped = (height * row_size_reshaped + 3) & ~0x3; // align overall frame size to 4 bytes again
  }

  return status;
}

memx_status memx_gbf_encode_float32_to_gbf80(float* flt32_buffer, uint8_t* gbf80_buffer, int length)
{
  memx_status status = MEMX_STATUS_OK;
  MemxGbfGbf80Map* gbf80_map;
  MemxGbfFloat32Map* flt32_map;
  uint8_t* gbf80;
  float* flt32;
  int gbf80_offset = 0;
  int flt32_offset = 0;

  unsigned char exp;
  unsigned char man;
  #define max(x, y) (((x) > (y)) ? (x) : (y))
  #define _SET_MANTISSA_SHIFT_WITH_ROUNDING(_exp_shift_) \
    do { \
      if((_exp_shift_) == 0) { \
        man = (flt32_map->man == 0x7f) ?(unsigned char)(0x80|flt32_map->man) \
          : (unsigned char)(0x80|flt32_map->man) + ((flt32_map->zero >> 15) & 0x1); \
      } else { \
        man = (unsigned char) ((0x80|flt32_map->man) >> (_exp_shift_)) \
          + ((flt32_map->man >> ((_exp_shift_)-1)) & 0x1); \
      } \
    } while(0)

  if(memx_status_no_error(status)&&(flt32_buffer == NULL))
    status = MEMX_STATUS_GBF_INVALID_DATA;
  if(memx_status_no_error(status)&&(gbf80_buffer == NULL))
    status = MEMX_STATUS_GBF_INVALID_DATA;
  if(memx_status_no_error(status)&&(length < 0))
    status = MEMX_STATUS_GBF_INVALID_LENGTH;

  while(memx_status_no_error(status)&&(flt32_offset < length)) {
    gbf80 = gbf80_buffer + gbf80_offset;
    flt32 = flt32_buffer + flt32_offset;

    // performs float32 to float16 rounding, based on IEEE floating point design
    // no need to handle exponent and mantissa separately
    for(int i=0; i<8; ++i) {
      if(flt32_offset+i < length) {
        *(uint32_t*)(flt32+i) += 0x00008000;
        *(uint32_t*)(flt32+i) &= 0xffff0000;
      }
    }

    // gets maximum exponent among 8 floating points
    exp = 0;
    for(int i=0; i<8; ++i) {
      if(flt32_offset+i < length) {
        flt32_map = (MemxGbfFloat32Map*)(flt32+i);
        exp = max(exp, (unsigned char)flt32_map->exp);
      }
    }

    // combines 8 floating points to gbf80
    gbf80_map = (MemxGbfGbf80Map*)gbf80;
    gbf80_map->exp = exp;
    if(flt32_offset+0 < length) {
      flt32_map = (MemxGbfFloat32Map*)(flt32+0);
      _SET_MANTISSA_SHIFT_WITH_ROUNDING(exp-flt32_map->exp);
      gbf80_map->man_0 = man & 0xff;
      gbf80_map->sign_0 = (man & 0xff) ? flt32_map->sign : 0;
    } else {
      gbf80_map->man_0 = 0;
      gbf80_map->sign_0 = 0;
    }
    if(flt32_offset+1 < length) {
      flt32_map = (MemxGbfFloat32Map*)(flt32+1);
      _SET_MANTISSA_SHIFT_WITH_ROUNDING(exp-flt32_map->exp);
      gbf80_map->man_1 = man & 0xff;
      gbf80_map->sign_1 = (man & 0xff) ? flt32_map->sign : 0;
    } else {
      gbf80_map->man_1 = 0;
      gbf80_map->sign_1 = 0;
    }
    if(flt32_offset+2 < length) {
      flt32_map = (MemxGbfFloat32Map*)(flt32+2);
      _SET_MANTISSA_SHIFT_WITH_ROUNDING(exp-flt32_map->exp);
      gbf80_map->man_2 = man & 0xff;
      gbf80_map->sign_2 = (man & 0xff) ? flt32_map->sign : 0;
    } else {
      gbf80_map->man_2 = 0;
      gbf80_map->sign_2 = 0;
    }
    if(flt32_offset+3 < length) {
      flt32_map = (MemxGbfFloat32Map*)(flt32+3);
      _SET_MANTISSA_SHIFT_WITH_ROUNDING(exp-flt32_map->exp);
      gbf80_map->man_3_0 = man & 0x1f;
      gbf80_map->man_3_1 = (man >> 5) & 0x7;
      gbf80_map->sign_3 = (man & 0xff) ? flt32_map->sign : 0;
    } else {
      gbf80_map->man_3_0 = 0;
      gbf80_map->man_3_1 = 0;
      gbf80_map->sign_3 = 0;
    }
    if(flt32_offset+4 < length) {
      flt32_map = (MemxGbfFloat32Map*)(flt32+4);
      _SET_MANTISSA_SHIFT_WITH_ROUNDING(exp-flt32_map->exp);
      gbf80_map->man_4 = man & 0xff;
      gbf80_map->sign_4 = (man & 0xff) ? flt32_map->sign : 0;
    } else {
      gbf80_map->man_4 = 0;
      gbf80_map->sign_4 = 0;
    }
    if(flt32_offset+5 < length) {
      flt32_map = (MemxGbfFloat32Map*)(flt32+5);
      _SET_MANTISSA_SHIFT_WITH_ROUNDING(exp-flt32_map->exp);
      gbf80_map->man_5 = man & 0xff;
      gbf80_map->sign_5 = (man & 0xff) ? flt32_map->sign : 0;
    } else {
      gbf80_map->man_5 = 0;
      gbf80_map->sign_5 = 0;
    }
    if(flt32_offset+6 < length) {
      flt32_map = (MemxGbfFloat32Map*)(flt32+6);
      _SET_MANTISSA_SHIFT_WITH_ROUNDING(exp-flt32_map->exp);
      gbf80_map->man_6 = man & 0xff;
      gbf80_map->sign_6 = (man & 0xff) ? flt32_map->sign : 0;
    } else {
      gbf80_map->man_6 = 0;
      gbf80_map->sign_6 = 0;
    }
    if(flt32_offset+7 < length) {
      flt32_map = (MemxGbfFloat32Map*)(flt32+7);
      _SET_MANTISSA_SHIFT_WITH_ROUNDING(exp-flt32_map->exp);
      gbf80_map->man_7_0 = man & 0x1;
      gbf80_map->man_7_1 = (man >> 1) & 0x7f;
      gbf80_map->sign_7 = (man & 0xff) ? flt32_map->sign : 0;
    } else {
      gbf80_map->man_7_0 = 0;
      gbf80_map->man_7_1 = 0;
      gbf80_map->sign_7 = 0;
    }

    gbf80_offset += 10;
    flt32_offset += 8;
  }

  return status;
}

memx_status memx_gbf_decode_gbf80_to_float32(uint8_t* gbf80_buffer, float* flt32_buffer, int length)
{
  memx_status status = MEMX_STATUS_OK;
  MemxGbfGbf80Map* gbf80_map;
  MemxGbfFloat32Map* flt32_map;
  uint8_t* gbf80;
  float* flt32;
  int gbf80_offset = 0;
  int flt32_offset = 0;

  unsigned char exp;
  unsigned char man;
  #define _SET_FLOAT32_WITH_LEADING_ONE_ADJUST(_man_, _exp_, _sign_) \
    do { \
      man = (unsigned char) (_man_); \
      exp = (unsigned char) (_exp_); \
      while(man && !(man&0x80)) { \
        exp -= 1; \
        man <<= 1; \
      } \
      flt32_map->zero = 0; \
      flt32_map->man = man & 0x7f; \
      flt32_map->exp = man ? exp & 0xff : 0; \
      flt32_map->sign = (_sign_) & 0x1; \
    } while(0)

  if(memx_status_no_error(status)&&(gbf80_buffer == NULL))
    status = MEMX_STATUS_GBF_INVALID_DATA;
  if(memx_status_no_error(status)&&(flt32_buffer == NULL))
    status = MEMX_STATUS_GBF_INVALID_DATA;
  if(memx_status_no_error(status)&&(length < 0))
    status = MEMX_STATUS_GBF_INVALID_LENGTH;

  while(memx_status_no_error(status)&&(flt32_offset < length)) {
    gbf80 = gbf80_buffer + gbf80_offset;
    flt32 = flt32_buffer + flt32_offset;

    gbf80_map = (MemxGbfGbf80Map*)gbf80;
    if(flt32_offset+0 < length) {
      flt32_map = (MemxGbfFloat32Map*)(flt32+0);
      _SET_FLOAT32_WITH_LEADING_ONE_ADJUST(
        gbf80_map->man_0,
        gbf80_map->exp,
        gbf80_map->sign_0);
    }
    if(flt32_offset+1 < length) {
      flt32_map = (MemxGbfFloat32Map*)(flt32+1);
      _SET_FLOAT32_WITH_LEADING_ONE_ADJUST(
        gbf80_map->man_1,
        gbf80_map->exp,
        gbf80_map->sign_1);
    }
    if(flt32_offset+2 < length) {
      flt32_map = (MemxGbfFloat32Map*)(flt32+2);
      _SET_FLOAT32_WITH_LEADING_ONE_ADJUST(
        gbf80_map->man_2,
        gbf80_map->exp,
        gbf80_map->sign_2);
    }
    if(flt32_offset+3 < length) {
      flt32_map = (MemxGbfFloat32Map*)(flt32+3);
      _SET_FLOAT32_WITH_LEADING_ONE_ADJUST(
        (gbf80_map->man_3_1 << 5) | gbf80_map->man_3_0,
        gbf80_map->exp,
        gbf80_map->sign_3);
    }
    if(flt32_offset+4 < length) {
      flt32_map = (MemxGbfFloat32Map*)(flt32+4);
      _SET_FLOAT32_WITH_LEADING_ONE_ADJUST(
        gbf80_map->man_4,
        gbf80_map->exp,
        gbf80_map->sign_4);
    }
    if(flt32_offset+5 < length) {
      flt32_map = (MemxGbfFloat32Map*)(flt32+5);
      _SET_FLOAT32_WITH_LEADING_ONE_ADJUST(
        gbf80_map->man_5,
        gbf80_map->exp,
        gbf80_map->sign_5);
    }
    if(flt32_offset+6 < length) {
      flt32_map = (MemxGbfFloat32Map*)(flt32+6);
      _SET_FLOAT32_WITH_LEADING_ONE_ADJUST(
        gbf80_map->man_6,
        gbf80_map->exp,
        gbf80_map->sign_6);
    }
    if(flt32_offset+7 < length) {
      flt32_map = (MemxGbfFloat32Map*)(flt32+7);
      _SET_FLOAT32_WITH_LEADING_ONE_ADJUST(
        (gbf80_map->man_7_1 << 1) | gbf80_map->man_7_0,
        gbf80_map->exp,
        gbf80_map->sign_7);
    }

    gbf80_offset += 10;
    flt32_offset += 8;
  }

  return status;
}

