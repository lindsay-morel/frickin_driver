/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Inc. All rights reserved.
 *
 ******************************************************************************/
#ifndef DFP_DECODE_H_
#define DFP_DECODE_H_

#ifdef __cplusplus
extern "C" {
#endif


/***************************************************************************//**
 * includes
 ******************************************************************************/
#include "memx_status.h"
#include "memx_common.h"

/**
 * @brief Model input or output port configuration. Information is obtained from
 * DFP unencrypted secion and returned with unified format for DFP file
 * version-independent purpose.
 */
typedef struct _MemxDfpPortConfig
{
  uint8_t port; // port index
  uint8_t port_set; // port set
  uint8_t mpu_id; // MPU ID
  uint8_t model_index; // model index
  uint8_t active; // 1: active, 0: inactive
  uint8_t format; // input port = 0:GBF80, 1:RGB888, 2:RGB565, 3:YUV422, 4:YUY2, 5:BF16?; output port = 0:GBF80, 1:BF16?
  uint16_t dim_x; // shape dimension x (height)
  uint16_t dim_y; // shape dimension y (width)
  uint16_t dim_z; // shape dimension z
  uint32_t dim_c; // shape dimension c (channels (user, after HPOC))

  // input port only:
  uint8_t range_convert_enabled; // FP->RGB using data ranges conversion
  float range_convert_shift; // amount to shift before scale
  float range_convert_scale; // amount to scale before integer cast

  // outport port only:
  uint8_t  hpoc_en; // HPOC enabled/disabled
  uint32_t hpoc_dim_c; // expanded HPOC channels shape
  uint16_t hpoc_list_length; // HPOC channel list length
  uint16_t *hpoc_dummy_channels; // list of dummy channels to remove

} MemxDfpPortConfig;

typedef struct _dfpcontext
{
    uint32_t            identifier_data;
    uint32_t            dfp_attr;
    uint32_t            input_mode_flag;
    uint32_t            input_port_number;
    uint32_t            output_port_number;
    uint32_t            weight_size;
    uint32_t            config_size;
    MemxDfpPortConfig   *pInputConfigList;
    MemxDfpPortConfig   *pOuputConfigList;
    uint8_t             *pWeightBaseAdr;
    uint8_t             *pRgCfgBaseAdr;
} DfpContext, *pDfpContext;

typedef enum _MemxDfpInputMode {
  MEMX_DFP_INPUT_MODE_BLOCKING = 0,
  MEMX_DFP_INPUT_MODE_NOBLOCKING = 1,
} _MemxDfpInputMode;

/**
 * @brief Loads firmware from a .bin file.
 * Currently limited to Cascade.
 *
 * @param filename            .bin file path
 * @param dest                ptr to bytes buffer with data (we will alloc its mem)
 * @param read_bytes          number of bytes that were read into dest
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_load_firmware(const char *filename, uint8_t **dest, uint32_t *read_bytes);

/**
 * @brief Parsing dfp context from source and ouput the information to DfpMeta
 *
 * @param pSrc                dfp source, may be file or buffer
 * @param dfp_attr            indicate some information about the dfp
 * @param idx                 which rgcfg array to use (legacy value, only 0 is support)
 * @param pDfpMeta            output result, collect infomration that driver required
 *
 * @return 0 on success, otherwise error code
 */
memx_status memx_dfp_parsing_context(const char* pSrc, int dfp_attr, uint8_t idx, void *pDfpMeta);

/**
 * @brief Get DFP Parsing Result by Model ID
 *
 * @param model_id  model ID
 *
 * @return pDfpMeta pointer or null pointer
 */
pDfpContext memx_dfp_get_dfp_mata(uint8_t model_id);

/**
 * @brief Free DFP Parsing Result by Model ID, This free function only use for dfp meta allocated by udriver
 *
 * @param model_id  model ID
 *
 * @return N/A
 */
void memx_free_dfp_meta(uint8_t model_id);

/**
 * @brief Check pointer valid or not in the DFP Cache Entry
 *
 * @param pCache  the pointer to cache entry
 *
 * @param type    the user input download option
 *
 * @return MEMX_STATUS
*/
memx_status memx_dfp_check_cache_entry(DfpContext* pCache, int type);

#ifdef __cplusplus
}
#endif

#endif /* DFP_DECODE_H_ */
