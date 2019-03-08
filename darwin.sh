#!/bin/bash
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

verify_enable() {
  feature="$1"
  feature_status=${!1}
  if [ "$feature" = "BUSTACHE_ENABLED" ]; then
    BUSTACHE_MAX="9"
    ## we should check the xcode version
    CLANG_VERSION=`clang --version | head -n 1 | awk '{print $4}'`
    CLANG_MAJOR=`echo $CLANG_VERSION | cut -d. -f1`
    CLANG_MINOR=`echo $CLANG_VERSION | cut -d. -f2`
    CLANG_REVISION=`echo $CLANG_VERSION | cut -d. -f3`
    if [ "$CLANG_MAJOR" -ge "$BUSTACHE_MAX" ]; then
      echo "false"
    else
      echo "true"
    fi
  else
    echo "true"
  fi
}

add_os_flags(){
  :
}

install_bison() {
  BISON_INSTALLED="false"
  if [ -x "$(command -v bison)" ]; then
    BISON_VERSION=`bison --version | head -n 1 | awk '{print $4}'`
    BISON_MAJOR=`echo $BISON_VERSION | cut -d. -f1`
    if (( BISON_MAJOR >= 3 )); then
      BISON_INSTALLED="true"
    fi
  fi
  if [ "$BISON_INSTALLED" = "false" ]; then
    ## ensure that the toolchain is installed
    INSTALL_BASE="sudo zypper in -y gcc gcc-c++"
    ${INSTALL_BASE}
    wget https://ftp.gnu.org/gnu/bison/bison-3.0.4.tar.xz
    tar xvf bison-3.0.4.tar.xz
    pushd bison-3.0.4
    ./configure
    make
    sudo make install
    popd
  fi

}

bootstrap_cmake(){
  brew install cmake
}
build_deps(){

  COMMAND="brew install cmake"
  INSTALLED=()
  INSTALLED+=("bzip2")
  for option in "${OPTIONS[@]}" ; do
    option_value="${!option}"
    if [ "$option_value" = "${TRUE}" ]; then
      # option is enabled
      FOUND_VALUE=""
      for cmake_opt in "${DEPENDENCIES[@]}" ; do
        KEY=${cmake_opt%%:*}
        VALUE=${cmake_opt#*:}
        if [ "$KEY" = "$option" ]; then
          FOUND_VALUE="$VALUE"
          if [ "$FOUND_VALUE" = "libcurl" ]; then
            brew install curl --with-openssl
          elif [ "$FOUND_VALUE" = "libpcap" ]; then
            INSTALLED+=("libpcap")
          elif [ "$FOUND_VALUE" = "openssl" ]; then
            INSTALLED+=("openssl")
          elif [ "$FOUND_VALUE" = "libusb" ]; then
            INSTALLED+=("libusb")
          elif [ "$FOUND_VALUE" = "libpng" ]; then
            INSTALLED+=("libpng")
          elif [ "$FOUND_VALUE" = "bison" ]; then
            install_bison
          elif [ "$FOUND_VALUE" = "flex" ]; then
            INSTALLED+=("flex")
          elif [ "$FOUND_VALUE" = "python" ]; then
            INSTALLED+=("python")
          elif [ "$FOUND_VALUE" = "boost" ]; then
            INSTALLED+=("boost")
          elif [ "$FOUND_VALUE" = "lua" ]; then
            INSTALLED+=("lua")
          elif [ "$FOUND_VALUE" = "libtool" ]; then
            INSTALLED+=("libtool")
          elif [ "$FOUND_VALUE" = "automake" ]; then
            INSTALLED+=("automake")
            INSTALLED+=("autoconf")
          elif [ "$FOUND_VALUE" = "gpsd" ]; then
            INSTALLED+=("gpsd")
          elif [ "$FOUND_VALUE" = "libarchive" ]; then
            INSTALLED+=("bzip2")
          fi
        fi
      done

    fi
  done

  for option in "${INSTALLED[@]}" ; do
    COMMAND="${COMMAND} $option"
  done

  echo "Ensuring you have all dependencies installed..."
  ${COMMAND} > /dev/null 2>&1

  for option in "${INSTALLED[@]}" ; do
    if [ "$option" = "curl" ]; then
      brew link curl --force > /dev/null 2>&1
    fi
  done
}
