/***************************************************************************//**
 * @file memx_c_example1.cpp
 * @brief Example 1: basic inference
 *
 * Basic steps to run single model DFP inference.
 *
 * @note
 * Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
 *
 ******************************************************************************/
#include <stdint.h>
#include <stdio.h>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/opencv.hpp>
#include "memx/memx.h"

using namespace cv;

int main(void) {
  const uint8_t model_id = 0; // model 0
  const uint8_t group_id = 0; // MPU device group 0
  const int timeout = 200; // 200 ms
  int argmax = 0; // index with maximum score

  // Assumes input feature map uses only flow 0 as format float32(224,224,3)
  float* ifmap;
  // Assumes output feature map uses only flow 0 as format float32(1,1,1000)
  float ofmap[1*1*1000];

  // 1. Bind MPU device group 0 as MX3:Cascade to model 0.
  memx_status status = memx_open(model_id, group_id, MEMX_DEVICE_CASCADE_PLUS);
  printf(" 1. memx_open = %d\n", status);

  // 2. Download model within DFP file to MPU device group, input and
  // output feature map shape is auto. configured after download complete.
  if (memx_status_no_error(status)) {
    status = memx_download_model(model_id,
      "models/mobilenet_v1.dfp", 0, // model_idx = 0
      MEMX_DOWNLOAD_TYPE_WTMEM_AND_MODEL);
  }
  printf(" 2. memx_download_model = %d\n", status);

  // 3. Enable data transfer of this model to device. Set to no wait here
  // since driver will go to data transfer state eventually.
  if (memx_status_no_error(status)) {
    status = memx_set_stream_enable(model_id, 0);
  }
  printf(" 3. memx_set_stream_enable = %d\n", status);

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // 4. maybe put some input feature map pre-processing here
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  if (memx_status_no_error(status)) {
    Mat img = imread("image.png", IMREAD_COLOR);
    cv::resize(img, img, cv::Size(224,224), 0, 0, CV_INTER_LINEAR);
    img.convertTo(img, CV_32F, 1.0/127.5, -1);
    ifmap = (float*)img.data;
  }
  printf(" 4. pre-processing\n");

  // 5. Stream input feature map to device flow 0 and run inference.
  if (memx_status_no_error(status)) {
    status = memx_stream_ifmap(model_id, 0, ifmap, timeout);
  }
  printf(" 5. memx_stream_ifmap = %d\n", status);

  // 6. Stream output feature map from device flow 0 after inference.
  if (memx_status_no_error(status)) {
    status = memx_stream_ofmap(model_id, 0, ofmap, timeout);
  }
  printf(" 6. memx_stream_ofmap = %d\n", status);

  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // 7. maybe put some output feature map post-processing here
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  if (memx_status_no_error(status)) {
    for (int i = 1; i < 1000; ++i) {
      if (ofmap[argmax] < ofmap[i]) {
        argmax = i;
      }
    }
  }
  printf(" 7. post-processing, argmax = %d\n", argmax);

  // 8. Always remeber to clean-up resources before leaving.
  memx_close(model_id);
  printf(" 8. memx_close = %d\n", status);

  // End of process
  if (memx_status_no_error(status)) {
    printf("success.\n");
  } else {
    printf("failure.\n");
  }
  return 0;
}

