/***************************************************************************//**
 * Copyright (c) 2023 MemryX Inc.
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 ******************************************************************************/

#include <string>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <stdint.h>

#include "dfp.h"


using namespace Dfp;

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-//
// DataShapes                                                    //
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-//

DataShapes::DataShapes(){
    num_shapes = 0;
    sizes = NULL;
}

DataShapes::DataShapes(int num, unsigned int *sizes_){
    num_shapes = num;
    sizes = new unsigned int[num];
    memcpy(sizes, sizes_, num*sizeof(unsigned int));
}

DataShapes::~DataShapes(){
    if(sizes != NULL){
        delete [] sizes;
        sizes = NULL;
    }
}

DataShapes::DataShapes(const DataShapes &t){
    num_shapes = t.num_shapes;
    sizes = new unsigned int[num_shapes];
    memcpy(sizes, t.sizes, num_shapes*sizeof(unsigned int));
}

void DataShapes::set_num_shapes(int num){
    if(sizes != NULL)
        delete [] sizes;
    num_shapes = num;
    sizes = new unsigned int[num_shapes];
    memset(sizes, 0, num_shapes*sizeof(float));
}

void DataShapes::set_size(int idx, unsigned int size){
    if(idx < 0 || idx >= num_shapes)
        return;
    sizes[idx] = size;
}
        

DataShapes& DataShapes::operator=(const DataShapes& other){
    if(this != &other){
        if(other.num_shapes > 0 && other.sizes != NULL){
            if(sizes != NULL)
                delete [] sizes;
            num_shapes = other.num_shapes;
            sizes = new unsigned int[num_shapes];
            memcpy(sizes, other.sizes, num_shapes*sizeof(float));
        }
    }
    return *this;
}

unsigned int& DataShapes::operator[](std::size_t idx){
    if(idx < (size_t)num_shapes)
        return sizes[idx];
    else
        return sizes[0];
}

unsigned int DataShapes::operator[](std::size_t idx) const {
    if(idx < (size_t)num_shapes)
        return sizes[idx];
    else
        return 0;
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-//
// DfpObject                                                       //
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-//

// CTORs
//--------------------------------------------------

// ctor using c string
DfpObject::DfpObject(const char *f){
    iports = NULL;
    oports = NULL;
    if(__load_dfp_file(f) != 0){
        printf("Failed to load dfp file %s\n", f);
        valid = false;
        if(iports != NULL){
            delete [] iports;
            iports = NULL;
        }
        if(oports != NULL){
            for(int i=0; i < meta.num_outports; i++){
                if(oports[i].hpoc_dummy_channels != NULL){
                    delete [] oports[i].hpoc_dummy_channels;
                    oports[i].hpoc_dummy_channels = NULL;
                }
            }
            delete [] oports;
            oports = NULL;
        }
        meta.num_inports = 0;
        meta.num_outports = 0;
    } else {
        valid = true;
    };
}

// ctor using string
DfpObject::DfpObject(std::string f){
    iports = NULL;
    oports = NULL;
    if(__load_dfp_file(f.c_str()) != 0){
        printf("Failed to load dfp file %s\n", f.c_str());
        valid = false;
        meta.num_inports = 0;
        meta.num_outports = 0;
        if(iports != NULL){
            delete [] iports;
            iports = NULL;
        }
        if(oports != NULL){
            delete [] oports;
            oports = NULL;
        }
    } else {
        valid = true;
    };
}



// DTOR
//--------------------------------------------------
DfpObject::~DfpObject(){
    if(iports != NULL){
        delete [] iports;
        iports = NULL;
    }
    if(oports != NULL){
        for(int i=0; i < meta.num_outports; i++){
            if(oports[i].hpoc_dummy_channels != NULL){
                delete [] oports[i].hpoc_dummy_channels;
                oports[i].hpoc_dummy_channels = NULL;
            }
        }
        delete [] oports;
        oports = NULL;
    }
}



// various 'get' functions
//--------------------------------------------------

DfpMeta DfpObject::get_dfp_meta(){
    return meta;
}



// input shapes
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int DfpObject::get_input_shape_fmt(int port, uint16_t *dh, uint16_t *dw, uint16_t *dz, uint32_t *dc, PortDataFormat *pdf){

    if(iports == NULL) return -1;

    if(port < 0 || port >= meta.num_inports){
        printf("Invalid port %d given to get_input_shape\n", port);
        return -1;
    }


    *dh = iports[port].dim_h;
    *dw = iports[port].dim_w;
    *dz = iports[port].dim_z;
    *dc = iports[port].dim_c;

    if(iports[port].format == 0 || iports[port].format == 5 || iports[port].format == 6){
        *pdf = FLOAT;
    } else {
        *pdf = UINT8;
    }


    return 0;
}


int DfpObject::get_all_input_shapes_fmts(uint16_t *dhs, uint16_t *dws, uint16_t *dzs, uint32_t *dcs, PortDataFormat *pdfs){
    
    if(iports == NULL) return -1;

    for(int i=0; i < meta.num_inports; i++){
        dhs[i] = iports[i].dim_h;
        dws[i] = iports[i].dim_w;
        dzs[i] = iports[i].dim_z;
        dcs[i] = iports[i].dim_c;
        if(iports[i].format == 0 || iports[i].format == 5 || iports[i].format == 6){
            pdfs[i] = FLOAT;
        } else {
            pdfs[i] = UINT8;
        }
    }

    return 0;
}

DataShapes DfpObject::all_indata_shapes(){

    DataShapes dat;
    dat.set_num_shapes(meta.num_used_inports);
    for(int i=0; i < meta.num_used_inports; i++){
        dat.set_size(i, iports[i].total_size);
    }

    return dat;
}


DataShapes DfpObject::all_outdata_shapes(){

    DataShapes dat;
    dat.set_num_shapes(meta.num_used_outports);
    for(int i=0; i < meta.num_used_outports; i++){
        dat.set_size(i, oports[i].total_size);
    }

    return dat;
}


// output shapes
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int DfpObject::get_output_shape(int port, uint16_t *dh, uint16_t *dw, uint16_t *dz, uint32_t *dc){
    
    if(oports == NULL) return -1;

    if(port < 0 || port >= meta.num_outports){
        printf("Invalid port %d given to get_output_shape\n", port);
        return -1;
    }

    *dh = oports[port].dim_h;
    *dw = oports[port].dim_w;
    *dz = oports[port].dim_z;
    *dc = oports[port].dim_c;

    return 0;
}


int DfpObject::get_all_output_shapes(uint16_t *dhs, uint16_t *dws, uint16_t *dzs, uint32_t *dcs){
    
    if(oports == NULL) return -1;

    for(int i=0; i < meta.num_outports; i++){
        dhs[i] = oports[i].dim_h;
        dws[i] = oports[i].dim_w;
        dzs[i] = oports[i].dim_z;
        dcs[i] = oports[i].dim_c;
    }

    return 0;
}


// complete port info
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
PortInfo* DfpObject::input_port(int port){
    
    if(iports == NULL) return NULL;
    
    if(port < 0 || port >= meta.num_inports){
        printf("Invalid port %d given to get input_port info\n", port);
        return NULL;
    }

    return &(iports[port]);
}

PortInfo* DfpObject::output_port(int port){
    
    if(oports == NULL) return NULL;
    
    if(port < 0 || port >= meta.num_outports){
        printf("Invalid port %d given to get_output_port_info\n", port);
        return NULL;
    }

    // FYI: this calls the default copy constructors for strings, vectors, etc.
    return &(oports[port]);
}

int DfpObject::get_all_input_port_info(PortInfo *dstv){
    if(iports == NULL) return -1;
    for(int i=0; i < meta.num_inports; i++){
        dstv[i] = iports[i];
    }
    return 0;
}

int DfpObject::get_all_output_port_info(PortInfo *dstv){
    if(oports == NULL) return -1;
    for(int i=0; i < meta.num_outports; i++){
        dstv[i] = oports[i];
    }
    return 0;
}

std::string DfpObject::path(){
    return src_file_path;
}


// the big cahuna
//--------------------------------------------------

int DfpObject::__load_dfp_file(const char *f){

    FILE* fp = NULL;
    uint64_t sim_data_len;
    PortInfo *port_cfg = NULL;

    src_file_path = std::string(f);

    fp = fopen(f, "rb");

    if(fp == NULL)
        return -1;

    //------------------------------------------------------------------
    // first 8 bytes are DFP version header, OR simulator section byte count
    if(fread(&sim_data_len, sizeof(uint64_t), 1, fp) != 1) goto error;


    //------------------------------------------------------------------
    // DFP v5
    if(sim_data_len == 5){
        meta.dfp_version_str = "5";
        meta.dfp_version = 5;

        // get the actual data length
        if(fread(&sim_data_len, sizeof(uint64_t), 1, fp) != 1) goto error;

        // get compile date/time
        uint8_t dateleng = 0;
        if(fread(&dateleng, sizeof(uint8_t), 1, fp) != 1) goto error;
        char *datestr = new char[dateleng+1];
        datestr[dateleng] = '\0';
        if(fread(datestr, dateleng*sizeof(char), 1, fp) != 1) goto error;
        meta.compile_time = std::string(datestr);
        delete [] datestr;

        // skip over model names
        uint32_t modelinfoleng = 0;
        if(fread(&modelinfoleng, sizeof(uint32_t), 1, fp) != 1) goto error;
        if(fseek(fp, modelinfoleng, SEEK_CUR) != 0) goto error;

        // get compiler version
        uint8_t compilerverleng = 0;
        if(fread(&compilerverleng, sizeof(uint8_t), 1, fp) != 1) goto error;
        char *verstr = new char[compilerverleng+1];
        verstr[compilerverleng] = '\0';
        if(fread(verstr, compilerverleng*sizeof(char), 1, fp) != 1) goto error;
        meta.compiler_version = std::string(verstr);
        delete [] verstr;

        // skip over compiler argument list
        uint32_t argsleng = 0;
        if(fread(&argsleng, sizeof(uint32_t), 1, fp) != 1) goto error;
        if(fseek(fp, argsleng, SEEK_CUR) != 0) goto error;

        // get gen_and_towers
        uint8_t gen_and_towers = 0;
        if(fread(&gen_and_towers, sizeof(uint8_t), 1, fp) != 1) goto error;
        switch( (gen_and_towers & 0x0F) ){
            case 4: {
                        meta.mxa_gen = 3.1f;
                        meta.mxa_gen_name = "Cascade+";
                        break;
                    };
            case 3: {
                        meta.mxa_gen = 3.0f;
                        meta.mxa_gen_name = "Cascade";
                        break;
                    };
            case 2: {
                        meta.mxa_gen = 2.0f;
                        meta.mxa_gen_name = "Barton";
                        break;
                    };
            default: goto error;
        }

        // get number of MXAs
        uint8_t num_chips = 0;
        if(fread(&num_chips, sizeof(uint8_t), 1, fp) != 1) goto error;
        meta.num_chips = num_chips;

        // skip over 2 bytes of frequency info
        if(fseek(fp, 2, SEEK_CUR) != 0) goto error;

        // get the number of inport/outport
        uint8_t inports, outports;
        if(fread(&inports, sizeof(uint8_t), 1, fp) != 1) goto error;
        if(fread(&outports, sizeof(uint8_t), 1, fp) != 1) goto error;
        meta.num_inports = inports;
        meta.num_outports = outports;

        iports = new PortInfo[inports];
        oports = new PortInfo[outports];


        uint8_t index_and_status;
        uint16_t layernameleng = 0;

        // INPORTS
        // =============================================================================
        meta.num_used_inports = 0;
        for(uint8_t i=0; i < inports; i++){
            port_cfg = &(iports[i]);

            if(fread(&index_and_status, sizeof(uint8_t), 1, fp) != 1) goto error;

            // ACTIVE inport
            if((index_and_status & 0x80) == 0x80){

                port_cfg->port = index_and_status & 0x7F;
                port_cfg->active = 1;

                // port_set
                if(fread(&(port_cfg->port_set), sizeof(uint8_t), 1, fp) != 1) goto error;

                // mpu_id
                if(fread(&(port_cfg->mpu_id), sizeof(uint8_t), 1, fp) != 1) goto error;

                // model_index
                if(fread(&(port_cfg->model_index), sizeof(uint8_t), 1, fp) != 1) goto error;

                // layer name length, and
                // skip the name string because we don't care
                if(fread(&layernameleng, sizeof(uint16_t), 1, fp) != 1) goto error;
                if(fseek(fp, layernameleng, SEEK_CUR) != 0) goto error;

                // format
                if(fread(&(port_cfg->format), sizeof(uint8_t), 1, fp) != 1) goto error;

                // data range enabled
                if(fread(&(port_cfg->range_convert_enabled), sizeof(uint8_t), 1, fp) != 1) goto error;

                // data range shift
                if(fread(&(port_cfg->range_convert_shift), sizeof(float), 1, fp) != 1) goto error;

                // data range scale
                if(fread(&(port_cfg->range_convert_scale), sizeof(float), 1, fp) != 1) goto error;

                // dim_x,y,z,c
                if(fread(&(port_cfg->dim_h), sizeof(uint16_t), 1, fp) != 1) goto error;
                if(fread(&(port_cfg->dim_w), sizeof(uint16_t), 1, fp) != 1) goto error;
                if(fread(&(port_cfg->dim_z), sizeof(uint16_t), 1, fp) != 1) goto error;
                if(fread(&(port_cfg->dim_c), sizeof(uint32_t), 1, fp) != 1) goto error; // full 32b
                port_cfg->total_size = ( port_cfg->dim_h
                                           * port_cfg->dim_w
                                           * port_cfg->dim_z
                                           * port_cfg->dim_c );

                meta.num_used_inports += 1;

            } else {
                // INACTIVE inports skipped
                port_cfg->port = index_and_status & 0x7F;
                port_cfg->active = 0;
            }
        } // for inports

        // OUTPORTS
        // =============================================================================
        meta.num_used_outports = 0;
        uint8_t hpoc_enabled = 0;
        uint16_t hpoc_list_length = 0;
        // now we're at the outport data
        for(uint8_t i=0; i < outports; i++){
            port_cfg = &(oports[i]);

            if(fread(&index_and_status, sizeof(uint8_t), 1, fp) != 1) goto error;

            // ACTIVE outport
            if((index_and_status & 0x80) == 0x80){

                port_cfg->port = index_and_status & 0x7F;
                port_cfg->active = 1;

                // port_set
                if(fread(&(port_cfg->port_set), sizeof(uint8_t), 1, fp) != 1) goto error;

                // mpu_id
                if(fread(&(port_cfg->mpu_id), sizeof(uint8_t), 1, fp) != 1) goto error;

                // model_index
                if(fread(&(port_cfg->model_index), sizeof(uint8_t), 1, fp) != 1) goto error;

                // get layer name leng and skip it
                if(fread(&layernameleng, sizeof(uint16_t), 1, fp) != 1) goto error;
                if(fseek(fp, layernameleng, SEEK_CUR) != 0) goto error;

                // format
                if(fread(&(port_cfg->format), sizeof(uint8_t), 1, fp) != 1) goto error;

                // dim_x,y,z,c
                if(fread(&(port_cfg->dim_h), sizeof(uint16_t), 1, fp) != 1) goto error;
                if(fread(&(port_cfg->dim_w), sizeof(uint16_t), 1, fp) != 1) goto error;
                if(fread(&(port_cfg->dim_z), sizeof(uint16_t), 1, fp) != 1) goto error;
                if(fread(&(port_cfg->dim_c), sizeof(uint32_t), 1, fp) != 1) goto error; // full 32b
                port_cfg->total_size = ( port_cfg->dim_h
                                           * port_cfg->dim_w
                                           * port_cfg->dim_z
                                           * port_cfg->dim_c );

                // HPOC info
                if(fread(&hpoc_enabled, sizeof(uint8_t), 1, fp) != 1) goto error;
                port_cfg->hpoc_en = hpoc_enabled;
                if(hpoc_enabled == 1){
                    // skip hpoc_dim_x,y,z
                    if(fseek(fp, 6, SEEK_CUR) != 0) goto error;
                    // get hpoc_dim_c
                    if(fread(&(port_cfg->hpoc_dim_c), sizeof(uint32_t), 1, fp) != 1) goto error;

                    // get dummy channel list leng
                    if(fread(&hpoc_list_length, sizeof(uint16_t), 1, fp) != 1) goto error;
                    port_cfg->hpoc_list_length = hpoc_list_length;
                    port_cfg->hpoc_dummy_channels = new uint16_t[hpoc_list_length];

                    uint16_t temp_ch = 0;
                    for (int z = 0; z < hpoc_list_length; z++){
                        if (fread(&temp_ch, sizeof(uint16_t), 1, fp) != 1) { goto error; }
                        (port_cfg->hpoc_dummy_channels)[z] = temp_ch;
                    }
                } else {
                    port_cfg->hpoc_dummy_channels = NULL;
                }

                meta.num_used_outports += 1;
            }
            // INACTIVE outport
            else {
                port_cfg->port = index_and_status & 0x7F;
                port_cfg->active = 0;
                port_cfg->hpoc_en = 0;
                port_cfg->hpoc_list_length = 0;
                port_cfg->hpoc_dummy_channels = NULL;
                port_cfg->hpoc_dim_c = 0;
            }
        } // for outports
    }

    //------------------------------------------------------------------
    // DFP v4
    else if(sim_data_len == 4){
        meta.dfp_version_str = "4";
        meta.dfp_version = 4;

        // get the actual data length
        if(fread(&sim_data_len, sizeof(uint64_t), 1, fp) != 1) goto error;

        // get compile date/time
        uint8_t dateleng = 0;
        if(fread(&dateleng, sizeof(uint8_t), 1, fp) != 1) goto error;
        char *datestr = new char[dateleng+1];
        datestr[dateleng] = '\0';
        if(fread(datestr, dateleng*sizeof(char), 1, fp) != 1) goto error;
        meta.compile_time = std::string(datestr);
        delete [] datestr;

        // skip over model names
        uint32_t modelinfoleng = 0;
        if(fread(&modelinfoleng, sizeof(uint32_t), 1, fp) != 1) goto error;
        if(fseek(fp, modelinfoleng, SEEK_CUR) != 0) goto error;

        // v4 doesn't have compiler version info
        meta.compiler_version = "< unknown >";

        // get gen_and_towers
        uint8_t gen_and_towers = 0;
        if(fread(&gen_and_towers, sizeof(uint8_t), 1, fp) != 1) goto error;
        switch( (gen_and_towers & 0x0F) ){
            case 4: {
                        meta.mxa_gen = 3.1f;
                        meta.mxa_gen_name = "Cascade+";
                        break;
                    };
            case 3: {
                        meta.mxa_gen = 3.0f;
                        meta.mxa_gen_name = "Cascade";
                        break;
                    };
            case 2: {
                        meta.mxa_gen = 2.0f;
                        meta.mxa_gen_name = "Barton";
                        break;
                    };
            default: goto error;
        }

        // get number of MXAs
        uint8_t num_chips = 0;
        if(fread(&num_chips, sizeof(uint8_t), 1, fp) != 1) goto error;
        meta.num_chips = num_chips;

        // skip over 2 bytes of frequency info
        if(fseek(fp, 2, SEEK_CUR) != 0) goto error;

        // get the number of inport/outport
        uint8_t inports, outports;
        if(fread(&inports, sizeof(uint8_t), 1, fp) != 1) goto error;
        if(fread(&outports, sizeof(uint8_t), 1, fp) != 1) goto error;
        meta.num_inports = inports;
        meta.num_outports = outports;

        iports = new PortInfo[inports];
        oports = new PortInfo[outports];


        uint8_t index_and_status;
        uint16_t layernameleng = 0;

        // INPORTS
        // =============================================================================
        meta.num_used_inports = 0;
        for(uint8_t i=0; i < inports; i++){
            port_cfg = &(iports[i]);

            if(fread(&index_and_status, sizeof(uint8_t), 1, fp) != 1) goto error;

            // ACTIVE inport
            if((index_and_status & 0x80) == 0x80){

                port_cfg->port = index_and_status & 0x7F;
                port_cfg->active = 1;

                // port_set
                if(fread(&(port_cfg->port_set), sizeof(uint8_t), 1, fp) != 1) goto error;

                // mpu_id
                if(fread(&(port_cfg->mpu_id), sizeof(uint8_t), 1, fp) != 1) goto error;

                // model_index
                if(fread(&(port_cfg->model_index), sizeof(uint8_t), 1, fp) != 1) goto error;

                // layer name length, and
                // skip the name string because we don't care
                if(fread(&layernameleng, sizeof(uint16_t), 1, fp) != 1) goto error;
                if(fseek(fp, layernameleng, SEEK_CUR) != 0) goto error;

                // format
                if(fread(&(port_cfg->format), sizeof(uint8_t), 1, fp) != 1) goto error;

                // data range enabled
                if(fread(&(port_cfg->range_convert_enabled), sizeof(uint8_t), 1, fp) != 1) goto error;

                // data range shift
                if(fread(&(port_cfg->range_convert_shift), sizeof(float), 1, fp) != 1) goto error;

                // data range scale
                if(fread(&(port_cfg->range_convert_scale), sizeof(float), 1, fp) != 1) goto error;

                // dim_x,y,z,c
                uint16_t dimc_temp = 0;
                if(fread(&(port_cfg->dim_h), sizeof(uint16_t), 1, fp) != 1) goto error;
                if(fread(&(port_cfg->dim_w), sizeof(uint16_t), 1, fp) != 1) goto error;
                if(fread(&(port_cfg->dim_z), sizeof(uint16_t), 1, fp) != 1) goto error;
                if(fread(&dimc_temp, sizeof(uint16_t), 1, fp) != 1) goto error; // just 16b
                port_cfg->dim_c = dimc_temp;
                port_cfg->total_size = ( port_cfg->dim_h
                                           * port_cfg->dim_w
                                           * port_cfg->dim_z
                                           * port_cfg->dim_c );

                meta.num_used_inports += 1;

            } else {
                // INACTIVE inports skipped
                port_cfg->port = index_and_status & 0x7F;
                port_cfg->active = 0;
            }
        } // for inports

        // OUTPORTS
        // =============================================================================
        meta.num_used_outports = 0;
        uint8_t hpoc_enabled = 0;
        uint16_t hpoc_list_length = 0;
        // now we're at the outport data
        for(uint8_t i=0; i < outports; i++){
            port_cfg = &(oports[i]);

            if(fread(&index_and_status, sizeof(uint8_t), 1, fp) != 1) goto error;

            // ACTIVE outport
            if((index_and_status & 0x80) == 0x80){

                port_cfg->port = index_and_status & 0x7F;
                port_cfg->active = 1;

                // port_set
                if(fread(&(port_cfg->port_set), sizeof(uint8_t), 1, fp) != 1) goto error;

                // mpu_id
                if(fread(&(port_cfg->mpu_id), sizeof(uint8_t), 1, fp) != 1) goto error;

                // model_index
                if(fread(&(port_cfg->model_index), sizeof(uint8_t), 1, fp) != 1) goto error;

                // get layer name leng and skip it
                if(fread(&layernameleng, sizeof(uint16_t), 1, fp) != 1) goto error;
                if(fseek(fp, layernameleng, SEEK_CUR) != 0) goto error;

                // format
                if(fread(&(port_cfg->format), sizeof(uint8_t), 1, fp) != 1) goto error;

                // dim_x,y,z,c
                uint16_t dimc_temp = 0;
                if(fread(&(port_cfg->dim_h), sizeof(uint16_t), 1, fp) != 1) goto error;
                if(fread(&(port_cfg->dim_w), sizeof(uint16_t), 1, fp) != 1) goto error;
                if(fread(&(port_cfg->dim_z), sizeof(uint16_t), 1, fp) != 1) goto error;
                if(fread(&dimc_temp, sizeof(uint16_t), 1, fp) != 1) goto error; // just 16b
                port_cfg->dim_c = dimc_temp;
                port_cfg->total_size = ( port_cfg->dim_h
                                           * port_cfg->dim_w
                                           * port_cfg->dim_z
                                           * port_cfg->dim_c );

                // HPOC info
                if(fread(&hpoc_enabled, sizeof(uint8_t), 1, fp) != 1) goto error;
                port_cfg->hpoc_en = hpoc_enabled;
                if(hpoc_enabled == 1){

                    // get dummy channel list leng
                    if(fread(&hpoc_list_length, sizeof(uint16_t), 1, fp) != 1) goto error;
                    port_cfg->hpoc_list_length = hpoc_list_length;
                    port_cfg->hpoc_dummy_channels = new uint16_t[hpoc_list_length];

                    uint16_t temp_ch = 0;
                    for (int z = 0; z < hpoc_list_length; z++){
                        if (fread(&temp_ch, sizeof(uint16_t), 1, fp) != 1) { goto error; }
                        (port_cfg->hpoc_dummy_channels)[z] = temp_ch;
                    }
                } else {
                    port_cfg->hpoc_dummy_channels = NULL;
                }

                meta.num_used_outports += 1;
            }
            // INACTIVE outport
            else {
                port_cfg->port = index_and_status & 0x7F;
                port_cfg->active = 0;
                port_cfg->hpoc_en = 0;
                port_cfg->hpoc_list_length = 0;
                port_cfg->hpoc_dummy_channels = NULL;
            }
        } // for outports

    }
        
    //------------------------------------------------------------------
    // DFP v3
    else if(sim_data_len == 3){
        meta.dfp_version_str = "3";
        meta.dfp_version = 3;

        // get the actual data length
        if(fread(&sim_data_len, sizeof(uint64_t), 1, fp) != 1) goto error;

        // v3 doesn't have compiler version info nor compile time
        meta.compiler_version = "< unknown >";
        meta.compile_time = "< unknown >";

        // get gen_and_towers
        uint8_t gen_and_towers = 0;
        if(fread(&gen_and_towers, sizeof(uint8_t), 1, fp) != 1) goto error;
        switch( (gen_and_towers & 0x0F) ){
            case 4: {
                        meta.mxa_gen = 3.1f;
                        meta.mxa_gen_name = "Cascade+";
                        break;
                    };
            case 3: {
                        meta.mxa_gen = 3.0f;
                        meta.mxa_gen_name = "Cascade";
                        break;
                    };
            case 2: {
                        meta.mxa_gen = 2.0f;
                        meta.mxa_gen_name = "Barton";
                        break;
                    };
            default: goto error;
        }

        // get number of MXAs
        uint8_t num_chips = 0;
        if(fread(&num_chips, sizeof(uint8_t), 1, fp) != 1) goto error;
        meta.num_chips = num_chips;

        // skip over 2 bytes of frequency info
        if(fseek(fp, 2, SEEK_CUR) != 0) goto error;

        // get the number of inport/outport
        uint8_t inports, outports;
        if(fread(&inports, sizeof(uint8_t), 1, fp) != 1) goto error;
        if(fread(&outports, sizeof(uint8_t), 1, fp) != 1) goto error;
        meta.num_inports = inports;
        meta.num_outports = outports;

        iports = new PortInfo[inports];
        oports = new PortInfo[outports];


        uint8_t index_and_status;

        // INPORTS
        // =============================================================================
        meta.num_used_inports = 0;
        for(uint8_t i=0; i < inports; i++){
            port_cfg = &(iports[i]);

            if(fread(&index_and_status, sizeof(uint8_t), 1, fp) != 1) goto error;

            // ACTIVE inport
            if((index_and_status & 0x80) == 0x80){

                port_cfg->port = index_and_status & 0x7F;
                port_cfg->active = 1;

                // port_set
                if(fread(&(port_cfg->port_set), sizeof(uint8_t), 1, fp) != 1) goto error;

                // format
                if(fread(&(port_cfg->format), sizeof(uint8_t), 1, fp) != 1) goto error;

                // data range enabled
                if(fread(&(port_cfg->range_convert_enabled), sizeof(uint8_t), 1, fp) != 1) goto error;

                // data range shift
                if(fread(&(port_cfg->range_convert_shift), sizeof(float), 1, fp) != 1) goto error;

                // data range scale
                if(fread(&(port_cfg->range_convert_scale), sizeof(float), 1, fp) != 1) goto error;

                // dim_x,y,z,c
                uint16_t dimc_temp = 0;
                if(fread(&(port_cfg->dim_h), sizeof(uint16_t), 1, fp) != 1) goto error;
                if(fread(&(port_cfg->dim_w), sizeof(uint16_t), 1, fp) != 1) goto error;
                if(fread(&(port_cfg->dim_z), sizeof(uint16_t), 1, fp) != 1) goto error;
                if(fread(&dimc_temp, sizeof(uint16_t), 1, fp) != 1) goto error; // just 16b
                port_cfg->dim_c = dimc_temp;
                port_cfg->total_size = ( port_cfg->dim_h
                                           * port_cfg->dim_w
                                           * port_cfg->dim_z
                                           * port_cfg->dim_c );

                meta.num_used_inports += 1;

            } else {
                // INACTIVE inports skipped
                port_cfg->port = index_and_status & 0x7F;
                port_cfg->active = 0;
            }
        } // for inports

        // OUTPORTS
        // =============================================================================
        meta.num_used_outports = 0;
        uint8_t hpoc_enabled = 0;
        uint16_t hpoc_list_length = 0;
        // now we're at the outport data
        for(uint8_t i=0; i < outports; i++){
            port_cfg = &(oports[i]);

            if(fread(&index_and_status, sizeof(uint8_t), 1, fp) != 1) goto error;

            // ACTIVE outport
            if((index_and_status & 0x80) == 0x80){

                port_cfg->port = index_and_status & 0x7F;
                port_cfg->active = 1;

                // port_set
                if(fread(&(port_cfg->port_set), sizeof(uint8_t), 1, fp) != 1) goto error;

                // format
                if(fread(&(port_cfg->format), sizeof(uint8_t), 1, fp) != 1) goto error;

                // dim_x,y,z,c
                uint16_t dimc_temp = 0;
                if(fread(&(port_cfg->dim_h), sizeof(uint16_t), 1, fp) != 1) goto error;
                if(fread(&(port_cfg->dim_w), sizeof(uint16_t), 1, fp) != 1) goto error;
                if(fread(&(port_cfg->dim_z), sizeof(uint16_t), 1, fp) != 1) goto error;
                if(fread(&dimc_temp, sizeof(uint16_t), 1, fp) != 1) goto error; // just 16b
                port_cfg->dim_c = dimc_temp;
                port_cfg->total_size = ( port_cfg->dim_h
                                           * port_cfg->dim_w
                                           * port_cfg->dim_z
                                           * port_cfg->dim_c );

                // HPOC info
                if(fread(&hpoc_enabled, sizeof(uint8_t), 1, fp) != 1) goto error;
                port_cfg->hpoc_en = hpoc_enabled;
                if(hpoc_enabled == 1){

                    // get dummy channel list leng
                    if(fread(&hpoc_list_length, sizeof(uint16_t), 1, fp) != 1) goto error;
                    port_cfg->hpoc_list_length = hpoc_list_length;
                    port_cfg->hpoc_dummy_channels = new uint16_t[hpoc_list_length];

                    uint16_t temp_ch = 0;
                    for (int z = 0; z < hpoc_list_length; z++){
                        if (fread(&temp_ch, sizeof(uint16_t), 1, fp) != 1) { goto error; }
                        (port_cfg->hpoc_dummy_channels)[z] = temp_ch;
                    }
                } else {
                    port_cfg->hpoc_dummy_channels = NULL;
                }

                meta.num_used_outports += 1;
            }
            // INACTIVE outport
            else {
                port_cfg->port = index_and_status & 0x7F;
                port_cfg->active = 0;
                port_cfg->hpoc_en = 0;
                port_cfg->hpoc_list_length = 0;
                port_cfg->hpoc_dummy_channels = NULL;
            }
        } // for outports

    }
    

    //------------------------------------------------------------------
    // legacy format
    else if(sim_data_len > 20){
        meta.dfp_version_str = "legacy";
        meta.dfp_version = -1;

        // legacy doesn't have compiler version info nor compile time
        meta.compiler_version = "< unknown >";
        meta.compile_time = "< unknown >";

        // get gen_and_towers
        uint8_t gen_and_towers = 0;
        if(fread(&gen_and_towers, sizeof(uint8_t), 1, fp) != 1) goto error;
        switch( (gen_and_towers & 0x0F) ){
            case 4: {
                        meta.mxa_gen = 3.1f;
                        meta.mxa_gen_name = "Cascade+";
                        break;
                    };
            case 3: {
                        meta.mxa_gen = 3.0f;
                        meta.mxa_gen_name = "Cascade";
                        break;
                    };
            case 2: {
                        meta.mxa_gen = 2.0f;
                        meta.mxa_gen_name = "Barton";
                        break;
                    };
            default: goto error;
        }

        // get number of MXAs
        uint8_t num_chips = 0;
        if(fread(&num_chips, sizeof(uint8_t), 1, fp) != 1) goto error;
        meta.num_chips = num_chips;

        // skip over 2 bytes of frequency info
        if(fseek(fp, 2, SEEK_CUR) != 0) goto error;

        // get the number of inport/outport
        uint8_t inports, outports;
        if(fread(&inports, sizeof(uint8_t), 1, fp) != 1) goto error;
        if(fread(&outports, sizeof(uint8_t), 1, fp) != 1) goto error;
        meta.num_inports = inports;
        meta.num_outports = outports;

        iports = new PortInfo[inports];
        oports = new PortInfo[outports];


        uint8_t index_and_status;

        // INPORTS
        // =============================================================================
        meta.num_used_inports = 0;
        for(uint8_t i=0; i < inports; i++){
            port_cfg = &(iports[i]);

            if(fread(&index_and_status, sizeof(uint8_t), 1, fp) != 1) goto error;

            port_cfg->port = index_and_status & 0x7F;
            port_cfg->active = ((index_and_status & 0x80) == 0x80) ? 1 : 0;

            // port_set
            if(fread(&(port_cfg->port_set), sizeof(uint8_t), 1, fp) != 1) goto error;

            // format
            if(fread(&(port_cfg->format), sizeof(uint8_t), 1, fp) != 1) goto error;

            // data range enabled
            if(fread(&(port_cfg->range_convert_enabled), sizeof(uint8_t), 1, fp) != 1) goto error;

            // data range shift
            if(fread(&(port_cfg->range_convert_shift), sizeof(float), 1, fp) != 1) goto error;

            // data range scale
            if(fread(&(port_cfg->range_convert_scale), sizeof(float), 1, fp) != 1) goto error;

            // dim_x,y,z,c
            uint16_t dimc_temp = 0;
            if(fread(&(port_cfg->dim_h), sizeof(uint16_t), 1, fp) != 1) goto error;
            if(fread(&(port_cfg->dim_w), sizeof(uint16_t), 1, fp) != 1) goto error;
            if(fread(&(port_cfg->dim_z), sizeof(uint16_t), 1, fp) != 1) goto error;
            if(fread(&dimc_temp, sizeof(uint16_t), 1, fp) != 1) goto error; // just 16b
            port_cfg->dim_c = dimc_temp;
            port_cfg->total_size = ( port_cfg->dim_h
                                       * port_cfg->dim_w
                                       * port_cfg->dim_z
                                       * port_cfg->dim_c );

            if(port_cfg->active == 1)
                meta.num_used_inports += 1;

        } // for inports
        
        // OUTPORTS
        // =============================================================================
        meta.num_used_outports = 0;
        // now we're at the outport data
        for(uint8_t i=0; i < outports; i++){
            port_cfg = &(oports[i]);

            if(fread(&index_and_status, sizeof(uint8_t), 1, fp) != 1) goto error;

            port_cfg->port = index_and_status & 0x7F;
            port_cfg->active = ((index_and_status & 0x80) == 0x80) ? 1 : 0;

            port_cfg->port_set = 0;

            // format
            if(fread(&(port_cfg->format), sizeof(uint8_t), 1, fp) != 1) goto error;

            // dim_x,y,z,c
            uint16_t dimc_temp = 0;
            if(fread(&(port_cfg->dim_h), sizeof(uint16_t), 1, fp) != 1) goto error;
            if(fread(&(port_cfg->dim_w), sizeof(uint16_t), 1, fp) != 1) goto error;
            if(fread(&(port_cfg->dim_z), sizeof(uint16_t), 1, fp) != 1) goto error;
            if(fread(&dimc_temp, sizeof(uint16_t), 1, fp) != 1) goto error; // just 16b
            port_cfg->dim_c = dimc_temp;
            port_cfg->total_size = ( port_cfg->dim_h
                                       * port_cfg->dim_w
                                       * port_cfg->dim_z
                                       * port_cfg->dim_c );

            port_cfg->hpoc_en = 0;
            port_cfg->hpoc_dummy_channels = NULL;

            if(port_cfg->active == 1)
                meta.num_used_outports += 1;

        } // for outports

    } // dfp version checks


    fclose(fp);

    return 0;

error:
    if (fp != NULL) {
        fclose(fp);
        fp = NULL;
    }

    return -1;
}
