/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Inc. All rights reserved.
 *
 ******************************************************************************/
#include "memx_global_config.h"
#include "memx_platform.h"
#include "mxpack.h"
#include "memx_mpu_comm.h"
#include <stdlib.h>

#define USED_DATA_FROM_BUFFER (0)
#define COPY_DATA_FROM_BUFFER (1)

#define INPUT_PORT  (0)
#define OUTPUT_PORT (1)
DfpContext gDfpMeta[32] = {0};
/***************************************************************************//**
 * DFPv6 helper function
 ******************************************************************************/
static void dfp_meta_free(DfpContext* meta) {
    if (meta){
        if (meta->pInputConfigList) {
            for (uint32_t i = 0; i < meta->input_port_number; i++) {
                if (meta->pInputConfigList[i].hpoc_dummy_channels) {
                    free(meta->pInputConfigList[i].hpoc_dummy_channels);
                    meta->pInputConfigList[i].hpoc_dummy_channels = NULL;
                }
            }
            free(meta->pInputConfigList);
            meta->pInputConfigList = NULL;
        }

        if (meta->pOuputConfigList) {
            for (uint32_t i = 0; i < meta->output_port_number; i++) {
                if (meta->pOuputConfigList[i].hpoc_dummy_channels) {
                    free(meta->pOuputConfigList[i].hpoc_dummy_channels);
                    meta->pOuputConfigList[i].hpoc_dummy_channels = NULL;
                }
            }
            free(meta->pOuputConfigList);
            meta->pOuputConfigList = NULL;
        }

        if(!(meta->dfp_attr & MEMX_MPU_DOWNLOAD_MODEL_TYPE_BUFFER)){
            if (meta->pWeightBaseAdr) {
                free(meta->pWeightBaseAdr);
                meta->pWeightBaseAdr = NULL;
            }

            if (meta->pRgCfgBaseAdr) {
                free(meta->pRgCfgBaseAdr);
                meta->pRgCfgBaseAdr = NULL;
            }
        }

        memset(meta, 0, sizeof(DfpContext));
    }
}

static memx_status dfpv6_extract_port(mxpack_list_t *plist, MemxDfpPortConfig** port_cfgs, uint8_t port_type) {
    memx_status         status      = MEMX_STATUS_OK;
    mxpack_dict_t       *tempd      = NULL;
    mxpack_list_t       *templ      = NULL;
    MemxDfpPortConfig   *port_cfg   = NULL;
    uint8_t             enabled     = 0;
    // all inports
    for(uint8_t i=0; i < plist->num_elem; i++){
        port_cfg = &(*port_cfgs)[i];

        mxpack_dict_t *pd = (mxpack_dict_t*) mxpack_get_list_item_ptr(plist, i);
        if(pd == NULL){
            SET_STATUS_AND_GOTO(status, MEMX_STATUS_NULL_POINTER, done);
        } else {
            port_cfg->port = *((uint8_t*) mxpack_get_keyval(pd, "port"));
            enabled = *((uint8_t*) mxpack_get_keyval(pd, "active"));

            if(enabled == 0){
                port_cfg->active = 0;
            } else {
                port_cfg->active = 1;

                // port_set
                port_cfg->port_set = *((uint8_t*) mxpack_get_keyval(pd, "port_set"));

                // mpu_id
                port_cfg->mpu_id = *((uint8_t*) mxpack_get_keyval(pd, "mpu_id"));

                // model_index
                port_cfg->model_index = *((uint8_t*) mxpack_get_keyval(pd, "model_index"));

                // format
                tempd = (mxpack_dict_t*) mxpack_get_keyval(pd, "packing_format");
                port_cfg->format = *((uint8_t*) mxpack_get_keyval(tempd, "as_int"));

                // shape
                templ = (mxpack_list_t*) mxpack_get_keyval(pd, "mxa_shape");
                switch(templ->dtype){
                    case MXPACK_UINT8:
                        port_cfg->dim_x = *((uint8_t*) mxpack_get_list_item_ptr(templ, 0));
                        port_cfg->dim_y = *((uint8_t*) mxpack_get_list_item_ptr(templ, 1));
                        port_cfg->dim_z = *((uint8_t*) mxpack_get_list_item_ptr(templ, 2));
                        port_cfg->dim_c = *((uint8_t*) mxpack_get_list_item_ptr(templ, 3));
                        break;
                    case MXPACK_UINT16:
                        port_cfg->dim_x = *((uint16_t*) mxpack_get_list_item_ptr(templ, 0));
                        port_cfg->dim_y = *((uint16_t*) mxpack_get_list_item_ptr(templ, 1));
                        port_cfg->dim_z = *((uint16_t*) mxpack_get_list_item_ptr(templ, 2));
                        port_cfg->dim_c = *((uint16_t*) mxpack_get_list_item_ptr(templ, 3));
                        break;
                    case MXPACK_UINT32:
                        port_cfg->dim_x = (uint16_t) *((uint32_t*) mxpack_get_list_item_ptr(templ, 0));
                        port_cfg->dim_y = (uint16_t) *((uint32_t*) mxpack_get_list_item_ptr(templ, 1));
                        port_cfg->dim_z = (uint16_t) *((uint32_t*) mxpack_get_list_item_ptr(templ, 2));
                        port_cfg->dim_c = (uint16_t) *((uint32_t*) mxpack_get_list_item_ptr(templ, 3));
                        break;
                    default:
                        SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, done);
                }

                if(port_type == INPUT_PORT){// range convert stuff
                    tempd = (mxpack_dict_t*) mxpack_get_keyval(pd, "range_convert");
                    port_cfg->range_convert_enabled = *((uint8_t*) mxpack_get_keyval(tempd, "enabled"));
                    if(port_cfg->range_convert_enabled){
                        port_cfg->range_convert_shift = *((float*) mxpack_get_keyval(tempd, "shift"));
                        port_cfg->range_convert_scale = *((float*) mxpack_get_keyval(tempd, "scale"));
                    } else {
                        port_cfg->range_convert_shift = 0.0;
                        port_cfg->range_convert_scale = 1.0;
                    }
                }

                if(port_type == OUTPUT_PORT){// HPOC stuff
                    tempd = (mxpack_dict_t*) mxpack_get_keyval(pd, "hpoc");
                    port_cfg->hpoc_en = *((uint8_t*) mxpack_get_keyval(tempd, "enabled"));

                    if(port_cfg->hpoc_en){
                        // hpoc'd channel shape
                        templ = (mxpack_list_t*) mxpack_get_keyval(tempd, "shape");
                        switch(templ->dtype){
                            case MXPACK_UINT8:
                                port_cfg->hpoc_dim_c = *((uint8_t*) mxpack_get_list_item_ptr(templ, 3));
                                break;
                            case MXPACK_UINT16:
                                port_cfg->hpoc_dim_c = *((uint16_t*) mxpack_get_list_item_ptr(templ, 3));
                                break;
                            case MXPACK_UINT32:
                                port_cfg->hpoc_dim_c = *((uint32_t*) mxpack_get_list_item_ptr(templ, 3));
                                break;
                            default:
                                SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, done);
                        }

                        // dummychan list
                        templ = (mxpack_list_t*) mxpack_get_keyval(tempd, "channels");
                        port_cfg->hpoc_list_length = (uint16_t) templ->num_elem;
                        port_cfg->hpoc_dummy_channels = (uint16_t*) malloc(templ->num_elem * sizeof(uint16_t));

                        for(uint8_t z = 0; z < port_cfg->hpoc_list_length; z++){
                            switch(templ->dtype){
                                case MXPACK_UINT8:
                                    port_cfg->hpoc_dummy_channels[z] = *((uint8_t*) mxpack_get_list_item_ptr(templ, z));
                                    break;
                                case MXPACK_UINT16:
                                    port_cfg->hpoc_dummy_channels[z] = *((uint16_t*) mxpack_get_list_item_ptr(templ, z));
                                    break;
                                case MXPACK_UINT32:
                                    port_cfg->hpoc_dummy_channels[z] = (uint16_t) *((uint32_t*) mxpack_get_list_item_ptr(templ, z));
                                    break;
                                default:
                                    SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, done);
                            }
                        }
                    }
                }
            } // if enabled
        }
    } // for all port
done:
    return status;
}

static void mark_multi_input_model(pDfpContext pDfpMeta) {
    pDfpMeta->input_mode_flag = 0;
    for(uint8_t i=0; i < pDfpMeta->input_port_number ; i++){
        MemxDfpPortConfig *port_cfg = &(pDfpMeta->pInputConfigList[i]);
        if(port_cfg->active == 1){
            for(uint8_t j=0; j < pDfpMeta->input_port_number ; j++){
                if(i != j){
                    MemxDfpPortConfig *port_cfg2 =  &(pDfpMeta->pInputConfigList[j]);
                    if(port_cfg2->active == 1){
                        if(port_cfg2->model_index == port_cfg->model_index){
                            pDfpMeta->input_mode_flag |= (MEMX_DFP_INPUT_MODE_NOBLOCKING << i);
                            pDfpMeta->input_mode_flag |= (MEMX_DFP_INPUT_MODE_NOBLOCKING << j);
                        } // if same model
                    } //if(port_cfg2 active)
                } // if(i!=j)
            } // for j in inports again
        } // if this port_cfg active
    } // main scan loop
}

static memx_status parse_dfp_v6_mxpack(uint8_t* mxpack_data, uint8_t idx, pDfpContext pDfpMeta, uint8_t option) {
    memx_status         status          = MEMX_STATUS_OK;
    mxpack_dict_t       d               = {0};
    mxpack_binary_t*    sdfp            = NULL;
    mxpack_list_t*      inport_info     = NULL;
    mxpack_list_t*      outport_info    = NULL;
    uint8_t*            pSrc            = NULL;
    size_t              hoffset         = 0;
    uint8_t             num_rgcfg       = 0;

    size_t parsed = mxpack_process_dict(&d, mxpack_data);
    if (parsed == 0) {
        SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
    }

    sdfp = (mxpack_binary_t*) mxpack_get_keyval(&d, "hw_dfp");
    if (!sdfp) {
        SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
    }

    pSrc = (option & COPY_DATA_FROM_BUFFER) ? sdfp->data : mxpack_get_hw_dfp_offset();
    hoffset += sizeof(uint64_t); // skip hw_data_len

    pDfpMeta->weight_size = *(uint32_t*)(pSrc + hoffset);
    hoffset += sizeof(uint32_t);

    if (option & COPY_DATA_FROM_BUFFER) {
        pDfpMeta->pWeightBaseAdr = malloc(pDfpMeta->weight_size);
        if (!pDfpMeta->pWeightBaseAdr) {
            SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
        }
        memcpy(pDfpMeta->pWeightBaseAdr, pSrc + hoffset, pDfpMeta->weight_size);
    } else {
        pDfpMeta->pWeightBaseAdr = pSrc + hoffset;
    }

    hoffset += pDfpMeta->weight_size;

    num_rgcfg = *(uint8_t*)(pSrc + hoffset);
    hoffset += 1;

    if (idx >= num_rgcfg) {
        SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_INVALID_PARAMETER, cleanup);
    }

    for (uint8_t i = 0; i < idx; i++) {
        uint32_t skip_len = *(uint32_t*)(pSrc + hoffset);
        hoffset += sizeof(uint32_t) + skip_len;
    }

    pDfpMeta->config_size = *(uint32_t*)(pSrc + hoffset);
    hoffset += sizeof(uint32_t);

    if (option & COPY_DATA_FROM_BUFFER) {
        pDfpMeta->pRgCfgBaseAdr = malloc(pDfpMeta->config_size);
        if (!pDfpMeta->pRgCfgBaseAdr) {
            SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
        }
        memcpy(pDfpMeta->pRgCfgBaseAdr, pSrc + hoffset, pDfpMeta->config_size);
    } else {
        pDfpMeta->pRgCfgBaseAdr = pSrc + hoffset;
    }

    // --- Inports ---
    pDfpMeta->input_port_number = *(uint8_t*) mxpack_get_keyval(&d, "num_inports");
    pDfpMeta->pInputConfigList = (MemxDfpPortConfig*) calloc(pDfpMeta->input_port_number, sizeof(MemxDfpPortConfig));
    if (!pDfpMeta->pInputConfigList) {
        SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
    }

    inport_info = (mxpack_list_t*) mxpack_get_keyval(&d, "inport_info");
    if (!inport_info || inport_info->num_elem != pDfpMeta->input_port_number) {
        SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
    }

    status = dfpv6_extract_port(inport_info, &pDfpMeta->pInputConfigList, INPUT_PORT);
    if (status != MEMX_STATUS_OK) {
        SET_STATUS_AND_GOTO(status, status, cleanup);
    }

    // --- Outports ---
    pDfpMeta->output_port_number = *(uint8_t*) mxpack_get_keyval(&d, "num_outports");
    pDfpMeta->pOuputConfigList = (MemxDfpPortConfig*) calloc(pDfpMeta->output_port_number, sizeof(MemxDfpPortConfig));
    if (!pDfpMeta->pOuputConfigList) {
        SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
    }

    outport_info = (mxpack_list_t*) mxpack_get_keyval(&d, "outport_info");
    if (!outport_info || outport_info->num_elem != pDfpMeta->output_port_number) {
        SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
    }

    status = dfpv6_extract_port(outport_info, &pDfpMeta->pOuputConfigList, OUTPUT_PORT);
cleanup:
    mxpack_free_dict(&d);
    return status;
}

static memx_status memx_dfp_load_all_buffer(uint8_t *pBuffer, uint8_t idx, pDfpContext pDfpMeta) {
    uint64_t        sim_data_len    = 0;
    memx_status     status          = MEMX_STATUS_OK;
    uint64_t        hw_data_len     = 0;
    uint32_t        section_len     = 0;
    uint32_t        buffer_offset   = 0;

    if (idx != 0 || !pDfpMeta) {
        SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_INVALID_PARAMETER, cleanup);
    }

    sim_data_len = *(uint64_t *)(pBuffer + buffer_offset);
    buffer_offset += 8;

    if (sim_data_len == 6) {
        uint8_t dtype = *(pBuffer + buffer_offset);
        if(dtype != 0x01){ // not a dict!
            SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_INVALID_FILE, cleanup);
        } else {
            buffer_offset += 1;
            status = parse_dfp_v6_mxpack(pBuffer + buffer_offset, idx, pDfpMeta, USED_DATA_FROM_BUFFER);
        }
    } else if (sim_data_len == 3 || sim_data_len == 4 || sim_data_len == 5 || sim_data_len > 50) {
        // get the actual data length
        if (sim_data_len < 50) {
            sim_data_len = *(uint64_t *)(pBuffer + buffer_offset);
            buffer_offset += 8;
        }

        size_t sim_data_start_offset = buffer_offset; //record offset for sim data

        buffer_offset += (uint32_t) sim_data_len; //skip sim data for hw_data

        hw_data_len = *(uint64_t *)(pBuffer + buffer_offset);
        buffer_offset += 8;

        if (hw_data_len == 0) {
            SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
        } else {
            pDfpMeta->weight_size = *(uint32_t *)(pBuffer + buffer_offset);
            buffer_offset += 4;
            pDfpMeta->pWeightBaseAdr = pBuffer + buffer_offset;
            // Skip section length
            buffer_offset += pDfpMeta->weight_size;

            // skip over the "num rgcfg files" byte
            buffer_offset += 1;

            pDfpMeta->pRgCfgBaseAdr = pBuffer + buffer_offset;

            // loop over rgcfg sections until we get to [idx] number
            for (uint8_t i = 0; i < idx; i++) {
                section_len = *(uint32_t *)(pBuffer + buffer_offset);
                buffer_offset += (4 + section_len);
            }

            pDfpMeta->config_size = *(uint32_t *)(pBuffer + buffer_offset);
            buffer_offset += 4;
            pDfpMeta->pRgCfgBaseAdr = pBuffer + buffer_offset;
        }

        buffer_offset = (uint32_t) sim_data_start_offset; //back to sim data

        uint8_t date_length = 0;
        date_length = *(pBuffer + buffer_offset);
        buffer_offset += (1 + date_length);

        uint32_t model_info_length = 0;
        model_info_length = *(uint32_t *)(pBuffer + buffer_offset);
        buffer_offset += (4 + model_info_length);

        uint8_t compiler_version_length = 0;
        compiler_version_length = *(pBuffer + buffer_offset);
        buffer_offset += (1 + compiler_version_length);

        uint32_t args_length = 0;
        args_length = *(uint32_t *)(pBuffer + buffer_offset);
        buffer_offset += (4 + args_length);

        buffer_offset += 4;

        uint8_t inports, outports;
        inports = *(pBuffer + buffer_offset);
        buffer_offset += 1;
        outports = *(pBuffer + buffer_offset);
        buffer_offset += 1;

        pDfpMeta->input_port_number = inports;
        pDfpMeta->pInputConfigList= (MemxDfpPortConfig*)calloc(inports, sizeof(MemxDfpPortConfig));
        if(pDfpMeta->pInputConfigList == NULL) {
            SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_NOT_ENOUGH_MEMORY, cleanup);
        } else {
            uint16_t layernameleng = 0;
            uint8_t index_and_status;

            for(uint8_t i=0; i < inports; i++) {
                MemxDfpPortConfig *port_cfg = &(pDfpMeta->pInputConfigList[i]);

                index_and_status = *(pBuffer + buffer_offset);
                buffer_offset += 1;

                if((index_and_status & 0x80) == 0x80) {// ACTIVE inport
                    port_cfg->port = index_and_status & 0x7F;
                    port_cfg->active = 1;

                    port_cfg->port_set = *(pBuffer + buffer_offset);
                    buffer_offset += 1;

                    port_cfg->mpu_id = *(pBuffer + buffer_offset);
                    buffer_offset += 1;

                    port_cfg->model_index = *(pBuffer + buffer_offset);
                    buffer_offset += 1;

                    layernameleng = *(uint16_t *)(pBuffer + buffer_offset);
                    buffer_offset += (2 + layernameleng);

                    port_cfg->format = *(pBuffer + buffer_offset);
                    buffer_offset += 1;

                    port_cfg->range_convert_enabled = *(pBuffer + buffer_offset);
                    buffer_offset += 1;

                    port_cfg->range_convert_shift = *(float *)(pBuffer + buffer_offset);
                    buffer_offset += 4;

                    port_cfg->range_convert_scale = *(float *)(pBuffer + buffer_offset);
                    buffer_offset += 4;

                    port_cfg->dim_x = *(uint16_t *)(pBuffer + buffer_offset);
                    buffer_offset += 2;
                    port_cfg->dim_y = *(uint16_t *)(pBuffer + buffer_offset);
                    buffer_offset += 2;
                    port_cfg->dim_z = *(uint16_t *)(pBuffer + buffer_offset);
                    buffer_offset += 2;
                    port_cfg->dim_c = *(uint32_t *)(pBuffer + buffer_offset);
                    buffer_offset += 4;
                }
            }
        }

        pDfpMeta->output_port_number = outports;
        pDfpMeta->pOuputConfigList = (MemxDfpPortConfig*)calloc(outports, sizeof(MemxDfpPortConfig));
        if(pDfpMeta->pOuputConfigList  == NULL) {
            SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_NOT_ENOUGH_MEMORY, cleanup);
        } else {
            uint8_t hpoc_enabled = 0;
            uint16_t hpoc_list_length = 0;

            for(uint8_t i=0; i < outports; i++) {
                MemxDfpPortConfig *port_cfg = &(pDfpMeta->pOuputConfigList[i]);

                uint8_t index_and_status = *(pBuffer + buffer_offset);
                buffer_offset += 1;

                if((index_and_status & 0x80) == 0x80) { // ACTIVE outport
                    port_cfg->port = index_and_status & 0x7F;
                    port_cfg->active = 1;

                    port_cfg->port_set = *(pBuffer + buffer_offset);
                    buffer_offset += 1;

                    port_cfg->mpu_id = *(pBuffer + buffer_offset);
                    buffer_offset += 1;

                    port_cfg->model_index = *(pBuffer + buffer_offset);
                    buffer_offset += 1;

                    uint16_t layer_name_length = *(uint16_t *)(pBuffer + buffer_offset);
                    buffer_offset += (2 + layer_name_length);

                    port_cfg->format = *(pBuffer + buffer_offset);
                    buffer_offset += 1;

                    port_cfg->dim_x = *(uint16_t *)(pBuffer + buffer_offset);
                    buffer_offset += 2;
                    port_cfg->dim_y = *(uint16_t *)(pBuffer + buffer_offset);
                    buffer_offset += 2;
                    port_cfg->dim_z = *(uint16_t *)(pBuffer + buffer_offset);
                    buffer_offset += 2;
                    port_cfg->dim_c = *(uint32_t *)(pBuffer + buffer_offset);
                    buffer_offset += 4;

                    hpoc_enabled = *(pBuffer + buffer_offset);
                    buffer_offset += 1;
                    port_cfg->hpoc_en = hpoc_enabled;
                    if (hpoc_enabled == 1) {
                        // skip hpoc_dim_x,y,z
                        buffer_offset += 6;

                        port_cfg->hpoc_dim_c = *(uint32_t *)(pBuffer + buffer_offset);
                        buffer_offset += 4;

                        hpoc_list_length = *(uint16_t *)(pBuffer + buffer_offset);
                        buffer_offset += 2;
                        port_cfg->hpoc_list_length = hpoc_list_length;
                        port_cfg->hpoc_dummy_channels = (uint16_t*) calloc(hpoc_list_length, sizeof(uint16_t));
                        if (port_cfg->hpoc_dummy_channels == NULL) {
                            SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_NOT_ENOUGH_MEMORY, cleanup);
                        } else {
                            uint16_t temp_ch = 0;
                            for (int z = 0; z < hpoc_list_length; z++) {
                                temp_ch = *(uint16_t *)(pBuffer + buffer_offset);
                                buffer_offset += 2;
                                (port_cfg->hpoc_dummy_channels)[z] = temp_ch;
                            }
                        }
                    } else {
                        port_cfg->hpoc_dummy_channels = NULL;
                    }
                }
            }
        }
    } else {
        SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_VERSION_TOO_OLD, cleanup);
    }

    mark_multi_input_model(pDfpMeta);

cleanup:
    return status;
}

static memx_status memx_dfp_load_all(const char* filename, uint8_t idx, pDfpContext pDfpMeta) {
    uint64_t        sim_data_len    = 0;
    memx_status     status          = MEMX_STATUS_OK;
    uint64_t        hw_data_len     = 0;
    uint32_t        section_len     = 0;
    FILE*           fp              = NULL;

    fp = fopen(filename, "rb");
    if (!fp) {
        SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_INVALID_FILE, cleanup);
    }

    if (idx != 0 || !pDfpMeta) {
        SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_INVALID_PARAMETER, cleanup);
    }

    if (fread(&sim_data_len, sizeof(sim_data_len), 1, fp) != 1) {
        SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
    }

    if (sim_data_len == 6) {
        uint8_t *pBuffer = NULL;
        size_t prev = ftell(fp);

        // get size
        fseek(fp, 0L, SEEK_END);
        size_t sz = ftell(fp) - prev;

        // alloc
        pBuffer = (uint8_t*) malloc(sz);

        // read data to pBuffer
        fseek(fp, (long) prev, SEEK_SET);
        if(fread(pBuffer, sz, 1, fp) != 1){
            SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
        }

        // check for dict header in DFP
        uint8_t dtype = pBuffer[0];

        if(dtype != 0x01){
            free(pBuffer);
            SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_INVALID_FILE, cleanup);
        }

        status = parse_dfp_v6_mxpack(pBuffer + 1, idx, pDfpMeta, COPY_DATA_FROM_BUFFER);
        free(pBuffer);
    } else if (sim_data_len == 3 || sim_data_len == 4 || sim_data_len == 5 || sim_data_len > 50) {
        // get the actual data length
        if (sim_data_len < 50) {
            if (fread(&sim_data_len, sizeof(sim_data_len), 1, fp) != 1) {
                SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
            }
        }

        size_t prev = ftell(fp); //record file pointer for port information

        fseek(fp, (long)sim_data_len, SEEK_CUR);

        if (fread(&hw_data_len, sizeof(hw_data_len), 1, fp) != 1 || hw_data_len == 0) {
            SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
        }

        if (fread(&section_len, sizeof(section_len), 1, fp) != 1) {
            SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
        }
        pDfpMeta->pWeightBaseAdr = malloc(section_len);
        if (!pDfpMeta->pWeightBaseAdr) {
            SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
        }
        if (fread(pDfpMeta->pWeightBaseAdr, section_len, 1, fp) != 1) {
            SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
        }
        pDfpMeta->weight_size = section_len;

        fseek(fp, 1, SEEK_CUR);

        // loop over rgcfg sections until we get to [idx] number
        for (uint8_t i = 0; i < idx; i++) {
            if (fread(&section_len, sizeof(section_len), 1, fp) != 1) {
                SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
            }
            // skip over all those bytes
            fseek(fp, section_len, SEEK_CUR);
        }

        // get num bytes in DESIRED rgcfg section
        if (fread(&section_len, sizeof(section_len), 1, fp) != 1) {
            SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
        }

        // alloc the space
        pDfpMeta->pRgCfgBaseAdr = malloc(section_len);
        if (!pDfpMeta->pRgCfgBaseAdr) {
            SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
        }

        // read the data in
        if (fread(pDfpMeta->pRgCfgBaseAdr, section_len, 1, fp) != 1) {
            SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
        }

        pDfpMeta->config_size = section_len;

        fseek(fp, (long) prev, SEEK_SET);

        // skip compile date/time
        uint8_t dateleng = 0;
        if(fread(&dateleng, sizeof(uint8_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
        if(fseek(fp, dateleng, SEEK_CUR) != 0) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);

        // skip over model names
        uint32_t modelinfoleng = 0;
        if(fread(&modelinfoleng, sizeof(uint32_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
        if(fseek(fp, modelinfoleng, SEEK_CUR) != 0) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);

        // skip over compiler version
        uint8_t compilerverleng = 0;
        if(fread(&compilerverleng, sizeof(uint8_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
        if(fseek(fp, compilerverleng, SEEK_CUR) != 0) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);

        // skip over compiler argument list
        uint32_t argsleng = 0;
        if(fread(&argsleng, sizeof(uint32_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
        if(fseek(fp, argsleng, SEEK_CUR) != 0) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);

        // skip over 4 bytes of mix_sim metadata
        // (gen_and_towers, num_mpus, frequency)
        if(fseek(fp, 4, SEEK_CUR) != 0) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);

        // get the number of inport/outport
        uint8_t inports, outports;
        if(fread(&inports, sizeof(uint8_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
        if(fread(&outports, sizeof(uint8_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);

        pDfpMeta->input_port_number = inports;
        pDfpMeta->pInputConfigList = (MemxDfpPortConfig*)calloc(inports, sizeof(MemxDfpPortConfig));
        if(pDfpMeta->pInputConfigList == NULL) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_NOT_ENOUGH_MEMORY, cleanup);

        uint8_t index_and_status;
        uint16_t layernameleng = 0;

        for(uint8_t i=0; i < inports; i++){
            MemxDfpPortConfig *port_cfg = &(pDfpMeta->pInputConfigList[i]);

            if(fread(&index_and_status, sizeof(uint8_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);

            if((index_and_status & 0x80) == 0x80){// ACTIVE inport
                port_cfg->port = index_and_status & 0x7F;
                port_cfg->active = 1;

                // port_set
                if(fread(&(port_cfg->port_set), sizeof(uint8_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);

                // mpu_id
                if(fread(&(port_cfg->mpu_id), sizeof(uint8_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);

                // model_index
                if(fread(&(port_cfg->model_index), sizeof(uint8_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);

                // layer name length, and
                // skip the name string because we don't care
                if(fread(&layernameleng, sizeof(uint16_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
                if(fseek(fp, layernameleng, SEEK_CUR) != 0) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);

                // format
                if(fread(&(port_cfg->format), sizeof(uint8_t), 1, fp) != 1)SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);

                // data range enabled
                if(fread(&(port_cfg->range_convert_enabled), sizeof(uint8_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);

                // data range shift
                if(fread(&(port_cfg->range_convert_shift), sizeof(float), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);

                // data range scale
                if(fread(&(port_cfg->range_convert_scale), sizeof(float), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);

                // dim_x,y,z,c
                if(fread(&(port_cfg->dim_x), sizeof(uint16_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
                if(fread(&(port_cfg->dim_y), sizeof(uint16_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
                if(fread(&(port_cfg->dim_z), sizeof(uint16_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
                if(fread(&(port_cfg->dim_c), sizeof(uint32_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
            }
        }

        pDfpMeta->output_port_number = outports;
        pDfpMeta->pOuputConfigList = (MemxDfpPortConfig*)calloc(outports, sizeof(MemxDfpPortConfig));
        if(pDfpMeta->pOuputConfigList == NULL) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_NOT_ENOUGH_MEMORY, cleanup);

        uint8_t hpoc_enabled = 0;
        uint16_t hpoc_list_length = 0;
        // now we're at the outport data
        for(uint8_t i=0; i < outports; i++){
            MemxDfpPortConfig* port_cfg = &(pDfpMeta->pOuputConfigList[i]);

            if(fread(&index_and_status, sizeof(uint8_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);

            if((index_and_status & 0x80) == 0x80){// ACTIVE outport
                port_cfg->port = index_and_status & 0x7F;
                port_cfg->active = 1;

                // port_set
                if(fread(&(port_cfg->port_set), sizeof(uint8_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);

                // mpu_id
                if(fread(&(port_cfg->mpu_id), sizeof(uint8_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);

                // model_index
                if(fread(&(port_cfg->model_index), sizeof(uint8_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);

                // get layer name leng and skip it
                if(fread(&layernameleng, sizeof(uint16_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
                if(fseek(fp, layernameleng, SEEK_CUR) != 0) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);

                // format
                if(fread(&(port_cfg->format), sizeof(uint8_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);

                // dim_x,y,z,c
                if(fread(&(port_cfg->dim_x), sizeof(uint16_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
                if(fread(&(port_cfg->dim_y), sizeof(uint16_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
                if(fread(&(port_cfg->dim_z), sizeof(uint16_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
                if(fread(&(port_cfg->dim_c), sizeof(uint32_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);

                // HPOC info
                if(fread(&hpoc_enabled, sizeof(uint8_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
                port_cfg->hpoc_en = hpoc_enabled;
                if(hpoc_enabled == 1){
                    // skip hpoc_dim_x,y,z
                    if(fseek(fp, 6, SEEK_CUR) != 0) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
                    // get hpoc_dim_c
                    if(fread(&(port_cfg->hpoc_dim_c), sizeof(uint32_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);

                    // get dummy channel list leng
                    if(fread(&hpoc_list_length, sizeof(uint16_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
                    port_cfg->hpoc_list_length = hpoc_list_length;
                    port_cfg->hpoc_dummy_channels = (uint16_t*) calloc(hpoc_list_length, sizeof(uint16_t));
                     if(port_cfg->hpoc_dummy_channels == NULL) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_NOT_ENOUGH_MEMORY, cleanup);

                    uint16_t temp_ch = 0;
                    for (int z = 0; z < hpoc_list_length; z++){
                        if (fread(&temp_ch, sizeof(uint16_t), 1, fp) != 1) SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_READ_ERROR, cleanup);
                        (port_cfg->hpoc_dummy_channels)[z] = temp_ch;
                    }
                }
            }
        }
    }
    else {
        SET_STATUS_AND_GOTO(status, MEMX_STATUS_DFP_VERSION_TOO_OLD, cleanup);
    }

    mark_multi_input_model(pDfpMeta);

cleanup:
    if (fp) {
        fclose(fp);
        fp = NULL;
    }

    return status;
}

/***************************************************************************//**
 *
 ******************************************************************************/
 memx_status memx_load_firmware(const char *filename, uint8_t **dest, uint32_t *read_bytes) {
    memx_status status          = MEMX_STATUS_OK;
    uint32_t    fw_size         = 0;
    FILE *fp                    = fopen(filename, "rb");

    if(fp){
        //Get File Size
        fseek(fp, 0, SEEK_END);
        fw_size = ftell(fp);
        rewind(fp);

        //Allocate Space
        *dest = (uint8_t *)malloc(fw_size * sizeof(uint8_t));
        if(*dest) {
            *read_bytes = (uint32_t)fread(*dest, 1, fw_size, fp);
            if (*read_bytes != fw_size) {
                status =  MEMX_STATUS_FILE_READ_ERROR;
            }
        } else {
            status = MEMX_STATUS_OUT_OF_MEMORY;
        }
    } else {
        status = MEMX_STATUS_FILE_NOT_FOUND;
    }

    if(fp){
       fclose(fp);
    }
    if(memx_status_error(status)){
         free(*dest);
        *dest = NULL;
        *read_bytes = 0;
    }
    return status;
}

pDfpContext memx_dfp_get_dfp_mata(uint8_t model_id){
    return &gDfpMeta[model_id];
}

void memx_free_dfp_meta(uint8_t model_id){
    dfp_meta_free(&gDfpMeta[model_id]);
}

memx_status memx_dfp_parsing_context(const char* pSrc, int dfp_attr, uint8_t idx, void *pDfpcxt)
{
    memx_status status      = MEMX_STATUS_OK;
    uint8_t     *pbBuf      = (uint8_t *) (pSrc);
    pDfpContext pDfpMeta    = (pDfpContext) pDfpcxt;

    memset(pDfpMeta, 0, sizeof(DfpContext));
    pDfpMeta->dfp_attr = dfp_attr;

    if(dfp_attr & MEMX_MPU_DOWNLOAD_MODEL_TYPE_BUFFER) {
        status = memx_dfp_load_all_buffer(pbBuf, idx, pDfpMeta);
    } else {
        status = memx_dfp_load_all(pSrc, idx, pDfpMeta);
    }

    if(status != MEMX_STATUS_OK){
        dfp_meta_free(pDfpMeta);
    }
    return status;
}

memx_status memx_dfp_check_cache_entry(DfpContext* pCache, int type){
    memx_status status = MEMX_STATUS_OK;
    if(pCache){
        if(!pCache->pInputConfigList || !pCache->pInputConfigList || !pCache->pRgCfgBaseAdr || !pCache->pWeightBaseAdr){
            status = MEMX_STATUS_NULL_POINTER;
        } else {
            for(uint8_t i = 0; i < pCache->output_port_number; i++){
                if(pCache->pOuputConfigList[i].active){
                    if(pCache->pOuputConfigList[i].hpoc_en && (pCache->pOuputConfigList[i].hpoc_dummy_channels == NULL)){
                        status = MEMX_STATUS_NULL_POINTER;
                        break;
                    }
                } else {
                    break;
                }
            }
        }

        if(status == MEMX_STATUS_OK){ //add some attibute by driver
            type &= ~(MEMX_MPU_DOWNLOAD_MODEL_TYPE_BUFFER);
            pCache->dfp_attr = type;
            mark_multi_input_model(pCache);
        }
    } else {
        status = MEMX_STATUS_NULL_POINTER;
    }

    return status;
}