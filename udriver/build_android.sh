#!/bin/bash
set -e

echo "Compiling prebuilt libs for Android..."

NDK_PATH="${NDK_PATH:-/home/memryx/android-ndk-r27c}"
ANDROID_PLATFORMS=("android-32" "android-33" "android-34" "android-35")
ANDROID_ABIS=("arm64-v8a" "armeabi-v7a")
HEADER_SRC="include/common/memx.h"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_BASE="${SCRIPT_DIR}/../android/android_build/vendor/memryx/prebuilt"

TOOLCHAIN_FILE="${NDK_PATH}/build/cmake/android.toolchain.cmake"
if [ ! -f "${TOOLCHAIN_FILE}" ]; then
  echo "Error: Toolchain file not found at ${TOOLCHAIN_FILE}"
  echo "Please ensure NDK_PATH is correct and NDK is properly installed."
  echo "Android NDK can be download from https://developer.android.com/ndk/downloads"
  exit 1
fi

for ABI in "${ANDROID_ABIS[@]}"; do
  for PLATFORM in "${ANDROID_PLATFORMS[@]}"; do
    echo "Building for ${PLATFORM}, ABI=${ABI}..."

    BUILD_DIR="build_android/${PLATFORM}/${ABI}"
    OUTPUT_SO_DIR="${OUTPUT_BASE}/${PLATFORM}/${ABI}"
    OUTPUT_INC_DIR="${OUTPUT_BASE}/${PLATFORM}/include"

    rm -rf "${BUILD_DIR}"
    mkdir -p "${OUTPUT_SO_DIR}" "${OUTPUT_INC_DIR}"

    cmake -B "${BUILD_DIR}" -S . \
      -DCMAKE_TOOLCHAIN_FILE="${NDK_PATH}/build/cmake/android.toolchain.cmake" \
      -DANDROID_ABI="${ABI}" \
      -DANDROID_PLATFORM="${PLATFORM}" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_LIBRARY_OUTPUT_DIRECTORY="${OUTPUT_SO_DIR}" \
      -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="${OUTPUT_SO_DIR}" \
      -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY="${OUTPUT_SO_DIR}"

    cmake --build "${BUILD_DIR}" --target memx_android
    [ -f "${OUTPUT_SO_DIR}/libmemx_android.so" ] || {
      echo "Build failed: .so not found!"
      exit 1
    }
    cp "${HEADER_SRC}" "${OUTPUT_INC_DIR}/memx.h"

    echo ""
    echo "======== ${PLATFORM} / ${ABI} build OK ========"
    echo "==> ${OUTPUT_SO_DIR}/libmemx_android.so"
    echo "==> ${OUTPUT_INC_DIR}/memx.h"
    echo ""
  done
  echo ""
  if [ -f "${NDK_PATH}/source.properties" ]; then
    NDK_VER=$(grep "Pkg.Revision" "${NDK_PATH}/source.properties" | cut -d' ' -f3)
    echo "NDK Path: ${NDK_PATH}, Version: ${NDK_VER}"
  else
    echo "NDK Path: ${NDK_PATH}, Unknown Version"
  fi
done
tree ${OUTPUT_BASE}
