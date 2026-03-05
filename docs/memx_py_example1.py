"""
Copyright (c) 2019-2022 MemryX Inc.
All Rights Reserved.

============
Information:
============
File Name: memx_py_example1.py
Project: MiX-3

============
Description:
============
Example 1: Basic Inference

========
Authors:
========

"""

import numpy as np # numpy
import cv2 # python3-opencv
from memryx import mxa # memryx

def main():
  '''Main process'''
  model_id = 0 # model 0
  group_id = 0 # MPU device group 0
  argmax = 0 # index with maximum score

  # Assumes input feature map uses only flow 0 as format float32(224,224,3)
  ifmap = None
  # Assumes output feature map uses only flow 0 as format float32(1,1,1000)
  ofmap = np.zeros((1,1,1000), dtype=np.float32) # allocate memory space

  # 1. Bind MPU device group 0 as MX3:Cascade to model 0.
  err = mxa.open(model_id, group_id, 3) # 3 = MX3:Cascade
  print(" 1. mxa.open =", err)

  # 2. Download model within DFP file to MPU device group, input and
  # output feature map shape is auto. configured after download complete.
  if not err:
    err = mxa.download(model_id,
      r"models/mobilenet_v1.dfp", 0, # model_idx = 0
      mxa.download_type_wtmem_and_model)
  print(" 2. mxa.download =", err)

  # 3. Enable data transfer of this model to device. Set to no wait here
  # since driver will go to data transfer state eventually.
  if not err:
    err = mxa.set_stream_enable(model_id, 0)
  print(" 3. mxa.set_stream_enable =", err)

  # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  # 4. maybe put some input feature map pre-processing here
  # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  if not err:
    img = cv2.imread(r"image.png")
    img = cv2.resize(img, (224,224), interpolation=cv2.INTER_LINEAR)
    ifmap = img.astype(np.float32) / 127.5 - 1 # type(ifmap) = <class 'numpy.ndarray'>
  print(" 4. pre-processing")

  # 5. Stream input feature map to device flow 0 and run inference.
  if not err:
    err = mxa.stream_ifmap(model_id, 0, ifmap, timeout=200) # 200 ms
  print(" 5. mxa.stream_ifmap =", err)

  # 6. Stream output feature map from device flow 0 after inference.
  if not err:
    err = mxa.stream_ofmap(model_id, 0, ofmap, timeout=200) # 200 ms
  print(" 6. mxa.stream_ofmap =", err)

  # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  # 7. maybe put some output feature map post-processing here
  # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  if not err:
    ofmap = ofmap.reshape(1000) # reshape to single dimension
    argmax = np.argmax(ofmap)
  print(" 7. post-processing, argmax =", argmax)

  # 8. Always remeber to clean-up resources before leaving.
  mxa.close(model_id)
  print(" 8. mxa.close =", err)

  # End of process
  if not err:
    print("success.")
  else:
    print("failure.")

if __name__ == "__main__":
  main()

