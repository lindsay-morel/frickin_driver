"""
Copyright (c) 2019-2022 MemryX Inc.
All Rights Reserved.

============
Information:
============
File Name: memx_py_example2.py
Project: MiX-3

============
Description:
============
Example 2: Multiple Models Coexist

========
Authors:
========

"""

import threading
import numpy as np # numpy
import cv2 # python3-opencv
from memryx import mxa # memryx

MODEL_ID = 0
'''Model: MobileNet v1 + MobileNet v2'''
GROUP_ID = 0
'''MPU device group 0'''

class RunInferenceConfig:
  '''Since two models share the same procedure to run inference, we make it a
    common sub-routine with structure of parameters here.'''
  def __init__(self):
    self.model_id = 0 # model ID
    self.group_id = 0 # MPU device group ID
    self.iport = 0 # input port ID
    self.ifmap = None # input feature map
    self.oport = 0 # output port ID
    self.ofmap = None # output feature map

def run_inference(config: RunInferenceConfig) -> int:
  '''Simple frame-in-frame-out inference. Two models can run inference
    simultaneously for they are using different ports.'''
  err = 0

  # 1. Enable data transfer of both models to device.
  if not err:
    err = mxa.set_stream_enable(config.model_id, 0)  
  # 2. Write input feature map to device to run inference
  if not err:
    err = mxa.stream_ifmap(config.model_id,
      config.iport, config.ifmap, timeout=200)
  # 3. Read output feature map from device after inference
  if not err:
    err = mxa.stream_ofmap(config.model_id,
      config.oport, config.ofmap, timeout=200)
  # 4. Disable data transfer of this model to device.
  if not err:
    err = mxa.set_stream_disable(config.model_id, 0)

  return err

def run_inference_model_0() -> None:
  '''Model 0 run inference, this sub-routine runs in background thread'''
  # Assumes input feature map uses only flow 0 as format float32(224,224,3)
  ifmap = None
  # Assumes output feature map uses only flow 0 as format float32(1,1,1000)
  ofmap = np.zeros((1,1,1000), dtype=np.float32) # allocate memory space

  # 1. Pre-process input feature map
  img = cv2.imread(r"image.png")
  img = cv2.resize(img, (224,224), interpolation=cv2.INTER_LINEAR)
  ifmap = img.astype(np.float32) / 127.5 - 1 # type(ifmap) = <class 'numpy.ndarray'>

  # 2. Run inference setup
  config = RunInferenceConfig()
  config.model_id = MODEL_ID # model 0
  config.group_id = GROUP_ID # device 0
  config.iport = 0 # input port 0 (flow 0)
  config.ifmap = ifmap # input feature map
  config.oport = 0 # output port 0 (flow 0)
  config.ofmap = ofmap # output feature map

  # 3. Run inference common sub-routine
  err = run_inference(config)

  # 4. Post-process output feature map
  if not err:
    ofmap = ofmap.reshape(1000) # reshape to single dimension
    argmax = np.argmax(ofmap)
    print(" - Model 0 argmax =", argmax)
  else:
    print(" - Model 0 failed to run inference =", err)

def run_inference_model_1() -> None:
  '''Model 1 run inference, this sub-routine runs in background thread'''
  # Assumes input feature map uses only flow 0 as format float32(224,224,3)
  ifmap = None
  # Assumes output feature map uses only flow 0 as format float32(1,1,1000)
  ofmap = np.zeros((1,1,1000), dtype=np.float32) # allocate memory space

  # 1. Pre-process input feature map
  img = cv2.imread(r"images/siamese_cat.jpg")
  img = cv2.resize(img, (224,224), interpolation=cv2.INTER_LINEAR)
  ifmap = img.astype(np.float32) / 127.5 - 1 # type(ifmap) = <class 'numpy.ndarray'>

  # 2. Run inference setup
  config = RunInferenceConfig()
  config.model_id = MODEL_ID # model 0
  config.group_id = GROUP_ID # device 0
  config.iport = 1 # input port 1 (flow 1)
  config.ifmap = ifmap # input feature map
  config.oport = 1 # output port 1 (flow 1)
  config.ofmap = ofmap # output feature map

  # 3. Run inference common sub-routine
  err = run_inference(config)

  # 4. Post-process output feature map
  if not err:
    ofmap = ofmap.reshape(1000) # reshape to single dimension
    argmax = np.argmax(ofmap)
    print(" - Model 1 argmax =", argmax)
  else:
    print(" - Model 1 failed to run inference =", err)

def main():
  '''Main process, create two threads to run inferences in parallel.'''
  err = 0

  # 1. Bind MPU device group 0 as MX3:Cascade to model.
  if not err:
    err = mxa.open(MODEL_ID, GROUP_ID, 3) # 3 = MX3:Cascade

  # 2. Download weight memory and model to device. Because two models are
  # compiled together in one DFP file and coexist with no hardware resources
  # overlapped, we only need to download to device once.
  if not err:
    err = mxa.download(MODEL_ID, r"models/mobilenet_v1_v2.dfp", 0, # model_idx = 0
      mxa.download_type_wtmem_and_model)

  # 3. Run two models simultaneously using threads
  t0 = threading.Thread(target=run_inference_model_0, args=())
  t1 = threading.Thread(target=run_inference_model_1, args=())
  t0.start()
  t1.start()
  t0.join()
  t1.join()

  # 4. Always remember to clean-up resources before leaving.
  mxa.close(MODEL_ID)

  # End of process
  if not err:
    print("success.")
  else:
    print("failure.")

if __name__ == "__main__":
  main()

