
/***************************************************************************//**
 * @file memx_c_example4.cpp
 * @brief Example 4: Runtime Device Selection
 *
 * Run two different models compiled into different DFP files with runtime
 * weight memory and model re-configuration on two devices with runtime device
 * selection based on which device's lock is acquirable.
 *
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/opencv.hpp>
#include "memx/memx.h"

using namespace cv;

// Model 0: MobileNet v1
#define MODEL_0_ID (0)
// Model 1: MobileNet v2
#define MODEL_1_ID (1)
// MPU device group 0
#define GROUP_0_ID (0)
// MPU device group 1
#define GROUP_1_ID (1)

// Since two models share the same procedure to run inference, we make it a
// common sub-routine with structure of parameters here.
typedef struct  {
  uint8_t model_id; // model ID
  uint8_t group_id; // MPU device group ID
  const char* dfp_path; // DFP file path
  uint8_t iport; // input port ID
  void* ifmap; // input feature map
  uint8_t oport; // output port ID
  void* ofmap; // output feature map
} RunInferenceConfig;

// In order to guarantee only one model can access device in multi-threading
// case, we acquire `device lock` from driver before any hardware configuration.
memx_status run_inference(RunInferenceConfig* config)
{
  const int timeout = 200; // 200 ms

  // 1. Here we use 'trylock()' first to test if device is acquirable, if the
  // return value is '0' means lock is acquired successfully, otherwise we move
  // on to lock another device.
  if (memx_trylock(GROUP_0_ID) == 0) {
    config->group_id = GROUP_0_ID;
  } else {
    memx_lock(GROUP_1_ID); // wait until lock is acquired
    config->group_id = GROUP_1_ID;
  }
  // 2. Re-configure MPU device group binded to model.
  memx_status status = memx_reconfigure(config->model_id, config->group_id);
  printf(" - Model %u is running on device %u\n",
    config->model_id, config->group_id);

  // 3. Download weight memory and model to device.
  if (memx_status_no_error(status)) {
    status = memx_download_model(config->model_id,
      config->dfp_path, 0, MEMX_DOWNLOAD_TYPE_WTMEM_AND_MODEL);
  }
  // 4. Enable data transfer of this model to device.
  if (memx_status_no_error(status)) {
    status = memx_set_stream_enable(config->model_id, 0);
  }
  // 5. Write input feature map to device to run inference
  if (memx_status_no_error(status)) {
    status = memx_stream_ifmap(config->model_id,
      config->iport, config->ifmap, timeout);
  }
  // 6. Read output feature map from device after inference
  if (memx_status_no_error(status)) {
    status = memx_stream_ofmap(config->model_id,
      config->oport, config->ofmap, timeout);
  }
  // 7. Disable data transfer of this model to device.
  if (memx_status_no_error(status)) {
    // wait to stop may take some time, but is safe
    status = memx_set_stream_disable(config->model_id, 1);
  }

  // 7. Always remember to release lock finally.
  memx_unlock(config->group_id);

  return status;
}

// Model 0 run inference, this sub-routine runs in background thread
void* run_inference_model_0(void* arg)
{
  // Assumes input feature map uses only flow 0 as format float32(224,224,3)
  float* ifmap;
  // Assumes output feature map uses only flow 0 as format float32(1,1,1000)
  float ofmap[1*1*1000]; // allocate memory space

  // 1. Pre-process input feature map
  Mat img = imread("image.png", IMREAD_COLOR);
  cv::resize(img, img, cv::Size(224,224), 0, 0, CV_INTER_LINEAR);
  img.convertTo(img, CV_32F, 1.0/127.5, -1);
  ifmap = (float*)img.data;

  // 2. Run inference setup
  RunInferenceConfig config;
  config.model_id = MODEL_0_ID; // model 0
  config.dfp_path = "models/mobilenet_v1.dfp";
  config.iport = 0; // input port 0 (flow 0)
  config.ifmap = ifmap; // input feature map
  config.oport = 0; // output port 0 (flow 0)
  config.ofmap = ofmap; // output feature map

  // 3. Run inference common sub-routine
  memx_status status = run_inference(&config);

  // 4. Post-process output feature map
  if (memx_status_no_error(status)) {
    int argmax = 0;
    for (int i = 1; i < 1000; ++i) {
      argmax = (ofmap[i] > ofmap[argmax]) ? i : argmax;
    }
    printf(" - Model 0 argmax = %d\n", argmax);
  } else {
    printf(" - Model 0 failed to run inference = %d\n", status);
  }

  return NULL;
}

// Model 1 run inference, this sub-routine runs in background thread
void* run_inference_model_1(void* arg)
{
  // Assumes input feature map uses only flow 0 as format float32(224,224,3)
  float* ifmap;
  // Assumes output feature map uses only flow 0 as format float32(1,1,1000)
  float ofmap[1*1*1000]; // allocate memory space

  // 1. Pre-process input feature map
  Mat img = imread("images/siamese_cat.jpg", IMREAD_COLOR);
  cv::resize(img, img, cv::Size(224,224), 0, 0, CV_INTER_LINEAR);
  img.convertTo(img, CV_32F, 1.0/127.5, -1);
  ifmap = (float*)img.data;

  // 2. Run inference setup
  RunInferenceConfig config;
  config.model_id = MODEL_1_ID; // model 1
  config.dfp_path = "models/mobilenet_v2.dfp";
  config.iport = 0; // input port 0 (flow 0)
  config.ifmap = ifmap; // input feature map
  config.oport = 0; // output port 0 (flow 0)
  config.ofmap = ofmap; // output feature map

  // 3. Run inference common sub-routine
  memx_status status = run_inference(&config);

  // 4. Post-process output feature map
  if (memx_status_no_error(status)) {
    int argmax = 0;
    for (int i = 1; i < 1000; ++i) {
      argmax = (ofmap[i] > ofmap[argmax]) ? i : argmax;
    }
    printf(" - Model 1 argmax = %d\n", argmax);
  } else {
    printf(" - Model 1 failed to run inference = %d\n", status);
  }

  return NULL;
}

// Main process, create two threads to run inferences in parallel.
int main(void) {
  memx_status status = MEMX_STATUS_OK;
  pthread_t t0, t1;

  // 1. Bind MPU device group 0 as MX3:Cascade to both model 0 and model 1.
  // Group ID here can be any of MPU device group during initialization, since
  // runtime 'reconfigure()' will also initialize interface as 'open()'.
  if (memx_status_no_error(status)) {
    status = memx_open(MODEL_0_ID, GROUP_0_ID, MEMX_DEVICE_CASCADE_PLUS);
  }
  if (memx_status_no_error(status)) {
    status = memx_open(MODEL_1_ID, GROUP_0_ID, MEMX_DEVICE_CASCADE_PLUS);
  }

  // 2. Run two models simultaneously using posix threads (Linux only)
  if (memx_status_no_error(status)) {
    if ((pthread_create(&t0, NULL, &run_inference_model_0, NULL) != 0)
      ||(pthread_create(&t1, NULL, &run_inference_model_1, NULL) != 0)) {
      status = MEMX_STATUS_OTHERS;
    }
  }
  if (memx_status_no_error(status)) {
    pthread_join(t0, NULL);
    pthread_join(t1, NULL);
  }

  // 3. Always remember to clean-up resources before leaving.
  memx_close(MODEL_0_ID);
  memx_close(MODEL_1_ID);

  // End of process
  if (memx_status_no_error(status)) {
    printf("success.\n");
  } else {
    printf("failure.\n");
  }
  return 0;
}

