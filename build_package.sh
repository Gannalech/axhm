# Operazioni eseguite:
# 1) copy .c e .h file sotto buil_dir 
# 2) esecuzione della compilazione

# CUSTOMIZE path OPENWRT_HOME
# OPENWRT_HOME=../../openwrt

#!/bin/bash

set -e

if [ -e .env ]
then
    echo "loading .env file"
    . .env
else
    echo "missing .env file - OPERATION FAILED!!!!"
    exit 1
fi

if [[ -z "${OPENWRT_HOME}" ]]; then
  #non setttata
  echo "variable OPENWRT_HOME not setted -> exit"
  exit 1
else
  echo "OPENWRT_HOME: ${OPENWRT_HOME}"
fi

TARGET=${OPENWRT_HOME}/build_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/mh-1.0.0
echo "TARGET: ${TARGET}"
cd ./src
find . -name '*.c' -exec cp --parents {} ../${TARGET} \;
find . -name '*.h' -exec cp --parents {} ../${TARGET} \;
cd ..
cp openwrt_makefile/Makefile ${TARGET}

CURR_DIR=`pwd`

# esecuzione comando compilazione pacchetto
cd ${OPENWRT_HOME}
make -j1 V=s package/mh/compile

cd ${CURR_DIR}
