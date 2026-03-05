#################################################################
# 
# Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
#
#################################################################
cmake_minimum_required(VERSION 2.8)
project( memx_c_examples )
find_package( OpenCV REQUIRED )
include_directories( ${OpenCV_INCLUDE_DIRS} )

# Example 1: Basic Inference
add_executable( memx_c_example1 memx_c_example1.cpp )
target_link_libraries( memx_c_example1 ${OpenCV_LIBS} pthread memx)

# Example 2: Multiple Models Coexist
add_executable( memx_c_example2 memx_c_example2.cpp )
target_link_libraries( memx_c_example2 ${OpenCV_LIBS} pthread memx)

# Example 3: Runtime Model Swap
add_executable( memx_c_example3 memx_c_example3.cpp )
target_link_libraries( memx_c_example3 ${OpenCV_LIBS} pthread memx)

# Example 4: Runtime Device Selection
add_executable( memx_c_example4 memx_c_example4.cpp )
target_link_libraries( memx_c_example4 ${OpenCV_LIBS} pthread memx)

