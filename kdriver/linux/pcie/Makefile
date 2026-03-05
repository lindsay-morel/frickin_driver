CONFIG_MODULE_SIG=n
INCLUDES += -I$(PWD)/../../include

obj-m := memx_cascade_plus_pcie.o

# Objects that make up the module
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

# Standard out-of-tree build (Yocto passes KERNEL_SRC/KERNEL_PATH; Kbuild uses M=)
all:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) EXTRA_CFLAGS="$(INCLUDES)" modules

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) clean
	rm -f *.out app
