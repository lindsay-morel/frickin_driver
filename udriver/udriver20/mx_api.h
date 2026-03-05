#pragma once

#include <vector>
#include <mutex>
#include <queue>
#include <condition_variable>

#include <string>
#include <stdint.h>

/***************************************************************************//**
 * common
 ******************************************************************************/
#ifndef MEMX_COMMON_H_
#define MEMX_COMMON_H_
#if defined(__MSC_VER) || defined(WIN_EXPORTS) || defined(_WIN32) || defined(_WIN64)
  #define MEMX_API_EXPORT __declspec(dllexport)
  #define MEMX_API_IMPORT __declspec(dllimport)
#elif defined(__GNUC__)
  #define MEMX_API_EXPORT __attribute__((visibility("default")))
  #define MEMX_API_IMPORT
#else
  #define MEMX_API_EXPORT
  #define MEMX_API_IMPORT
#endif
#ifndef unused
#define unused(x) (void)(x)
#endif
#endif /* MEMX_COMMON_H_ */

/*****************************************************************************
    InfAcc : Inference accelerator
******************************************************************************/
#define GROUP_MAX_TENSOR 16

using namespace std;

// Thread-safe queue, blocking-wait
// FIXME, want to hide it from users
template <typename T>
class fifo_queue
{
private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cond;

public:
    size_t size()
    {
        return m_queue.size();
    }

    bool empty()
    {
        return m_queue.empty();
    }
    void push(T item)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_queue.push(item);
        m_cond.notify_one();
    }
    T pop()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond.wait(lock,
                    [this]()
                    { return !m_queue.empty(); });
        T item = m_queue.front();
        m_queue.pop();
        return item;
    }
    fifo_queue &operator=(const fifo_queue &rhs)  // copy assignment
    {
        if (this == &rhs) {
            return *this;
        }
    }
};

/**
 * @brief Namespace that includes all accelerator-related types and classes
 */
namespace MX
{
    /**
     * @brief a handy enum to handle error status
     */
    enum MX_status
    {
        MX_STATUS_OK = 0,
        MX_STATUS_TIMEOUT = 1,
        MX_STATUS_INVALID_DFP,
        MX_STATUS_INVALID_DATA_FMT,
        MX_STATUS_INVALID_CHIP_GEN,
        MX_STATUS_ERR_OPEN_DEV,
        MX_STATUS_ERR_GET_CHIPNUM,
        MX_STATUS_ERR_DFP_MISMATCH_WITH_HARDWARE,
        MX_STATUS_ERR_DOWNLOAD_MODEL,
        MX_STATUS_ERR_ENABLE_STREAM,
        MX_STATUS_END,

        MX_STATUS_ERR_INTERNAL = 1000, // error code >= 1000
    };

    /**
     * @brief input/output tensor information to access memryx accelerator
     */
    struct MxTensorInfo
    {
        uint8_t port_idx;      // one port for one tensor data transfer
        uint16_t h;            // shape height
        uint16_t w;            // shape width
        uint16_t z;            // shape z
        uint32_t c;            // shape channel
        int size;              // total number of elements for this tensor
        int num_bytes;         // number of raw data bytes for this tensor
        string data_type;      // "FLOAT32", "RGB888" ..etc
    };

    /**
     * @brief input/output interface of a given neural network
     */
    struct MxModelInfo
    {
        int num_input = 0;                    // number of input tensors
        int num_output = 0;                   // number of output tensors
        int input_bytes = 0;                  // total bytes of all input tensors
        int output_bytes = 0;                 // total bytes of all output tensors
        vector<MxTensorInfo> in_tensors;      // input tensors info
        vector<MxTensorInfo> out_tensors;     // output tensors info
    };

    /**
     * @brief Metadata about the dfp file.
     */
    struct MxDfpInfo
    {
        string compile_time;            // compilation timestamp
        string compiler_version;        // compiler version string
        float mxa_gen = .0f;            // MXA generation as float (3, 3.1)
        int num_chips = 0;              // number of MXAs this dfp was compiled for
        int num_models = 0;             // number of models compiled in this DFP
        vector<MxModelInfo> models;     // models info
    };

    /**
     * @brief chip generations
     */
    enum eChipGen
    {
        MX_CHIP_CASCADE = 30,
        MX_CHIP_CASCADE_PLUS = 31
    };

    /**
    * @multi group config
    */
    enum multiGroup
    {
        MEMX_MPU_GROUP_CONFIG_ONE_GROUP_FOUR_MPUS = 0,
        MEMX_MPU_GROUP_CONFIG_TWO_GROUP_TWO_MPUS = 1,
        MEMX_MPU_GROUP_CONFIG_OTHERS = 2
    };

    /**
     * @brief necessary arguments for constructor
     */
    struct MxInfSetup
    {
        eChipGen chip_gen;
        multiGroup multi_group;
        // others
    };

    /**
     * @brief interface of a neural network model
     */
    class MxInfModel
    {
    private:
        uint8_t m_group_id; // which device this model belongs
        MxModelInfo m_modelInfo;
        int m_send_timeout; // sending timeout in milliseconds
        int m_recv_timeout; // receiving timeout in milliseconds
        fifo_queue<int> m_queInfTag;

    public:
        MEMX_API_EXPORT MxInfModel(uint8_t group_id, MxModelInfo model_info);
        ~MxInfModel();
        std::mutex m_s_mutex;
        std::mutex m_r_mutex;

        MEMX_API_EXPORT void set_transfer_timeout(int send_timeout, int recv_timeout);

        /**
         * @brief functions to send input tensors to an accelerator, choosing one of them
         */
        MEMX_API_EXPORT MX_status send(void *in_bufs[], int num_bufs);
        MEMX_API_EXPORT MX_status send(void *in_bufs[], int num_bufs, int inf_tag);

        /**
         * @brief functions to receive output tensors to accelerator, choosing one of them
         */
        MEMX_API_EXPORT MX_status receive(void *out_bufs[], int num_bufs);
        MEMX_API_EXPORT MX_status receive(void *out_bufs[], int num_bufs, int &inf_tag);

        MEMX_API_EXPORT MxModelInfo info();
    };

    /**
     * @brief a group is equal to an abstract hardware device
     * it could contain multiple neural netwrok models, depends on the dfp loaded
     */
    class MxAcceleratorGroup
    {
    private:
        uint8_t m_group_id;            // given device id
        uint8_t num_chips;             // number of chips used of current hardware
        MX_status m_status;            // device status
        MxDfpInfo m_dfpInfo;           // loaded dfp infomation to this device
        vector<MxInfModel *> m_models; // one device group may manipulate multiple models

        // functions interally used by constructor
        void init(const char *dfp_path, MxInfSetup setup);

    public:
        /**
         * @brief useful function for users to look into dfp
         */
        MEMX_API_EXPORT static MxDfpInfo inspect_dfp(const char *dfp_path);
        MEMX_API_EXPORT static const char *status_message(MX_status status);

        MEMX_API_EXPORT MxAcceleratorGroup(uint8_t group_id, const char *dfp_path, MxInfSetup setup);
        MEMX_API_EXPORT ~MxAcceleratorGroup();

        MEMX_API_EXPORT MxInfModel *model(int model_id);

        // Below sending and receiving functions do not take care of per-model inference work.
        // user should handle these carefully
        MEMX_API_EXPORT MX_status send_port(uint8_t port_id, void *buf, int timeout);
        MEMX_API_EXPORT MX_status receive_port(uint8_t port_id, void *buf, int timeout);

        /**
         * @brief status and error handling functions
         */
        MEMX_API_EXPORT MX_status status();

        MEMX_API_EXPORT bool status_ok();

        MEMX_API_EXPORT int num_models();
        MEMX_API_EXPORT int num_chips_used();
    };

}
