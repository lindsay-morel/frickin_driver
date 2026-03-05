"""
Copyright (c) 2019-2022 MemryX Inc.
All Rights Reserved.

============
Information:
============
File Name: memx_py_example4.py
Project: MiX-3

============
Description:
============
Example 4: Runtime Device Selection

========
Authors:
========

"""

import threading
import numpy as np # numpy
import cv2 # python3-opencv
from memryx import mxa # memryx

MODEL_0_ID = 0
'''Model 0: MobileNet v1'''
MODEL_1_ID = 1
'''Model 1: MobileNet v2'''
GROUP_0_ID = 0
'''MPU device group 0'''
GROUP_1_ID = 1
'''MPU device group 0'''

class RunInferenceConfig:
  '''Since two models share the same procedure to run inference, we make it a
    common sub-routine with structure of parameters here.'''
  def __init__(self):
    self.model_id = 0 # model ID
    self.group_id = 0 # MPU device group ID
    self.dfp_path = None # DFP file path
    self.iport = 0 # input port ID
    self.ifmap = None # input feature map
    self.oport = 0 # output port ID
    self.ofmap = None # output feature map

def run_inference(config: RunInferenceConfig) -> int:
  '''In order to guarantee only one model can access device in multi-threading
    case, we acquire `device lock` from driver before any hardware
    configuration.'''
  timeout = 200 # 200 ms

  # 1. Here we use 'trylock()' first to test if device is acquirable, if the
  # return value is '0' means lock is acquired successfully, otherwise we move
  # on to lock another device.
  if mxa.trylock(GROUP_0_ID) == 0:
    config.group_id = GROUP_0_ID
  else:
    mxa.lock(GROUP_1_ID); # wait until lock is acquired
    config.group_id = GROUP_1_ID
  # 2. Re-configure MPU device group binded to model.
  err = mxa.reconfigure(config.model_id, config.group_id)
  print(" - Model {} is running on device {}"
    .format(config.model_id, config.group_id))

  # 3. Download weight memory and model to device.
  if not err:
    err = mxa.download(config.model_id, config.dfp_path, 0,
      mxa.download_type_wtmem_and_model)
  # 4. Enable data transfer of this model to device.
  if not err:
    err = mxa.set_stream_enable(config.model_id, 0)
  # 5. Write input feature map to device to run inference
  if not err:
    err = mxa.stream_ifmap(config.model_id,
      config.iport, config.ifmap, timeout=timeout)
  # 6. Read output feature map from device after inference
  if not err:
    err = mxa.stream_ofmap(config.model_id,
      config.oport, config.ofmap, timeout=timeout)
  # 7. Disable data transfer of this model to device.
  if not err:
    # wait to stop may take some time, but is safe
    err = mxa.set_stream_disable(config.model_id, 1)

  # 8. Always remember to release lock finally.
  mxa.unlock(config.group_id)

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
  config.model_id = MODEL_0_ID # model 0
  config.dfp_path = r"models/mobilenet_v1.dfp"
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
  config.model_id = MODEL_1_ID # model 1
  config.dfp_path = r"models/mobilenet_v2.dfp"
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
    print(" - Model 1 argmax =", argmax)
  else:
    print(" - Model 1 failed to run inference =", err)

def main():
  '''Main process, create two threads to run inferences in parallel.'''
  err = 0

  # 1. Bind MPU device group 0 as MX3:Cascade to both model 0 and model 1.
  # Group ID here can be any of MPU device group during initialization, since
  # runtime 'reconfigure()' will also initialize interface as 'open()'.
  if not err:
    err = mxa.open(MODEL_0_ID, GROUP_0_ID, 3) # 3 = MX3:Cascade
  if not err:
    err = mxa.open(MODEL_1_ID, GROUP_0_ID, 3) # 3 = MX3:Cascade

  # 2. Run two models simultaneously using threads
  t0 = threading.Thread(target=run_inference_model_0, args=())
  t1 = threading.Thread(target=run_inference_model_1, args=())
  t0.start()
  t1.start()
  t0.join()
  t1.join()

  # 3. Always remember to clean-up resources before leaving.
  mxa.close(MODEL_0_ID)
  mxa.close(MODEL_1_ID)

  # End of process
  if not err:
    print("success.")
  else:
    print("failure.")

if __name__ == "__main__":
  main()

