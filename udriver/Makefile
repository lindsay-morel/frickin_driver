RM := rm -rfv
CC := gcc

CC_FLAGS := \
	-Wall -Wextra -pipe -fPIC -O3
CC_DEBUG_FLAGS := \
	-Wall -Wextra -pipe -fPIC -O0 -g -fsanitize=address

INSTALL_PREFIX ?= /usr
BINARY_DIR := build
ICOMMON_DIR := include/common
IUTIL_DIR := include/util
IMPUIO_DIR := include/mpuio
IMPU_DIR := include/mpu
IGBF_DIR := include/gbf
IDFP_DIR := include/dfp
ITOOL_DIR := include/tool
IPLATFORM_DIR := include/platform
UTIL_DIR := util
MPUIO_DIR := mpuio
PLATFORM_DIR := platform
MPU_DIR := mpu
GBF_DIR := gbf
DFP_DIR := dfp

.phony: all
all: memx

.phony: create_dir
create_dir:
	@mkdir -p $(BINARY_DIR)

.phony: memx
memx: create_dir
	$(CC) -shared -o $(BINARY_DIR)/libmemx.so $(CC_FLAGS) -march=native \
		memx.c $(DFP_DIR)/*.c $(GBF_DIR)/*.c $(MPU_DIR)/*.c $(MPUIO_DIR)/*.c $(PLATFORM_DIR)/*.c $(UTIL_DIR)/*.c -I$(ICOMMON_DIR) -I$(IDFP_DIR) -I$(IGBF_DIR) -I$(IMPU_DIR) -I$(IMPUIO_DIR) -I$(IUTIL_DIR) -I$(ITOOL_DIR) -I$(IPLATFORM_DIR)

.phony: dist_x86_64
dist_x86_64: create_dir
	$(CC) -shared -o $(BINARY_DIR)/libmemx.so $(CC_FLAGS) -mpopcnt -msse -msse2 -msse3 -mssse3 -msse4.1 -msse4.2 -mpclmul -mcx16 -msahf -mtune=generic \
		memx.c $(DFP_DIR)/*.c $(GBF_DIR)/*.c $(MPU_DIR)/*.c $(MPUIO_DIR)/*.c $(PLATFORM_DIR)/*.c $(UTIL_DIR)/*.c -I$(ICOMMON_DIR) -I$(IDFP_DIR) -I$(IGBF_DIR) -I$(IMPU_DIR) -I$(IMPUIO_DIR) -I$(IUTIL_DIR) -I$(ITOOL_DIR) -I$(IPLATFORM_DIR)

.phony: dist_aarch64
dist_aarch64: create_dir
	$(CC) -shared -o $(BINARY_DIR)/libmemx.so $(CC_FLAGS) -march=armv8-a+simd \
		memx.c $(DFP_DIR)/*.c $(GBF_DIR)/*.c $(MPU_DIR)/*.c $(MPUIO_DIR)/*.c $(PLATFORM_DIR)/*.c $(UTIL_DIR)/*.c -I$(ICOMMON_DIR) -I$(IDFP_DIR) -I$(IGBF_DIR) -I$(IMPU_DIR) -I$(IMPUIO_DIR) -I$(IUTIL_DIR) -I$(ITOOL_DIR) -I$(IPLATFORM_DIR)

.phony: mx_ace_build
mx_ace_build:
	@mkdir -p $(BINARY_DIR)/udriver20
	@mkdir -p $(BINARY_DIR)/udriver20/x86_x64
	@mkdir -p $(BINARY_DIR)/udriver20/arm_32
	@mkdir -p $(BINARY_DIR)/udriver20/arm_64

.phony: mx_ace_udrv20
mx_ace_udrv20: mx_ace_build
	g++ -c -Wall -Wextra -O3 -fpermissive memx.c $(DFP_DIR)/*.c $(GBF_DIR)/*.c $(MPU_DIR)/*.c $(MPUIO_DIR)/*.c $(PLATFORM_DIR)/*.c $(UTIL_DIR)/*.c udriver20/*.cpp -I$(ICOMMON_DIR) -I$(IDFP_DIR) -I$(IGBF_DIR) -I$(IMPU_DIR) -I$(IMPUIO_DIR) -I$(IUTIL_DIR) -I$(ITOOL_DIR) -I$(IPLATFORM_DIR)
	ar rcs $(BINARY_DIR)/udriver20/x86_x64/libudrv20.a *.o
	arm-linux-gnueabihf-g++ -c -Wall -Wextra -O3 -fpermissive memx.c $(DFP_DIR)/*.c $(GBF_DIR)/*.c $(MPU_DIR)/*.c $(MPUIO_DIR)/*.c $(PLATFORM_DIR)/*.c $(UTIL_DIR)/*.c udriver20/*.cpp -I$(ICOMMON_DIR) -I$(IDFP_DIR) -I$(IGBF_DIR) -I$(IMPU_DIR) -I$(IMPUIO_DIR) -I$(IUTIL_DIR) -I$(ITOOL_DIR) -I$(IPLATFORM_DIR)
	arm-linux-gnueabihf-ar rcs $(BINARY_DIR)/udriver20/arm_32/libudrv20.a *.o
	aarch64-linux-gnu-g++ -march=armv8-a -c -Wall -Wextra -O3 -fpermissive memx.c $(DFP_DIR)/*.c $(GBF_DIR)/*.c $(MPU_DIR)/*.c $(MPUIO_DIR)/*.c $(PLATFORM_DIR)/*.c $(UTIL_DIR)/*.c udriver20/*.cpp -I$(ICOMMON_DIR) -I$(IDFP_DIR) -I$(IGBF_DIR) -I$(IMPU_DIR) -I$(IMPUIO_DIR) -I$(IUTIL_DIR) -I$(ITOOL_DIR) -I$(IPLATFORM_DIR)
	aarch64-linux-gnu-ar rcs $(BINARY_DIR)/udriver20/arm_64/libudrv20.a *.o

.phony: mx_ace_release
mx_ace_release: mx_ace_udrv20

.phony: install
install: $(BINARY_DIR)/libmemx.so
	cp -v $(BINARY_DIR)/libmemx.so $(INSTALL_PREFIX)/lib
	mkdir -p $(INSTALL_PREFIX)/include/memx
	cp -v $(ICOMMON_DIR)/memx.h $(INSTALL_PREFIX)/include/memx/

.phony: clean
clean:
	@$(RM) $(BINARY_DIR)/*

