cmake_minimum_required(VERSION 3.10)

project(memx_android C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(memx_android SHARED
	memx.c
	dfp/memx_dfp.c
	dfp/mxpack.c
	gbf/memx_gbf.c
	mpu/memx_cascadeplus.c
	mpu/memx_mpu.c
	mpu/memx_device_manager.c
	mpuio/memx_mpuio.c
	mpuio/memx_mpuio_comm.c
	mpuio/memx_usb.c
	mpuio/memx_pcie_xflow.c
	mpuio/memx_pcie.c
	platform/memx_platform.c
	util/memx_list.c
	util/memx_ringbuffer.c
	util/memx_log.c
)

target_include_directories(memx_android PRIVATE
	"include/common"
	"include/dfp"
	"include/gbf"
	"include/mpu"
	"include/mpuio"
	"include/platform"
	"include/util"
)
