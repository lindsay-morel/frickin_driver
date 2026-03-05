#!/bin/bash

if [[ $(whoami) != "root" ]]; then
  echo "Must run as root (sudo)"
  exit 1
fi

echo ""
echo -e "\e[1;96mMemryX Driver/Library Installer\e[0m"
echo ""


# check for supported architectures
ARCH=$(uname -m)
if [[ $ARCH == "x86_64" ]] || [[ $ARCH == "aarch64" ]]; then
  echo ""
else
  echo -e "\e[1;91mERROR: unsupported architecture ${ARCH}. Need x86_64 or aarch64.\e[0m"
  exit 1
fi
  

# set variables
export TMPDIR=`mktemp -d /tmp/selfextract.XXXXXX`
ARCHIVE=`awk '/^__ARCHIVE_SECTION__/ {print NR + 1; exit 0; }' $0`


# extracts the data into TMPDIR
tail -n+$ARCHIVE $0 | tar xz -C $TMPDIR
if [ $? -ne 0 ]; then
  exit 1
fi

echo -e "\e[1;97mIn the following prompts, the default paths are filled in."
echo -e "Edit them before hitting ENTER if you wish to change them."
echo -e "Press Ctrl+C to quit.\e[0m\n\n"


# libmemx.so
printf "\e[1;96mWhere should we put the shared library?\e[0m  "
read -i "/usr/lib/libmemx.so" -e LIBMEMX
mkdir -p "$(dirname ${LIBMEMX})"
cp -v ${TMPDIR}/${ARCH}/libmemx.so $LIBMEMX
if [ $? -ne 0 ]; then
  exit 1
fi
echo ""

# headers
printf "\e[1;96mWhat folder should we put header files in?\e[0m  "
read -i "/usr/include/memx/" -e INCMEMX
mkdir -p $INCMEMX
cp -v ${TMPDIR}/include/* $INCMEMX
if [ $? -ne 0 ]; then
  exit 1
fi
echo ""

# udev rule
printf "\e[1;96mWhere should we put the udev rule?\e[0m  "
read -i "/usr/lib/udev/rules.d/" -e UDEVDIR
cp -v ${TMPDIR}/50-memxchip.rules ${UDEVDIR}/
if [ $? -ne 0 ]; then
  exit 1
fi
echo ""

# device firmware location
printf "\e[1;96mWhere should we put the firmware file?\e[0m  "
read -i "/lib/firmware/" -e FWDIR
cp -v ${TMPDIR}/cascade.bin ${FWDIR}/
if [ $? -ne 0 ]; then
  exit 1
fi
echo ""

# kernel driver
printf "\e[1;96mWhere should we put kernel driver source?\e[0m  "
read -i "/usr/src/memx-drivers/" -e KDRIVER
mkdir -p $KDRIVER
cp -rv ${TMPDIR}/kdriver/* $KDRIVER
if [ $? -ne 0 ]; then
  exit 1
fi
echo ""


# auto build module?
printf "\e[1;96mBuild and install the kernel module now?\e[0m  "
read -i "yes" -e BUILDNOW
if [[ $BUILDNOW == "yes" ]] || [[ $BUILDNOW == "Y" ]] || [[ $BUILDNOW == "y" ]] || [[ $BUILDNOW == "YES" ]]; then
  echo -e "\e[1;96mBuilding pre-production EVB USB driver....\e[0m"
  cd ${KDRIVER}/usb
  make all && make install
  if [ $? -ne 0 ]; then
    exit 1
  else
    echo -e "\e[1;96mSuccess!\e[0m"
  fi
  echo -e "\e[1;96mBuilding MX3 PCIe driver....\e[0m"
  cd ${KDRIVER}/pcie
  make all && make install
  if [ $? -ne 0 ]; then
    exit 1
  else
    echo -e "\e[1;96mSuccess!\e[0m"
  fi
else
  echo -e "\e[1;96mOkay, skipped.\e[0m"
fi


echo -e "\n\e[1;92mSetup complete.\e[0m\n"
exit 0


__ARCHIVE_SECTION__
