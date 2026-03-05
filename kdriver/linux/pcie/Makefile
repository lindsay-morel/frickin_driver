# MemryX MX3 PCIe kernel module Makefile (Yocto-friendly)

# When building under Yocto, KERNEL_SRC is passed in from the recipe.
# For local/manual builds, fall back to the running kernel headers.
KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build

# Kbuild variables
obj-m := memx_cascade_plus_pcie.o
memx_cascade_plus_pcie-objs := \
	memx_feature.o \
	memx_xflow.o \
	memx_msix_irq.o \
	memx_cascade_pciemain.o \
	memx_fw_cmd.o \
	memx_fw_init.o \
	memx_pcie_dev_list_ctrl.o \
	memx_fs_proc.o \
	memx_fs_sys.o \
	memx_fs.o \
	memx_fw_log.o \
	memx_fs_hwmon.o \
	memx_fs_device.o \
	memx_dma.o

# Default target (Yocto/module.bbclass will call "make modules")
all: modules

# Build the module
modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(CURDIR) modules

# Install the module into the staging root (Yocto/module.bbclass calls this)
modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(CURDIR) modules_install

# Clean build artifacts
clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(CURDIR) clean
