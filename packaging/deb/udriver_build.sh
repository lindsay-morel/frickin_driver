#!/bin/bash

if [[ $(uname -m) == "x86_64" ]]; then
  cd udriver && make dist_x86_64 && mkdir -vp debian/temp/lib debian/temp/include && chmod -R 777 debian && INSTALL_PREFIX=$(pwd)/debian/temp make install
else
  cd udriver && make dist_aarch64 && mkdir -vp debian/temp/lib debian/temp/include && chmod -R 777 debian && INSTALL_PREFIX=$(pwd)/debian/temp make install
fi
