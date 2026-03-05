#ifndef CONVERT_H_
#define CONVERT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _MemxGbfGbf80Map
{
    unsigned int man_0 : 8;
    unsigned int sign_0 : 1;
    unsigned int man_1 : 8;
    unsigned int sign_1 : 1;
    unsigned int man_2 : 8;
    unsigned int sign_2 : 1;
    unsigned int man_3_0 : 5;
    unsigned int man_3_1 : 3;
    unsigned int sign_3 : 1;
    unsigned int man_4 : 8;
    unsigned int sign_4 : 1;
    unsigned int man_5 : 8;
    unsigned int sign_5 : 1;
    unsigned int man_6 : 8;
    unsigned int sign_6 : 1;
    unsigned int man_7_0 : 1;
    unsigned int man_7_1 : 7;
    unsigned int sign_7 : 1;
    unsigned int exp : 8;
} MemxGbfGbf80Map;

typedef struct _MemxGbfFloat32Map
{
    unsigned int zero : 16;
    unsigned int man : 7;
    unsigned int exp : 8;
    unsigned int sign : 1;
} MemxGbfFloat32Map;

void gbf_encode(float *flt32_buffer, uint8_t *gbf80_buffer, int length);
void gbf_decode(uint8_t *gbf80_buffer, float *flt32_buffer, unsigned int length);

void convert_gbf(float *src, uint8_t *dst, int tensor_size, int num_ch);
void convert_gbf_row_pad(float *src, uint8_t *dst, int height, int width, int z, int num_ch);
void unconvert_gbf(uint8_t *src, float *dst, int tensor_size, int num_ch);
void unconvert_gbf_hpoc(uint8_t *src, float *dst, int height, int width, int z, int num_ch, int hpoc_size, int *hpoc_indexes, int row_pad);
void unconvert_gbf_row_pad(uint8_t *src, float *dst, int height, int width, int z, int num_ch);
void convert_bf16(float *src, uint8_t *dst, int tensor_size);
void unconvert_bf16(uint8_t *src, float *dst, int tensor_size);

#ifdef __cplusplus
}
#endif

#endif // CONVERT_H_
