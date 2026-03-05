#!/bin/bash

cd data && tar hczf ../payload.tar.gz ./*
if [ $? -ne 0 ]; then
  exit 1
fi

cd ../
cat install_stub.sh payload.tar.gz > install.sh
if [ $? -ne 0 ]; then
  exit 1
fi
chmod +x install.sh && rm -f payload.tar.gz
