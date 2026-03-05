# MemxDriver AOSP Integration Guide

This project provides an installation script to quickly integrate the **MemxDriver HAL Service** (including AIDL, JNI, System App, and SELinux policies) into a target AOSP tree.

## Included Components

- `android_build/vendor/memryx/` HAL source code and AIDL interface
- `android_build/device/<vendor>/<product>/sepolicy/` custom SELinux policy for memxdriver
- `MemxSampleApp` Android Studio sample application

---

## Directory Structure
```
├── install.sh                               # Auto-installation script for AOSP integration
├── android_build/                           # AOSP build-top
│   ├── vendor/memryx/                       # MemxDriver HAL sources, AIDL interfaces, JNI, and System App
│   │   ├── aidl/                            # AIDL interface definition (IMemxDriverService.aidl)
│   │   ├── hal_service/                     # Native HAL service binary and init files
│   │   ├── jni/                             # JNI bridge to native HAL library
│   │   ├── prebuilt/                        # Prebuilt shared libraries and headers (replaces udriver source build)
│   │   └── system_app/                      # MemxDriverSystemApp Java client (binds to HAL via AIDL)
│   └── device/<vendor>/<product>/sepolicy/  # SELinux policy for memxdriver HAL, contains file_contexts, service_contexts, and .te files
└── MemxSampleApp                            # Android Studio sample app that binds to MemxDriverSystemApp
```
> Note:
> `../udriver/` and `../kdriver/` are external components not included. See "Additional Components" below for integration details.

---

### Additional Components (outside android/)

- **`../udriver/`** (not included here):

  Shared memx native library (`libmemx_android.so`) providing low-level device access. This is required by `hal_service` and JNI layers.
  Superseded by `prebuilt/<abi>/<platform>/libmemx_android.so` generated via `build_android.sh` in udriver folder.

  > You must **manually download the Android NDK** (e.g., from [developer.android.com/ndk](https://developer.android.com/ndk)) and set the correct path in `build_android.sh` by editing:
  >
  > ```bash
  > NDK_PATH=/path/to/your/ndk
  > ```
  >
  > The script uses this path to cross-compile the shared library for multiple Android versions (e.g. android-32/33/34/35) and ABIs (arm64-v8a, armeabi-v7a).
  >
  > **Make sure the selected NDK version supports your target Android API level.**
  >

  For example, to build for **Android 14 (API level 34)**, you need **NDK r26 or higher**, which includes support for `android-34` in its toolchain.  >
  Then, edit `vendor/memryx/prebuilt/Android.bp` to point to your output:
  >
  > ```bp
  > cc_prebuilt_library_shared {
  >     name: "libmemx_android",
  >     vendor: true,
  >     export_include_dirs: ["android-34/include"],
  >     arch: {
  >         arm64: {
  >             srcs: ["android-34/arm64-v8a/libmemx_android.so"],
  >         },
  >     },
  > }
  > ```
  >
  > Replace `"android-34"` and `"arm64-v8a"` with the version and ABI you want to test.

- **`../kdriver/`** (not included here):

  The Memx kernel driver must be manually integrated into the target AOSP kernel source tree (e.g. `drivers/memryx/`) and built as part of the kernel image.
  This driver is tightly coupled with the kernel version and configuration, so **prebuilt binaries are not provided**.

  > To integrate the kernel driver:
  >
  > - Copy `kdriver/` sources into the AOSP kernel tree (e.g. `kernel/<vendor>/drivers/memryx/`)
  > - Add it to the kernel build system (`Makefile`, `Kconfig`)
  > - Enable it via `defconfig`
  > - Ensure it creates the expected device node (e.g. `/sys/memx0` or `/dev/memx0`)
  > - Add appropriate SELinux labels in `file_contexts`
  >
  > The driver must be compiled together with the target AOSP kernel to ensure compatibility with kernel headers and APIs.

---

## How to Install

### 1. Source AOSP environment

Before using the script, make sure your AOSP environment is set up:

```bash
source build/envsetup.sh
lunch <target_product>-<build_variant>
```

### 2. Run the install script

From the root of this repo, execute:

```bash
./install.sh
```

The script will:

- Auto-detect `AOSP_ROOT`, `PRODUCT`, and `VENDOR`
- Copy the MemxDriver sources to:
  - `vendor/memryx/`
  - `device/<vendor>/<product>/sepolicy/`
- Generate and apply `sepolicy_vendor.patch` if needed

---

## Manual Checks After Installation

You **must** manually update the following files to ensure proper integration.

### 1. `device.mk`

Update to include:

```makefile
PRODUCT_PACKAGES += \
    memxdriver_hal_service \
    memxdriver_jni \
    com.memryx.memxdriver.IMemxDriverService-V1-ndk \
    MemxDriverSystemApp

PRODUCT_COPY_FILES += \
    vendor/memryx/MemxDriverSystemApp/privapp-permissions-com.memryx.systemapp.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/privapp-permissions-com.memryx.systemapp.xml \
    vendor/memryx/hal_service/memxdriver_hal_service.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/memxdriver_hal_service.rc \
    vendor/memryx/hal_service/memxdriver_hal_service.xml:$(TARGET_COPY_OUT_VENDOR)/etc/vintf/manifest/memxdriver_hal_service.xml
```

---

### 2. `BoardConfig.mk`

Add the custom SELinux policy directory:

```makefile
BOARD_SEPOLICY_DIRS += device/<vendor>/<product>/sepolicy
BOARD_VENDOR_SEPOLICY_DIRS += device/<vendor>/<product>/sepolicy/vendor
TARGET_VENDOR_SECONTEXTS_FILE := device/<vendor>/<product>/sepolicy/vendor/service_contexts
TARGET_VENDOR_FILE_CONTEXTS_FILE := device/<vendor>/<product>/sepolicy/vendor/file_contexts
```

---

## Customization (Optional)

The install script supports parameter override:

```bash
./install.sh <vendor> <soc> <product>
```

---

## Post-install Steps

After installation and manual checks:

```bash
m  # or m MemxDriverSystemApp memxdriver_hal_service
```

If everything integrates properly, you should see:

- `memxdriver_hal_service` launched by init
- `MemxDriverSystemApp` installed on the system image
- Working `IMemxDriverService` via AIDL

---

## Troubleshooting

- **SELinux denial**: Run `dmesg | grep avc` and verify `hal_memxdriver` domain permissions
- **App can't find service**: Ensure AIDL `instance=default`, and `hwservicemanager` sees the HAL
- **Patch not applied**: Check `install.sh` log for patch conflicts

---
