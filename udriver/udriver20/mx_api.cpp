#include "mx_api.h"
#include "dfp.h"

#include "memx.h"
#include "memx_status.h"

using namespace MX;

#define MEMX_UDRIVER_STATUS_CODE_BASE 1000

/**
 * @brief table for error status and description
 */
const struct _errordesc
{
    int code;
    char *message;
} errordesc[] = {
    {MX_STATUS_OK, (char*) "Success"},
    {MX_STATUS_TIMEOUT, (char*) "Timeout"},
    {MX_STATUS_INVALID_DFP, (char*) "MxAcceleratorGroup: failed to parse/read DFP, please check the DFP file"},
    {MX_STATUS_INVALID_DATA_FMT, (char*) "Invalid input format (neither FLOAT32 nor RGB888), please check the DFP file"},
    {MX_STATUS_INVALID_CHIP_GEN, (char*) "Invalid chip generation, support MEMX_DEVICE_CASCADE_PLUS only"},
    {MX_STATUS_ERR_OPEN_DEV, (char*) "MxAcceleratorGroup: failed to open device group, please check permission or group value"},
    {MX_STATUS_ERR_GET_CHIPNUM, (char*) "MxAcceleratorGroup: failed to get number of used chips on hardware. (please check the hardware status)"},
    {MX_STATUS_ERR_DFP_MISMATCH_WITH_HARDWARE, (char*) "MxAcceleratorGroup: failed to load dfp"},
    {MX_STATUS_ERR_DOWNLOAD_MODEL, (char*) "MxAcceleratorGroup: failed to load model and weight memory"},
    {MX_STATUS_ERR_ENABLE_STREAM, (char*) "MxAcceleratorGroup: failed to enable stream"},
};

//////////////////////////////
/* class MxAcceleratorGroup */
//////////////////////////////

int MxAcceleratorGroup::num_chips_used()
{
    return num_chips;
}

int MxAcceleratorGroup::num_models()
{
    return m_dfpInfo.num_models;
}

bool MxAcceleratorGroup::status_ok()
{
    return m_status == MX_STATUS_OK;
}

MX_status MxAcceleratorGroup::status()
{
    return m_status;
}

MxDfpInfo MxAcceleratorGroup::inspect_dfp(const char *dfp_path)
{
    MxDfpInfo mxdfp;

    Dfp::DfpObject *dfp = new Dfp::DfpObject(dfp_path);
    Dfp::DfpMeta mdata = dfp->get_dfp_meta();

    // some easy way to detect error, maybe FIXME
    if (mdata.num_used_inports == 0)
    {
        fprintf(stderr, "%s\n", errordesc[MX_STATUS_INVALID_DFP].message);
        exit(MX_STATUS_INVALID_DFP);
    }

    mxdfp.compile_time = mdata.compile_time;
    mxdfp.compiler_version = mdata.compiler_version;
    mxdfp.mxa_gen = mdata.mxa_gen;
    mxdfp.num_chips = mdata.num_chips;

    // scan input ports and convert useful data into 'MxDfpInfo'
    for (int i = 0; i < mdata.num_used_inports; i++)
    {
        Dfp::PortInfo *pi = dfp->input_port(i);

        int mIdx = pi->model_index;
        if ((mIdx + 1) > (int)mxdfp.models.size())
        {
            // encounter new model
            MxModelInfo new_mdl;
            new_mdl.num_input = 0;
            new_mdl.num_output = 0;
            new_mdl.input_bytes = 0;
            new_mdl.output_bytes = 0;
            mxdfp.models.push_back(new_mdl);
        }

        string str_dt = "unknown";
        int num_bytes = 0;
        int size_dim = pi->dim_h * pi->dim_w * pi->dim_z * pi->dim_c;

        switch (pi->format)
        {
        // input port = 0:GBF80, 1:RGB888, 2:RGB565, 3:YUV422, 4:BF16, 5:FP32, 6:YUY2;
        case 0: // GBF80
            num_bytes = size_dim * 4;
            str_dt = "GBF80";
            break;
        case 1: // RGB888
            num_bytes = size_dim * 1;
            str_dt = "RGB888";
            break;
        case 2: // RGB565
            num_bytes = size_dim * 2 / 3;
            str_dt = "RGB565";
            break;
        case 3: // YUV422
            num_bytes = size_dim * 2 / 3;
            str_dt = "YUV422";
            break;
        case 4: // BF16
            num_bytes = size_dim * 2;
            str_dt = "BF16";
            break;
        case 5: // FP32
            num_bytes = size_dim * 4;
            str_dt = "FP32";
            break;
        case 6: // YUY2
            num_bytes = size_dim * 2 / 3;
            str_dt = "YUY2";
            break;

        default:
            fprintf(stderr, "%s\n", errordesc[MX_STATUS_INVALID_DATA_FMT].message);
            exit(MX_STATUS_INVALID_DATA_FMT);
            break;
        }

        MxTensorInfo new_ti = {pi->port, pi->dim_h, pi->dim_w, pi->dim_z, pi->dim_c, size_dim, num_bytes, str_dt};

        mxdfp.models[mIdx].in_tensors.push_back(new_ti);
        mxdfp.models[mIdx].num_input++;
        mxdfp.models[mIdx].input_bytes += num_bytes;
    }

    // scan output ports and convert useful data into 'MxDfpInfo'
    for (int i = 0; i < mdata.num_used_outports; i++)
    {
        Dfp::PortInfo *pi = dfp->output_port(i);

        int mIdx = pi->model_index;

        string str_dt = "unknown";
        int num_bytes = 0;
        int size_dim = pi->dim_h * pi->dim_w * pi->dim_z * pi->dim_c;

        switch (pi->format)
        {
        // output port = 0:GBF80, 4:BF16, 5:FP32
        case 0: // GBF80
            num_bytes = size_dim * 4;
            str_dt = "GBF80";
            break;
        case 4: // BF16
            num_bytes = size_dim * 2;
            str_dt = "BF16";
            break;
        case 5: // FP32
            num_bytes = size_dim * 4;
            str_dt = "FP32";
            break;
        default:
            fprintf(stderr, "%s\n", errordesc[MX_STATUS_INVALID_DATA_FMT].message);
            exit(MX_STATUS_INVALID_DATA_FMT);
            break;
        }

        MxTensorInfo new_ti = {pi->port, pi->dim_h, pi->dim_w, pi->dim_z, pi->dim_c, size_dim, num_bytes, str_dt};

        mxdfp.models[mIdx].out_tensors.push_back(new_ti);
        mxdfp.models[mIdx].num_output++;
        mxdfp.models[mIdx].output_bytes += num_bytes;
    }

    mxdfp.num_models = (int)mxdfp.models.size();

    delete dfp;

    return mxdfp;
}

void MxAcceleratorGroup::init(const char *dfp_path, MxInfSetup setup)
{
    float chip_gen = .0f;
    switch (setup.chip_gen)
    {
    case MX_CHIP_CASCADE_PLUS:
        chip_gen = (float) MEMX_DEVICE_CASCADE_PLUS; // 3.1
        break;
    default:
        throw MX_STATUS_INVALID_CHIP_GEN;
    }

    // open device group
    memx_status sts = memx_open(m_group_id, 0, chip_gen);
    if (sts != MEMX_STATUS_OK)
    {
        throw MX_STATUS_ERR_OPEN_DEV;
    }

    switch (setup.multi_group)
    {
    case MEMX_MPU_GROUP_CONFIG_ONE_GROUP_FOUR_MPUS:
        sts = memx_config_mpu_group(m_group_id, MEMX_MPU_GROUP_CONFIG_ONE_GROUP_FOUR_MPUS);
        break;
    case MEMX_MPU_GROUP_CONFIG_TWO_GROUP_TWO_MPUS:
        sts = memx_config_mpu_group(m_group_id, MEMX_MPU_GROUP_CONFIG_TWO_GROUP_TWO_MPUS);
        break;
    default:
        break;
    }
    if (sts != MEMX_STATUS_OK)
    {
        throw MX_STATUS_ERR_INTERNAL;
    }


    // get MxDfpInfo
    m_dfpInfo = inspect_dfp(dfp_path);

    // get device information (num_chips used)
    sts = memx_operation(m_group_id, 0, &num_chips, 0);
    if (sts != MEMX_STATUS_OK)
    {
        throw MX_STATUS_ERR_GET_CHIPNUM;
    }

    if (setup.multi_group == MEMX_MPU_GROUP_CONFIG_ONE_GROUP_FOUR_MPUS && m_dfpInfo.num_chips != num_chips)
    {
        fprintf(stderr, "This dfp (%s) should not run on a %d-chip hardware as it's compiled with %d-chip.\n",
                dfp_path, num_chips, m_dfpInfo.num_chips);
        throw MX_STATUS_ERR_DFP_MISMATCH_WITH_HARDWARE;
    }
    else if (setup.multi_group == MEMX_MPU_GROUP_CONFIG_TWO_GROUP_TWO_MPUS && m_dfpInfo.num_chips != 2)
    {
        fprintf(stderr, "This dfp (%s) should not run on a 2-chip hardware as it's compiled with %d-chip.\n",
            dfp_path, m_dfpInfo.num_chips);
        throw MX_STATUS_ERR_DFP_MISMATCH_WITH_HARDWARE;
    }

    // load DPF model to chips
    sts = memx_download_model(m_group_id, dfp_path, 0, MEMX_DOWNLOAD_TYPE_WTMEM_AND_MODEL);
    if (sts != MEMX_STATUS_OK)
    {
        throw MX_STATUS_ERR_DOWNLOAD_MODEL;
    }

    m_models.clear();

    for (int i = 0; i < m_dfpInfo.num_models; i++)
    {
        MxInfModel *inf_model = new MxInfModel(m_group_id, m_dfpInfo.models[i]);
        m_models.push_back(inf_model);
    }

    sts = memx_set_stream_enable(m_group_id, 0);
    if (sts != MEMX_STATUS_OK)
    {
        throw MX_STATUS_ERR_ENABLE_STREAM;
    }
}

MxAcceleratorGroup::MxAcceleratorGroup(uint8_t group_id, const char *dfp_path, MxInfSetup setup)
{
    m_group_id = group_id;

    // only return valid object, hide try-catch from users
    try
    {
        init(dfp_path, setup);
        m_status = MX_STATUS_OK;
    }
    catch (MX_status st)
    {
        m_status = st;
        switch (st)
        {
        case MX_STATUS_INVALID_CHIP_GEN:
        case MX_STATUS_ERR_OPEN_DEV:
            break;

        case MX_STATUS_ERR_GET_CHIPNUM:
        case MX_STATUS_ERR_DFP_MISMATCH_WITH_HARDWARE:
        case MX_STATUS_ERR_DOWNLOAD_MODEL:
        case MX_STATUS_ERR_ENABLE_STREAM:
            memx_close(m_group_id);
            break;

        default:
            break;
        }
        fprintf(stderr, "%s\n", status_message(m_status));
    }
}

MxAcceleratorGroup::~MxAcceleratorGroup()
{
    int num_models = (int) m_models.size();

    for (int i = 0; i < num_models; i++)
    {
        delete m_models[i];
    }

    // only call close when status is ok, otherwise handled already
    if (status_ok())
        memx_close(m_group_id);
}

MxInfModel *MxAcceleratorGroup::model(int model_id)
{
    if ((model_id + 1) > (int)m_models.size())
        return NULL;

    return m_models[model_id];
}

MX_status MxAcceleratorGroup::send_port(uint8_t port_id, void *buf, int timeout)
{
    MX_status mx_sts = MX_STATUS_OK;

    memx_status sts = memx_stream_ifmap(m_group_id, port_id, buf, timeout);
    if (sts != MEMX_STATUS_OK)
    {
        if (sts == 0x13000022 /*MEMX_STATUS_MPU_IFMAP_ENQUEUE_TIMEOUT*/) // FIXME: because memx_status.h is not allowed to be included
            mx_sts = MX_STATUS_TIMEOUT;
        else
            // error code refer to memx_status.h
            mx_sts = (MX_status)(MEMX_UDRIVER_STATUS_CODE_BASE + sts);

        fprintf(stderr, "%s\n", status_message(mx_sts));
    }

    return mx_sts;
}

MX_status MxAcceleratorGroup::receive_port(uint8_t port_id, void *buf, int timeout)
{
    MX_status mx_sts = MX_STATUS_OK;

    memx_status sts = memx_stream_ofmap(m_group_id, port_id, buf, timeout);
    if (sts != MEMX_STATUS_OK)
    {
        if (sts == 0x1300001D /*MEMX_STATUS_MPU_OFMAP_DEQUEUE_TIMEOUT*/) // FIXME: because memx_status.h is not allowed to be included
            mx_sts = MX_STATUS_TIMEOUT;
        else
            // error code refer to memx_status.h
            mx_sts = (MX_status)(MEMX_UDRIVER_STATUS_CODE_BASE + sts);

        fprintf(stderr, "%s\n", status_message(mx_sts));
    }

    return mx_sts;
}

const char *MxAcceleratorGroup::status_message(MX_status status)
{
    static char otherMxErr[64];

    if (status < MX_STATUS_END)
    {
        return errordesc[status].message;
    }
    else if (status >= 1000)
    {
        sprintf(otherMxErr, "MxAcceleratorGroup: internal error : %d\n", (int)status);
        return otherMxErr;
    }
    else
    {
        sprintf(otherMxErr, "MxAcceleratorGroup: unknow error code\n");
        return otherMxErr;
    }
}

//////////////////////
/* class MxInfModel */
//////////////////////

MxInfModel::MxInfModel(uint8_t group_id, MxModelInfo model_info)
{
    m_group_id = group_id;
    m_modelInfo = model_info;

    m_send_timeout = 0;
    m_recv_timeout = 0;
}

MxInfModel::~MxInfModel()
{
}

MxModelInfo MxInfModel::info()
{
    return this->m_modelInfo;
}

void MxInfModel::set_transfer_timeout(int send_timeout, int recv_timeout)
{
    m_send_timeout = send_timeout;
    m_recv_timeout = recv_timeout;
}

MX_status MxInfModel::send(void *in_bufs[], int num_bufs)
{
    MX_status mx_sts = MX_STATUS_OK;

    for (int i = 0; i < num_bufs; i++)
    {
        uint8_t port_idx = m_modelInfo.in_tensors[i].port_idx;
        memx_status sts = memx_stream_ifmap(m_group_id, port_idx, in_bufs[i], m_send_timeout);
        if (sts != MEMX_STATUS_OK)
        {
            if (sts == 0x13000022 /*MEMX_STATUS_MPU_IFMAP_ENQUEUE_TIMEOUT*/) // FIXME: because memx_status.h is not allowed to be included
            {
                mx_sts = MX_STATUS_TIMEOUT;
                break;
            }
            else
            {
                // error code refer to memx_status.h
                mx_sts = (MX_status)(MEMX_UDRIVER_STATUS_CODE_BASE + sts);
                break;
            }
            // fprintf(stderr, "%s\n", MxAcceleratorGroup::status_message(mx_sts));
        }
    }

    return mx_sts;
}

MX_status MxInfModel::send(void *in_bufs[], int num_bufs, int inf_tag)
{
    std::lock_guard<std::mutex> lock(this->m_s_mutex);
    m_queInfTag.push(inf_tag);
    return this->send(in_bufs, num_bufs);
}

MX_status MxInfModel::receive(void *out_bufs[], int num_bufs)
{
    MX_status mx_sts = MX_STATUS_OK;

    for (int i = 0; i < num_bufs; i++)
    {
        uint8_t port_idx = m_modelInfo.out_tensors[i].port_idx;
        memx_status sts = memx_stream_ofmap(m_group_id, port_idx, out_bufs[i], m_recv_timeout);
        if (sts != MEMX_STATUS_OK)
        {
            if (sts == 0x1300001D /*MEMX_STATUS_MPU_OFMAP_DEQUEUE_TIMEOUT*/) // FIXME: because memx_status.h is not allowed to be included
            {
                mx_sts = MX_STATUS_TIMEOUT;
                break;
            }
            else
            {
                // error code refer to memx_status.h
                mx_sts = (MX_status)(MEMX_UDRIVER_STATUS_CODE_BASE + sts);
                break;
            }
            // fprintf(stderr, "%s\n", MxAcceleratorGroup::status_message(mx_sts));
        }
    }

    return mx_sts;
}

MX_status MxInfModel::receive(void *out_bufs[], int num_bufs, int &inf_tag)
{
    MX_status sts;

    std::lock_guard<std::mutex> lock(this->m_r_mutex);
    sts = this->receive(out_bufs, num_bufs);
    if (sts != MX_STATUS_OK)
        return sts;

    inf_tag = m_queInfTag.pop();
    return sts;
}
