#!/bin/bash

set -e

# Check if envsetup.sh and lunch were sourced
if [[ -z "$ANDROID_BUILD_TOP" || -z "$TARGET_PRODUCT" ]]; then
    echo "Environment is not set. Please run:"
    echo "  source build/envsetup.sh"
    echo "  lunch <target_product>-<build_variant>"
    exit 1
fi

PREBUILT_LIB_DIR="android_build/vendor/memryx/prebuilt"
# Enable nullglob so that the glob expands to empty list if no match
# and collect all entries ending with “/” (i.e. directories)
shopt -s nullglob
libdirs=("$PREBUILT_LIB_DIR"/*/)
if (( ${#libdirs[@]} )); then
  echo "Found prebuilt libs under $PREBUILT_LIB_DIR"
  tree ${PREBUILT_LIB_DIR}
else
  echo "No prebuilt lib found under $PREBUILT_LIB_DIR"
  echo "Please run the build_android.sh script in udriver to generate prebuilt libs first."
  exit 1
fi

VENDOR_OVERRIDE="$1"
SOC_DIR_OVERRIDE="$2"
PRODUCT_OVERRIDE="$3"

# Auto-detect AOSP root and product
AOSP_ROOT="$ANDROID_BUILD_TOP"
PRODUCT="$TARGET_PRODUCT"
DEVICE_PATH="$AOSP_ROOT/device"

# If VENDOR and SOC_DIR not passed, try auto-detection
if [[ -z "$VENDOR_OVERRIDE" ]]; then
    # Try 1: device/*/*/$PRODUCT (e.g. vendor/soc/product)
    PRODUCT_DIR=$(find "$DEVICE_PATH" -type d -path "*/$PRODUCT" | grep -E ".*/.*/$PRODUCT$" | head -n1)
    if [[ -n "$PRODUCT_DIR" ]]; then
        VENDOR=$(echo "$PRODUCT_DIR" | awk -F '/' '{print $(NF-2)}')
        SOC_DIR=$(echo "$PRODUCT_DIR" | awk -F '/' '{print $(NF-1)}')
    else
        # Try 2: device/*/$PRODUCT (e.g. vendor/product)
        PRODUCT_DIR=$(find "$DEVICE_PATH" -type d -path "*/$PRODUCT" | grep -E ".*/$PRODUCT$" | head -n1)
        if [[ -n "$PRODUCT_DIR" ]]; then
            VENDOR=$(echo "$PRODUCT_DIR" | awk -F '/' '{print $(NF-1)}')
            SOC_DIR=""
        else
            echo "Unable to auto-detect vendor directory for '$PRODUCT' in '$DEVICE_PATH'."
            echo "Please pass arguments: ./install.sh <vendor> <soc> <product>"
            exit 1
        fi
    fi
else
    VENDOR="$VENDOR_OVERRIDE"
    SOC_DIR="$SOC_DIR_OVERRIDE"
	if [[ -n "$PRODUCT_OVERRIDE" ]]; then
		PRODUCT="$PRODUCT_OVERRIDE"
	fi
fi

echo "AOSP_ROOT   : $AOSP_ROOT"
echo "VENDOR      : $VENDOR"
if [[ -n "$SOC_DIR" ]]; then
	echo "SOC_DIR     : $SOC_DIR"
	DST_SEPOLICY=$AOSP_ROOT/device/$VENDOR/$SOC_DIR/sepolicy/
	echo "INSTALL_DIR : $DST_SEPOLICY"
else
	DST_SEPOLICY=$AOSP_ROOT/device/$VENDOR/$PRODUCT_DIR/sepolicy/
	echo "INSTALL_DIR : $DST_SEPOLICY"
fi
echo ""

# Define source and destination paths
SRC_SEPOLICY=./android_build/device/vendor/product/sepolicy/
DST_SEPOLICY=$AOSP_ROOT/device/$VENDOR/$SOC_DIR/sepolicy/

echo "Copying MemryX vendor source..."
cp -r ./android_build/vendor/memryx/ "$AOSP_ROOT/vendor/"

echo ""
echo "Processing SEPolicy folder..."

# Check if destination sepolicy/vendor already exists
if [[ -d "$DST_SEPOLICY/vendor" ]]; then
    echo "️Existing sepolicy/vendor found, generating diffs..."

    TMP_PATCH_DIR=$(mktemp -d)
    diff -ruN "$DST_SEPOLICY/vendor" "$SRC_SEPOLICY/vendor" > "$TMP_PATCH_DIR/sepolicy_vendor.patch" || true

    if [[ -s "$TMP_PATCH_DIR/sepolicy_vendor.patch" ]]; then
        echo "Differences detected. Applying patch automatically..."
        if patch -p1 -d "$AOSP_ROOT" < "$TMP_PATCH_DIR/sepolicy_vendor.patch"; then
            echo "Patch applied successfully."
        else
            echo "Failed to apply patch. Please resolve conflicts manually."
            echo "You can retry with:"
            echo "  patch -p1 -d $AOSP_ROOT < $TMP_PATCH_DIR/sepolicy_vendor.patch"
            echo "Or inspect the patch file here:"
            echo "  $TMP_PATCH_DIR/sepolicy_vendor.patch"
        fi
    else
        echo "No differences found. Files are identical."
    fi
else
    echo "No existing sepolicy/vendor folder found. Copying files..."
    mkdir -p "$DST_SEPOLICY"
    cp -r "$SRC_SEPOLICY/vendor" "$DST_SEPOLICY/"
    echo "SEPolicy files copied."
fi


# Reminder for manual steps
echo ""
echo "Please review the following integration steps:"
echo "1. device.mk: Add PRODUCT_PACKAGES and PRODUCT_COPY_FILES"
echo '    PRODUCT_PACKAGES += \'
echo '        memxdriver_hal_service \'
echo '        memxdriver_jni \'
echo '        com.memryx.memxdriver.IMemxDriverService-V1-ndk \'
echo '        MemxDriverSystemApp\'
echo ""
echo '    PRODUCT_COPY_FILES += \'
echo '        vendor/memryx/MemxDriverSystemApp/privapp-permissions-com.memryx.systemapp.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/privapp-permissions-com.memryx.systemapp.xml \'
echo '        vendor/memryx/hal_service/memxdriver_hal_service.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/memxdriver_hal_service.rc \'
echo '        vendor/memryx/hal_service/memxdriver_hal_service.xml:$(TARGET_COPY_OUT_VENDOR)/etc/vintf/manifest/memxdriver_hal_service.xml \'
echo ""
echo "2. BoardConfig.mk: Add BOARD_SEPOLICY_DIRS"
echo "    BOARD_VENDOR_SEPOLICY_DIRS += device/$VENDOR/$SOC_DIR/sepolicy/vendor"
echo "    TARGET_VENDOR_SECONTEXTS_FILE += device/$VENDOR/$SOC_DIR/sepolicy/vendor/service_contexts"
echo "    TARGET_VENDOR_FILE_CONTEXTS_FILE += device/$VENDOR/$SOC_DIR/sepolicy/vendor/file_contexts"
echo ""
echo "3. Android.bp in prebuilt: Confirm platform_apis"
echo "    export_include_dirs: ["android-32/include"],"
echo "    arch: {"
echo "        arm64: {"
echo "            srcs: ["android-32/arm64-v8a/libmemx_android.so"],"
echo "        },"
echo "        arm: {"
echo "            srcs: ["android-32/armeabi-v7a/libmemx_android.so"],"
echo "        },"
echo "    },"
echo ""
echo "4. Android.bp in aidl_interface: Rebuild the interface"
echo "    rm -rf vendor/memryx/aidl_interface/aidl_api"
echo "    rm -rf out/soong/.intermediates/vendor/memryx/aidl_interface"
echo "    m -j1 com.memryx.memxdriver.IMemxDriverService"
echo "    m -j8 com.memryx.memxdriver.IMemxDriverService-update-api"
echo "    m -j8 com.memryx.memxdriver.IMemxDriverService-freeze-api"
echo ""
echo "Done. You may now run 'm' to build."
