# MemryX MX3 PCIe kernel module Makefile (Yocto-friendly)

CONFIG_MODULE_SIG ?= n

# Yocto/module.bbclass will pass KERNEL_SRC (and usually KERNEL_PATH)
# Keep a fallback for non-Yocto manual builds.
KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build
KERNEL_PATH ?= $(KERNEL_SRC)

PWD := $(shell pwd)

INCLUDES += -I$(PWD)/../../include
EXTRA_CFLAGS += $(INCLUDES)

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
	memx_fs_hwmon.o

.PHONY: all modules clean

all: modules

modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" modules

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) clean
	rm -f app *.out
