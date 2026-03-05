/***************************************************************************//**
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#ifndef MEMX_MPU_COMM_H_
#define MEMX_MPU_COMM_H_

#ifdef __cplusplus
extern "C" {
#endif
#include "memx_dfp.h"
#include "memx_platform.h"
#include "memx_status.h"

#define MEMX_MPU_GROUP (128)
#define MEMX_MPU_MAX_CHIP_ID (128)
#define MEMX_MPU_MAX_CORE_GROUP (6)
#define MEMX_MPU_MAX_FLOW_COUNT (32)

#define MEMX_MPU_TYPE_SINGLE (0) // Single device case
#define MEMX_MPU_TYPE_SPLIT (1)  // Split device(in/out) case

#define MEMX_MPU_CHIP_ID_MASK (0x3f) // only 6 bits chip ID is allowed in cascade_plus

#define MEMX_MPU_DOWNLOAD_MODEL_TYPE_WEIGHT_MEMORY  (1 << 0)
#define MEMX_MPU_DOWNLOAD_MODEL_TYPE_MPU_CONFIG     (1 << 1)
#define MEMX_MPU_DOWNLOAD_MODEL_TYPE_CACHE          (1 << 5)
#define MEMX_MPU_DOWNLOAD_MODEL_TYPE_WEIGHT_LEGACY  (1 << 6)
#define MEMX_MPU_DOWNLOAD_MODEL_TYPE_BUFFER         (1 << 7)

#define MEMX_MPU_IFMAP_FORMAT_FLOAT32_RGB888 (100) // should be configured through 'memx_mpu_set_ifmap_range_convert' only
#define MEMX_MPU_OFMAP_FORMAT_FLOAT32_HPOC (200)

#define MEMX_MPU_GROUP_ID_UNATTTACHED (0xff)

#define MEMX_MPU_RESET_QSPI_M_BIT_POSITION (28)
#define MEMX_MPU_RESET_QSPI_S_BIT_POSITION (29)
#define MEMX_MPU_RESET_QSPI_M_BIT (BIT(MEMX_MPU_RESET_QSPI_M_BIT_POSITION))
#define MEMX_MPU_RESET_QSPI_S_BIT (BIT(MEMX_MPU_RESET_QSPI_S_BIT_POSITION))

typedef enum {
  MEMX_MPU_CHIP_GEN_INVALID      = 0,
  MEMX_MPU_CHIP_GEN_CASCADE_PLUS = 31
} MEMX_MPU_CHIP_GEN;

typedef enum {
  MEMX_MPU_FMAP_SHAPE_INDEX_CHANNEL_NUMBER = 0,
  MEMX_MPU_FMAP_SHAPE_INDEX_WIDTH = 1,
  MEMX_MPU_FMAP_SHAPE_INDEX_HEIGHT = 2,
  MEMX_MPU_FMAP_SHAPE_INDEX_Z = 3,
  MEMX_MPU_FMAP_SHAPE_INDEX_RESERVED
} MEMX_MPU_FMAP_SHAPE_INDEX;

typedef enum MEMX_MPU_IFMAP_FORMAT {
  MEMX_MPU_IFMAP_FORMAT_GBF80 = 0,
  MEMX_MPU_IFMAP_FORMAT_RGB565 = 1,
  MEMX_MPU_IFMAP_FORMAT_RGB888 = 2,
  MEMX_MPU_IFMAP_FORMAT_YUV422 = 3,
  MEMX_MPU_IFMAP_FORMAT_FLOAT16 = 4,
  MEMX_MPU_IFMAP_FORMAT_FLOAT32 = 5,
  MEMX_MPU_IFMAP_FORMAT_GBF80_ROW_PAD = 6,
  MEMX_MPU_IFMAP_FORMAT_RESERVED
} MEMX_MPU_IFMAP_FORMAT;

typedef enum MEMX_MPU_OFMAP_FORMAT {
  MEMX_MPU_OFMAP_FORMAT_GBF80 = 0,
  MEMX_MPU_OFMAP_FORMAT_FLOAT16 = 4,
  MEMX_MPU_OFMAP_FORMAT_FLOAT32 = 5,
  MEMX_MPU_OFMAP_FORMAT_GBF80_ROW_PAD = 6,
  MEMX_MPU_OFMAP_FORMAT_RESERVED
} MEMX_MPU_OFMAP_FORMAT;

/**
 * @brief MPU flow (feature map) shape description of specific flow (port). The
 * shape information should be configurable by user and can be read back.
 */
typedef struct _MemxMpuFmapShap
{
  uint8_t flow_id; // MPU flow (port) ID
  int32_t format; // feature map format which can be one of uint8(raw), float32 or gbf80
  int32_t shape[4]; // feature map shape which is multi-dimensional
} MemxMpuFmapShap;

/**
 * @brief Input feature map description of specific flow (port), which includes
 * both feature map common information and input feature map only information.
 */
typedef struct _MemxMpuCommIfmapInfo
{
  MemxMpuFmapShap fmap; // feature map common information
  uint8_t range_convert_enabled; // FP->RGB using data ranges conversion
  float range_convert_shift; // amount to shift before scale
  float range_convert_scale; // amount to scale before integer cast
} MemxMpuCommIfmapInfo;

/**
 * @brief Output feature map description of specific flow (port), which includes
 * both feature map common information and output feature map only information.
 */
typedef struct _MemxMpuCommOfmapInfo
{
  MemxMpuFmapShap fmap; // feature map common information
  int32_t hpoc_size; // high-precision-output-channel number
  int32_t *hpoc_indexes;  // high-precision-output-channel index array
} MemxMpuCommOfmapInfo;


/**
 * @brief Input or output feature map description, which includes both shape
 * configured by user and shape with format alignment calculated by driver.
 */
typedef struct _MemxMpuIoFmap
{
  int32_t format; // input feature map format
  int32_t channel_number; // input feature map channel number
  int32_t z; // input feature map z
  int32_t width; // input feature map width
  int32_t height; // input feature map height
  int32_t channel_size; // input feature map channel size based on given format
  int32_t row_size; // input feature map row size with no alignment
  int32_t user_frame_size; // input feature map frame size with no alignment
  int32_t fmap_channel_number; // input feature map channel number after format alignment (with dummy channel)
  int32_t fmap_pixel_size; // input feature map pixel size afer format alignment (with padding)
  int32_t fmap_row_size; // input feature map row size after format alignment (with padding)
  int32_t fmap_frame_size; // input feature map frame size after format alignment (with padding)
} MemxMpuIoFmap;

/**
 * @brief Input feature map description, which includes shape configured from
 * user and shape with format alignment calculated by driver.
 */
typedef struct _MemxMpuIfmapInfo
{
  int32_t valid; // flag to indicate if this feature map is configured
  MemxMpuIoFmap fmap; // feature map common information
  float range_convert_shift; // amount to shift before scale
  float range_convert_scale; // amount to scale before integer cast
} MemxMpuIfmapInfo;

// FIXME: we should merge all ofmap related info to format the unify struct,
//        in different chip develop purpose, simple use union to let user fo the select option.
/**
 * @brief Output feature map description, which includes shape configured from
 * user and shape with format alignment calculated by driver.
 */
typedef union _MemxMpuOfmapInfo
{
  struct {
    int32_t valid; // flag to indicate if this feature map is configured
    MemxMpuIoFmap fmap; // feature map common information
    size_t hpoc_is_ofmap_content_include_padding_zero;
    int32_t hpoc_dummy_channel_number; // high-precision-output-channel dummy channel number
    int32_t *hpoc_dummy_channel_indexes; // high-precision-output-channel dummy channel indexes
    int32_t hpoc_dummy_channel_array_size; // high-precision-output-channel info array size
  } Cascade;
  struct {
    int32_t valid; // flag to indicate if this feature map is configured
    MemxMpuIoFmap fmap; // feature map common information
    int32_t hpoc_enabled; // flag to indicate if this feature map is hpoc enabled
    int32_t hpoc_dummy_channel_number; // high-precision-output-channel dummy channel number
    int32_t *hpoc_dummy_channel_indexes; // high-precision-output-channel dummy channel indexes
    int32_t hpoc_dummy_channel_array_size; // high-precision-output-channel info array size
  } CascadePlus;
} MemxMpuOfmapInfo;

/**
 * @brief Input or output feature map row-based ring-buffer, which is used
 * transfer data from 'encoder' to 'ifmap-worker' in egress side and also from
 * 'ofmap-worker' to 'decoder' in ingress side.
 */
typedef union _MemxMpuFmapRingBuffer
{
  struct {
    int32_t valid; // flag indicates if setting of this ring-buffer is valid
    int32_t max_num_of_entry_by_hight; // total entry number, generally should be N times of feature map's row number (height)
    size_t size_of_one_entry_by_row; // each entry size (byte size), should be feature map's row size after alignment
    int32_t entry_r; // read pointer
    int32_t entry_w; // write pointer
    uint8_t *data; // buffer pointer, runtime allocated and should be N times of feature map's frame size after alignment

    platform_thread_cond_t pointer_changed;
    platform_mutex_t guard; // lock for condition change
    platform_mutex_t read_guard; // lock for parallel processing
    platform_mutex_t write_guard; // lock for parallel processing
    int32_t entry_report_r; // special purpose for ofmap worker to record if job has been submitted already
  } Cascade;
  struct {
    uint8_t valid; // flag indicates if setting of this ring-buffer is valid
    size_t max_num_of_frame; // total frame number
    size_t size_of_one_frame; // each frame size (byte size)
    size_t entry_r; // read pointer
    size_t entry_w; // write pointer
    uint8_t *data; // buffer pointer, runtime allocated and should be N times of feature map's frame size after alignment

    platform_thread_cond_t pointer_changed;
    platform_mutex_t guard; // lock for condition change
    platform_mutex_t read_guard; // lock for parallel processing
    platform_mutex_t write_guard; // lock for parallel processing
    size_t transferred_size; // record how much data has been tranferred for each flow
    size_t transferred_row; // record how much row has been tranferred for each flow
    uint8_t transferred_one_frame; // record one frame has been tranferred for each flow
    uint8_t *buf_status;  // record buf status (_mpu_buffer_status_empty/_mpu_buffer_status_full)
    size_t remain_buf_cnt; // record remain buffer count
    size_t read_buf_idx; // record read buf index
  } CascadePlus;
} MemxMpuFmapRingBuffer;

typedef enum _MemxMpuBufferStatus
{
  _mpu_buffer_status_empty = 0,
  _mpu_buffer_status_full = 1,
  _mpu_buffer_status_last
} MemxMpuBufferStatus;

/**
 * @brief cascade_plus input/output background worker state
 */
typedef enum _MemxMpuWorkerState
{
  _mpu_worker_state_pending = 0, // blocks all feature map sending/receiving operations
  _mpu_worker_state_running = 1, // allows all feature map sending/receiving operations
  _mpu_worker_state_last
} MemxMpuWorkerState;

/**
 * @brief Background 'ifmap-worker' and 'ofmap-worker' control information. This
 * structue is the setup information which be passed to worker thread in the
 * beginning, and also the runtime control and status information structure.
 */
struct _MemxMpu;
typedef struct _MemxMpuWorkerInfo
{
  int32_t enable; // by default worker is enabled, set to disable only when program is to be terminated
  struct _MemxMpu *mpu; // top level device (model) descriptor
  MemxMpuWorkerState state; // current worker state, do not modify it manually
  MemxMpuWorkerState state_next; // next worker state, set to 'pending' or 'running' to control worker

  platform_thread_cond_t state_changed; // wake-up or sleep condition
  platform_mutex_t guard; // lock for condition change to prevent event missing
  platform_thread_t worker; // background worker
  int32_t blocking; // blocking worker for new data arrive for better performance
} MemxMpuWorkerInfo;

#define _memx_mpu_worker_go_to_sleep(_worker_info_) \
  do { \
    platform_mutex_lock(&(_worker_info_)->guard); \
    while (((_worker_info_)->enable == 1) && \
           ((_worker_info_)->state_next == _mpu_worker_state_pending)) { \
      (_worker_info_)->state = _mpu_worker_state_pending; \
      platform_cond_wait(&(_worker_info_)->state_changed, &(_worker_info_)->guard); \
    } \
    (_worker_info_)->state = _mpu_worker_state_running; \
    platform_mutex_unlock(&(_worker_info_)->guard); \
  } while(0)

#define _memx_mpu_worker_go_to_state(_worker_info_, _state_) \
  do { \
    platform_mutex_lock(&(_worker_info_)->guard); \
    (_worker_info_)->state_next = (_state_); \
    platform_cond_signal(&(_worker_info_)->state_changed); \
    platform_mutex_unlock(&(_worker_info_)->guard); \
  } while(0)

#define _memx_mpu_worker_wait_state_entered(_worker_info_, _state_) \
  do { \
    while ((_worker_info_)->state != (_state_)) { \
      platform_usleep(1000); \
    } \
  } while(0)

typedef memx_status (*job_action_t)(MemxMpuWorkerInfo* job_worker_info, uint8_t flow_id);
/**
 * @brief Job descriptor that contains handler function pointer and all
 * information required to finish this job.
 */
typedef struct _MemxMpuJob
{
  uint8_t flow_id; // input or output flow ID
  job_action_t action; // job action, function to be executed.
} MemxMpuJob;

/**
 * @brief Job queue which contains queue instance, condition and mutex used to
 * notify data enqueue event.
 */
struct _MemxList;
typedef struct _MemxMpuJobQueue
{
  uint32_t max_number; // maximum number of items limitation
  struct _MemxList *queue; // queue instance
  platform_thread_cond_t enqueue; // enqueue event that some data is being added to queue
  platform_mutex_t guard; // lock for enqueue event
} MemxMpuJobQueue;

/**
 * @brief On-the-air counter used to record number of frames or entries waiting
 * to be processed. Access to this counter needs to be atomic.
 */
typedef struct _MemxMpuOtaCounter
{
  uint32_t value; // counter value
  platform_thread_cond_t value_changed; // value change condition
  platform_mutex_t guard; // lock for value change
} MemxMpuOtaCounter;

#define _memx_mpu_ota_counter_increase(_counter_, _value_adjust_) \
    do { \
        platform_mutex_lock(&(_counter_).guard); \
        (_counter_).value += (_value_adjust_); \
        platform_cond_signal(&(_counter_).value_changed); \
        platform_mutex_unlock(&(_counter_).guard); \
    } while(0)

#define _memx_mpu_ota_counter_decrease(_counter_, _value_adjust_) \
    do { \
        platform_mutex_lock(&(_counter_).guard); \
        (_counter_).value -= (_value_adjust_); \
        platform_mutex_unlock(&(_counter_).guard); \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* MEMX_MPU_COMM_H_ */
