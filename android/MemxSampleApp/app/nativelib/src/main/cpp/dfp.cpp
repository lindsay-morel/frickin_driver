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
#include <cassert>
#include <iostream>
#include "dfp.h"
#include "mxpack.h"

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

// ctor using bytes
DfpObject::DfpObject(const uint8_t *b){
    src_dfp_bytes = NULL;
    iports = NULL;
    oports = NULL;
    if(__load_dfp_bytes(b) != 0){
        printf("Failed to load dfp from bytes\n");
        valid = false;
        if(iports != NULL){
            for(int i=0; i < meta.num_inports; i++){
                if(iports[i].layer_name != NULL){
                    delete [] iports[i].layer_name;
                    iports[i].layer_name = NULL;
                }
            }
            delete [] iports;
            iports = NULL;
        }
        if(oports != NULL){
            for(int i=0; i < meta.num_outports; i++){
                if(oports[i].hpoc_dummy_channels != NULL){
                    delete [] oports[i].hpoc_dummy_channels;
                    oports[i].hpoc_dummy_channels = NULL;
                }
                if(oports[i].layer_name != NULL){
                    delete [] oports[i].layer_name;
                    oports[i].layer_name = NULL;
                }
            }
            delete [] oports;
            oports = NULL;
        }
    } else {
        valid = true;
    };
}

// ctor using c string
DfpObject::DfpObject(const char *f){
    src_dfp_bytes = NULL;
    iports = NULL;
    oports = NULL;
    if(__load_dfp_file(f) != 0){
        printf("Failed to load dfp file %s\n", f);
        valid = false;
        if(iports != NULL){
            for(int i=0; i < meta.num_inports; i++){
                if(iports[i].layer_name != NULL){
                    delete [] iports[i].layer_name;
                    iports[i].layer_name = NULL;
                }
            }
            delete [] iports;
            iports = NULL;
        }
        if(oports != NULL){
            for(int i=0; i < meta.num_outports; i++){
                if(oports[i].hpoc_dummy_channels != NULL){
                    delete [] oports[i].hpoc_dummy_channels;
                    oports[i].hpoc_dummy_channels = NULL;
                }
                if(oports[i].layer_name != NULL){
                    delete [] oports[i].layer_name;
                    oports[i].layer_name = NULL;
                }
            }
            delete [] oports;
            oports = NULL;
        }
    } else {
        valid = true;
    };
}

// ctor using string
DfpObject::DfpObject(std::string f){
    src_dfp_bytes = NULL;
    iports = NULL;
    oports = NULL;
    if(__load_dfp_file(f.c_str()) != 0){
        printf("Failed to load dfp file %s\n", f.c_str());
        valid = false;
        meta.num_inports = 0;
        meta.num_outports = 0;
        if(iports != NULL){
            for(int i=0; i < meta.num_inports; i++){
                if(iports[i].layer_name != NULL){
                    delete [] iports[i].layer_name;
                    iports[i].layer_name = NULL;
                }
            }
            delete [] iports;
            iports = NULL;
        }
        if(oports != NULL){
            for(int i=0; i < meta.num_outports; i++){
                if(oports[i].hpoc_dummy_channels != NULL){
                    delete [] oports[i].hpoc_dummy_channels;
                    oports[i].hpoc_dummy_channels = NULL;
                }
                if(oports[i].layer_name != NULL){
                    delete [] oports[i].layer_name;
                    oports[i].layer_name = NULL;
                }
            }
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
        for(int i=0; i < meta.num_inports; i++){
            if(iports[i].layer_name != NULL){
                    delete [] iports[i].layer_name;
                    iports[i].layer_name = NULL;
                }
        }
        delete [] iports;
        iports = NULL;
    }
    if(oports != NULL){
        for(int i=0; i < meta.num_outports; i++){
            if(oports[i].hpoc_dummy_channels != NULL){
                delete [] oports[i].hpoc_dummy_channels;
                oports[i].hpoc_dummy_channels = NULL;
            }
            if(oports[i].layer_name != NULL){
                delete [] oports[i].layer_name;
                oports[i].layer_name = NULL;
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
int DfpObject::__load_dfp_bytes(const uint8_t *b){

    size_t offset = 0;
    uint64_t sim_data_len;
    PortInfo *port_cfg = NULL;

    src_dfp_bytes = b;

    //------------------------------------------------------------------
    // first 8 bytes are DFP version header (or legacy #bytes)
    memcpy(&sim_data_len, b+offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);


    //------------------------------------------------------------------
    // DFP v6
    if(sim_data_len == 6){
        
        
        mxpack_list_t *templ = NULL;
        mxpack_dict_t *tempd = NULL;
        mxpack_ascii_t *temps = NULL;
 

        uint8_t dtype;
        memcpy(&dtype, b+offset, 1);
        offset += 1;
        // check for dict as outermost type
        if(dtype != 0x01){
            throw(std::runtime_error("DFPv6 outermost type is not a dict"));
            return -1;
        }

        // init the dict
        mxpack_dict_t d;
        size_t num_pbytes = mxpack_process_dict(&d, b+offset);
        if(num_pbytes == 0){
            throw(std::runtime_error("DFPv6 MXPACK parsing failed"));
            return -1;
        }

        // number of models
        templ = (mxpack_list_t*) mxpack_get_keyval(&d, "models");
        meta.num_models = templ->num_elem; 

        // compile date/time
        temps = (mxpack_ascii_t*) mxpack_get_keyval(&d, "compile_timestamp");
        meta.compile_time = std::string(temps->s);

        // compiler version
        temps = (mxpack_ascii_t*) mxpack_get_keyval(&d, "compiler_version");
        meta.compiler_version = std::string(temps->s);

        // use load-balancing 2x2 config?
        bool *use_2x2_lb = (bool*) mxpack_get_keyval(&d, "use_multigroup_loadbalance");
        if(use_2x2_lb == NULL){
            meta.use_multigroup_lb = false;
        } else {
            meta.use_multigroup_lb = *use_2x2_lb;
        }

        // MXA generation
        tempd = (mxpack_dict_t*) mxpack_get_keyval(&d, "sim_meta");
        uint8_t chip_gen = *( (uint8_t*) mxpack_get_keyval(tempd, "intgen") );
        if(chip_gen == 4){
            meta.mxa_gen = (float)3.1;
            meta.mxa_gen_name = "Cascade+";
        } else {
            mxpack_free_dict(&d);
            throw(std::runtime_error("DFPv6: this DFP is for an unsupported MXA generation"));
            return -1;
        }

        // num MXAs
        meta.num_chips = *( (uint8_t*) mxpack_get_keyval(tempd, "num_mpus") );

        // num ports
        meta.num_inports = *( (uint8_t*) mxpack_get_keyval(&d, "num_inports") );
        meta.num_outports = *( (uint8_t*) mxpack_get_keyval(&d, "num_outports") );

        iports = new PortInfo[meta.num_inports];
        oports = new PortInfo[meta.num_outports];

        mxpack_list_t *pinfol = NULL;
        pinfol = (mxpack_list_t*) mxpack_get_keyval(&d, "inport_info");
        if(pinfol->num_elem != (uint32_t) meta.num_inports){
            mxpack_free_dict(&d);
            throw(std::runtime_error("DFPv6: mismatching num inports and length of inport info list!"));
            return -1;
        }
    
        // INPORTS
        // =============================================================================
        meta.num_used_inports = 0;
        for(uint8_t i=0; i < meta.num_inports; i++){
            port_cfg = &(iports[i]);

            mxpack_dict_t *pd = (mxpack_dict_t*) mxpack_get_list_item_ptr(pinfol, i);
            if(pd == NULL){
                mxpack_free_dict(&d);
                throw(std::runtime_error("DFPv6 inport_info parse error"));
                return -1;
            }

            port_cfg->port = *( (uint8_t*) mxpack_get_keyval(pd, "port"));
            port_cfg->active = *( (uint8_t*) mxpack_get_keyval(pd, "active"));
            if(port_cfg->active){
                // port_set
                port_cfg->port_set = *((uint8_t*) mxpack_get_keyval(pd, "port_set"));
                // mpu_id
                port_cfg->mpu_id = *((uint8_t*) mxpack_get_keyval(pd, "mpu_id"));
                // model_index
                port_cfg->model_index = *((uint8_t*) mxpack_get_keyval(pd, "model_index"));
                // format
                tempd = (mxpack_dict_t*) mxpack_get_keyval(pd, "packing_format");
                port_cfg->format = *((uint8_t*) mxpack_get_keyval(tempd, "as_int"));
                // layer name
                temps = (mxpack_ascii_t*) mxpack_get_keyval(pd, "layer_name");
                port_cfg->layer_name = new char[temps->length + 1];
                memset(port_cfg->layer_name, 0, temps->length + 1);
                strncpy(port_cfg->layer_name, temps->s, temps->length);
                // range conversion stuff    
                tempd = (mxpack_dict_t*) mxpack_get_keyval(pd, "range_convert");
                port_cfg->range_convert_enabled = *((uint8_t*) mxpack_get_keyval(tempd, "enabled"));
                if(port_cfg->range_convert_enabled){
                    port_cfg->range_convert_shift = *((float*) mxpack_get_keyval(tempd, "shift"));
                    port_cfg->range_convert_scale = *((float*) mxpack_get_keyval(tempd, "scale"));
                } else {
                    port_cfg->range_convert_shift = 0.0;
                    port_cfg->range_convert_scale = 1.0;
                }
                // shape
                templ = (mxpack_list_t*) mxpack_get_keyval(pd, "mxa_shape");
                switch(templ->dtype){
                    case MXPACK_UINT8:
                        port_cfg->dim_h = *((uint8_t*) mxpack_get_list_item_ptr(templ, 0));
                        port_cfg->dim_w = *((uint8_t*) mxpack_get_list_item_ptr(templ, 1));
                        port_cfg->dim_z = *((uint8_t*) mxpack_get_list_item_ptr(templ, 2));
                        port_cfg->dim_c = *((uint8_t*) mxpack_get_list_item_ptr(templ, 3));
                        break;
                    case MXPACK_UINT16:
                        port_cfg->dim_h = *((uint16_t*) mxpack_get_list_item_ptr(templ, 0));
                        port_cfg->dim_w = *((uint16_t*) mxpack_get_list_item_ptr(templ, 1));
                        port_cfg->dim_z = *((uint16_t*) mxpack_get_list_item_ptr(templ, 2));
                        port_cfg->dim_c = *((uint16_t*) mxpack_get_list_item_ptr(templ, 3));
                        break;
                    case MXPACK_UINT32:
                        port_cfg->dim_h = (uint16_t) *((uint32_t*) mxpack_get_list_item_ptr(templ, 0));
                        port_cfg->dim_w = (uint16_t) *((uint32_t*) mxpack_get_list_item_ptr(templ, 1));
                        port_cfg->dim_z = (uint16_t) *((uint32_t*) mxpack_get_list_item_ptr(templ, 2));
                        port_cfg->dim_c = *((uint32_t*) mxpack_get_list_item_ptr(templ, 3));
                        break;
                    default:
                        // ERROR
                        mxpack_free_dict(&d);
                        throw(std::runtime_error("DFPv6 inport shape parse error"));
                        return -1;
                }
                
                port_cfg->total_size = ( port_cfg->dim_h
                                           * port_cfg->dim_w
                                           * port_cfg->dim_z
                                           * port_cfg->dim_c );
                meta.num_used_inports += 1;
                while(meta.model_inports.size()<=port_cfg->model_index){
                    std::vector<uint8_t> vect;
                    meta.model_inports.push_back(vect);
                }
                meta.model_inports[port_cfg->model_index].push_back(port_cfg->port);
            }
            else {
                // INACTIVE inports skipped
                port_cfg->layer_name = NULL;
            }

        }
        
        // OUTPORTS
        // =============================================================================
        pinfol = (mxpack_list_t*) mxpack_get_keyval(&d, "outport_info");
        if(pinfol->num_elem != (uint32_t) meta.num_outports){
            mxpack_free_dict(&d);
            throw(std::runtime_error("DFPv6: mismatching num outports and length of outport info list!"));
            return -1;
        }
        
        meta.num_used_outports = 0;
        for(uint8_t i=0; i < meta.num_outports; i++){
            port_cfg = &(oports[i]);

            mxpack_dict_t *pd = (mxpack_dict_t*) mxpack_get_list_item_ptr(pinfol, i);
            if(pd == NULL){
                mxpack_free_dict(&d);
                throw(std::runtime_error("DFPv6 outport_info parse error"));
                return -1;
            }

            port_cfg->port = *( (uint8_t*) mxpack_get_keyval(pd, "port"));
            port_cfg->active = *( (uint8_t*) mxpack_get_keyval(pd, "active"));
            if(port_cfg->active){
                // port_set
                port_cfg->port_set = *((uint8_t*) mxpack_get_keyval(pd, "port_set"));
                // mpu_id
                port_cfg->mpu_id = *((uint8_t*) mxpack_get_keyval(pd, "mpu_id"));
                // model_index
                port_cfg->model_index = *((uint8_t*) mxpack_get_keyval(pd, "model_index"));
                // format
                tempd = (mxpack_dict_t*) mxpack_get_keyval(pd, "packing_format");
                port_cfg->format = *((uint8_t*) mxpack_get_keyval(tempd, "as_int"));
                // layer name
                temps = (mxpack_ascii_t*) mxpack_get_keyval(pd, "layer_name");
                port_cfg->layer_name = new char[temps->length + 1];
                memset(port_cfg->layer_name, 0, temps->length + 1);
                strncpy(port_cfg->layer_name, temps->s, temps->length);
                // shape
                templ = (mxpack_list_t*) mxpack_get_keyval(pd, "mxa_shape");
                switch(templ->dtype){
                    case MXPACK_UINT8:
                        port_cfg->dim_h = *((uint8_t*) mxpack_get_list_item_ptr(templ, 0));
                        port_cfg->dim_w = *((uint8_t*) mxpack_get_list_item_ptr(templ, 1));
                        port_cfg->dim_z = *((uint8_t*) mxpack_get_list_item_ptr(templ, 2));
                        port_cfg->dim_c = *((uint8_t*) mxpack_get_list_item_ptr(templ, 3));
                        break;
                    case MXPACK_UINT16:
                        port_cfg->dim_h = *((uint16_t*) mxpack_get_list_item_ptr(templ, 0));
                        port_cfg->dim_w = *((uint16_t*) mxpack_get_list_item_ptr(templ, 1));
                        port_cfg->dim_z = *((uint16_t*) mxpack_get_list_item_ptr(templ, 2));
                        port_cfg->dim_c = *((uint16_t*) mxpack_get_list_item_ptr(templ, 3));
                        break;
                    case MXPACK_UINT32:
                        port_cfg->dim_h = (uint16_t) *((uint32_t*) mxpack_get_list_item_ptr(templ, 0));
                        port_cfg->dim_w = (uint16_t) *((uint32_t*) mxpack_get_list_item_ptr(templ, 1));
                        port_cfg->dim_z = (uint16_t) *((uint32_t*) mxpack_get_list_item_ptr(templ, 2));
                        port_cfg->dim_c = *((uint32_t*) mxpack_get_list_item_ptr(templ, 3));
                        break;
                    default:
                        // ERROR
                        mxpack_free_dict(&d);
                        throw(std::runtime_error("DFPv6 inport shape parse error"));
                        return -1;
                }
                
                port_cfg->total_size = ( port_cfg->dim_h
                                           * port_cfg->dim_w
                                           * port_cfg->dim_z
                                           * port_cfg->dim_c );

                // HPOC info
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
                            // ERROR
                            mxpack_free_dict(&d);
                            throw(std::runtime_error("DFPv6 HPOC shape parse error"));
                            return -1;
                    }

                    // dummychan list
                    templ = (mxpack_list_t*) mxpack_get_keyval(tempd, "channels");
                    port_cfg->hpoc_list_length = templ->num_elem;
                    port_cfg->hpoc_dummy_channels = (uint16_t*) malloc(templ->num_elem * sizeof(uint16_t));

                    for(unsigned int z=0; z < port_cfg->hpoc_list_length; z++){
                        switch(templ->dtype){
                            case MXPACK_UINT8:
                                port_cfg->hpoc_dummy_channels[z] = *((uint8_t*) mxpack_get_list_item_ptr(templ, z));
                                break;
                            case MXPACK_UINT16:
                                port_cfg->hpoc_dummy_channels[z] = *((uint16_t*) mxpack_get_list_item_ptr(templ, z));
                                break;
                            case MXPACK_UINT32:
                                port_cfg->hpoc_dummy_channels[z] = *((uint32_t*) mxpack_get_list_item_ptr(templ, z));
                                break;
                            default:
                                // ERROR
                                mxpack_free_dict(&d);
                                throw(std::runtime_error("DFPv6 HPOC dummy channel list parse error"));
                                return -1;
                        }
                    }

                } else {
                    // HPOC disabled
                    port_cfg->hpoc_dummy_channels = NULL;
                }
                


                meta.num_used_outports += 1;
                while(meta.model_outports.size()<=port_cfg->model_index){
                    std::vector<uint8_t> vect;
                    meta.model_outports.push_back(vect);
                }
                meta.model_outports[port_cfg->model_index].push_back(port_cfg->port);
            }
            else {
                // INACTIVE outports skipped
                port_cfg->active = 0;
                port_cfg->hpoc_en = 0;
                port_cfg->hpoc_list_length = 0;
                port_cfg->hpoc_dummy_channels = NULL;
                port_cfg->hpoc_dim_c = 0;
                port_cfg->layer_name = NULL;
            }
        }

        // sanity check
        if(meta.model_inports.size() != meta.model_outports.size()){
            mxpack_free_dict(&d);
            throw(std::runtime_error("DFPv6 # of input model_idxs != # of output model_idxs"));
            return -1;
        }

        // clean exit
        mxpack_free_dict(&d);

    }
    //------------------------------------------------------------------
    // DFP v5
    else if(sim_data_len == 5){
        meta.dfp_version_str = "5";
        meta.dfp_version = 5;

        // get the actual data length
        memcpy(&sim_data_len, b+offset, sizeof(uint64_t));
        offset += sizeof(uint64_t);

        // get compile date/time
        uint8_t dateleng = 0;
        memcpy(&dateleng, b+offset, 1);
        offset += 1;
        char *datestr = new char[dateleng+1];
        datestr[dateleng] = '\0';
        memcpy(datestr, b+offset, dateleng*sizeof(char));
        offset += dateleng*sizeof(char);
        meta.compile_time = std::string(datestr);
        delete [] datestr;
        datestr = NULL;

        // skip over model names
        uint32_t modelinfoleng = 0;
        memcpy(&modelinfoleng, b+offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        offset += modelinfoleng;

        // get compiler version
        uint8_t compilerverleng = 0;
        memcpy(&compilerverleng, b+offset, 1);
        offset += 1;
        char *verstr = new char[compilerverleng+1];
        verstr[compilerverleng] = '\0';
        memcpy(verstr, b+offset, compilerverleng*sizeof(char));
        offset += compilerverleng*sizeof(char);
        meta.compiler_version = std::string(verstr);
        delete [] verstr;
        verstr = NULL;

        // DFPv5 doesn't know about 2x2
        meta.use_multigroup_lb = false;

        // skip over compiler argument list
        uint32_t argsleng = 0;
        memcpy(&argsleng, b+offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        offset += argsleng;

        // get gen_and_towers
        uint8_t gen_and_towers = 0;
        memcpy(&gen_and_towers, b+offset, 1);
        offset += 1;
        switch( (gen_and_towers & 0x0F) ){
            case 4: {
                        meta.mxa_gen = (float)3.1;
                        meta.mxa_gen_name = "Cascade+";
                        break;
                    };
            case 3: {
                        meta.mxa_gen = (float)3.0;
                        meta.mxa_gen_name = "Cascade";
                        break;
                    };
            case 2: {
                        meta.mxa_gen = (float)2.0;
                        meta.mxa_gen_name = "Barton";
                        break;
                    };
            default: {
                        throw(std::runtime_error("DFPv5 parsing: invalid chip generation\n"));
                        return -1;
            };
        }

        // get number of MXAs
        uint8_t num_chips = 0;
        memcpy(&num_chips, b+offset, 1);
        offset += 1;
        meta.num_chips = num_chips;

        // skip over 2 bytes of frequency info
        offset += 2;

        // get the number of inport/outport
        uint8_t inports, outports;
        memcpy(&inports, b+offset, 1);
        offset += 1;
        memcpy(&outports, b+offset, 1);
        offset += 1;
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

            memcpy(&index_and_status, b+offset, 1);
            offset += 1;

            // ACTIVE inport
            if((index_and_status & 0x80) == 0x80){

                port_cfg->port = index_and_status & 0x7F;
                port_cfg->active = 1;

                // port_set
                memcpy(&(port_cfg->port_set), b+offset, 1);
                offset += 1;

                // mpu_id
                memcpy(&(port_cfg->mpu_id), b+offset, 1);
                offset += 1;

// model_index
                memcpy(&(port_cfg->model_index), b+offset, 1);
                offset += 1;

                // layer name length
                memcpy(&layernameleng, b+offset, sizeof(uint16_t));
                offset += sizeof(uint16_t);
                
                // Read layer name
                char *temp_layer = new char[layernameleng+1];
                memset(temp_layer, 0, layernameleng+1);
                memcpy(temp_layer, b+offset, layernameleng * sizeof(char));
                offset += layernameleng*sizeof(char);
                temp_layer[layernameleng] = '\0';
                // std::cout<<"temp Layer : "<<temp_layer<<"\n";
                port_cfg->layer_name = new char[layernameleng+1];
                memset(port_cfg->layer_name, 0, layernameleng+1);
                strncpy(port_cfg->layer_name, temp_layer, layernameleng);
                delete [] temp_layer;
                temp_layer = NULL;

                // format
                memcpy(&(port_cfg->format), b+offset, 1);
                offset += 1;

                // data range enabled
                memcpy(&(port_cfg->range_convert_enabled), b+offset, 1);
                offset += 1;

                // data range shift
                memcpy(&(port_cfg->range_convert_shift), b+offset, sizeof(float));
                offset += sizeof(float);

                // data range scale
                memcpy(&(port_cfg->range_convert_scale), b+offset, sizeof(float));
                offset += sizeof(float);

                // dim_h,y,z,c
                memcpy(&(port_cfg->dim_h), b+offset, sizeof(uint16_t));
                offset += sizeof(uint16_t);
                memcpy(&(port_cfg->dim_w), b+offset, sizeof(uint16_t));
                offset += sizeof(uint16_t);
                memcpy(&(port_cfg->dim_z), b+offset, sizeof(uint16_t));
                offset += sizeof(uint16_t);
                memcpy(&(port_cfg->dim_c), b+offset, sizeof(uint32_t));
                offset += sizeof(uint32_t);
                port_cfg->total_size = ( port_cfg->dim_h
                                           * port_cfg->dim_w
                                           * port_cfg->dim_z
                                           * port_cfg->dim_c );

                meta.num_used_inports += 1;
                while(meta.model_inports.size()<=port_cfg->model_index){
                    std::vector<uint8_t> vect;
                    meta.model_inports.push_back(vect);
                }
                meta.model_inports[port_cfg->model_index].push_back(port_cfg->port);

            } else {
                // INACTIVE inports skipped
                port_cfg->port = index_and_status & 0x7F;
                port_cfg->active = 0;
                port_cfg->layer_name = NULL;
            }
        } // for inports
        meta.num_models = (int)meta.model_inports.size();

        // OUTPORTS
        // =============================================================================
        meta.num_used_outports = 0;
        uint8_t hpoc_enabled = 0;
        uint16_t hpoc_list_length = 0;
        // now we're at the outport data
        for(uint8_t i=0; i < outports; i++){
            port_cfg = &(oports[i]);

            memcpy(&index_and_status, b+offset, 1);
            offset += 1;

            // ACTIVE outport
            if((index_and_status & 0x80) == 0x80){

                port_cfg->port = index_and_status & 0x7F;
                port_cfg->active = 1;

                // port_set
                memcpy(&(port_cfg->port_set), b+offset, 1);
                offset += 1;

                // mpu_id
                memcpy(&(port_cfg->mpu_id), b+offset, 1);
                offset += 1;

                // model_index
                memcpy(&(port_cfg->model_index), b+offset, 1);
                offset += 1;

                // layer name length
                memcpy(&layernameleng, b+offset, sizeof(uint16_t));
                offset += sizeof(uint16_t);
                
                // Read layer name
                char *temp_layer = new char[layernameleng+1];
                memset(temp_layer, 0, layernameleng+1);
                memcpy(temp_layer, b+offset, layernameleng * sizeof(char));
                offset += layernameleng*sizeof(char);
                temp_layer[layernameleng] = '\0';
                // std::cout<<"temp Layer : "<<temp_layer<<"\n";
                port_cfg->layer_name = new char[layernameleng+1];
                memset(port_cfg->layer_name, 0, layernameleng+1);
                strncpy(port_cfg->layer_name, temp_layer, layernameleng);
                delete [] temp_layer;
                temp_layer = NULL;

                // format
                memcpy(&(port_cfg->format), b+offset, 1);
                offset += 1;

                // dim_h,y,z,c
                memcpy(&(port_cfg->dim_h), b+offset, sizeof(uint16_t));
                offset += sizeof(uint16_t);
                memcpy(&(port_cfg->dim_w), b+offset, sizeof(uint16_t));
                offset += sizeof(uint16_t);
                memcpy(&(port_cfg->dim_z), b+offset, sizeof(uint16_t));
                offset += sizeof(uint16_t);
                memcpy(&(port_cfg->dim_c), b+offset, sizeof(uint32_t));
                offset += sizeof(uint32_t);
                port_cfg->total_size = ( port_cfg->dim_h
                                           * port_cfg->dim_w
                                           * port_cfg->dim_z
                                           * port_cfg->dim_c );

                // HPOC info
                memcpy(&hpoc_enabled, b+offset, 1);
                offset += 1;
                port_cfg->hpoc_en = hpoc_enabled;
                if(hpoc_enabled == 1){
                    // skip hpoc_dim_h,y,z
                    offset += 6;
                    // get hpoc_dim_c
                    memcpy(&(port_cfg->hpoc_dim_c), b+offset, sizeof(uint32_t));
                    offset += sizeof(uint32_t);

                    // get dummy channel list leng
                    memcpy(&hpoc_list_length, b+offset, sizeof(uint16_t));
                    offset += sizeof(uint16_t);
                    port_cfg->hpoc_list_length = hpoc_list_length;
                    port_cfg->hpoc_dummy_channels = new uint16_t[hpoc_list_length];

                    uint16_t temp_ch = 0;
                    for (int z = 0; z < hpoc_list_length; z++){
                        memcpy(&temp_ch, b+offset, sizeof(uint16_t));
                        offset += sizeof(uint16_t);
                        (port_cfg->hpoc_dummy_channels)[z] = temp_ch;
                    }
                } else {
                    port_cfg->hpoc_dummy_channels = NULL;
                }
                while(meta.model_outports.size()<=port_cfg->model_index){
                    std::vector<uint8_t> vect;
                    meta.model_outports.push_back(vect);
                }
                meta.model_outports[port_cfg->model_index].push_back(port_cfg->port);

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
                port_cfg->layer_name = NULL;
            }
        } // for outports
        if(meta.num_models != static_cast<int>(meta.model_outports.size())){
            throw(std::runtime_error("Num models not equal to model output ports vector size"));
            return -1;
        }

    }
    // invalid file or ancient DFP
    else {
        throw(std::runtime_error("Invalid file or unsupported DFP version"));
        return -1;
    }


    return 0;

}

int DfpObject::__load_dfp_file(const char *f){

    FILE* fp = NULL;
    src_file_path = std::string(f);

    fp = fopen(f, "rb");

    if(fp == NULL)
        return -1;

    // get size and alloc
    fseek(fp, 0L, SEEK_END);
    size_t sz = ftell(fp);

    uint8_t *all = (uint8_t*) malloc(sz);

    // read all the data
    fseek(fp, 0L, SEEK_SET);
    if(!all || fread(all, sz, 1, fp) != 1){
        fclose(fp);
        free(all);
        src_dfp_bytes = NULL;
        return -1;
    }

    // close and call load_dfp_bytes
    fclose(fp);
    fp = NULL;

    int retval = __load_dfp_bytes(all);

    free(all);
    all = NULL;

    src_dfp_bytes = NULL;

    return retval;
}
