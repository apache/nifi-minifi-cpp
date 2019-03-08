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
  if [ "$OS_MAJOR" = "6" ]; then
    if [ "$feature" = "GPS_ENABLED" ]; then
      echo "false"
    elif [ "$feature" = "USB_ENABLED" ]; then
      echo "false"
    else
      verify_gcc_enable $feature
    fi
  else
    if [ "$feature" = "USB_ENABLED" ]; then
      echo "false"
    else
      verify_gcc_enable $feature
    fi
  fi
}

add_os_flags() {
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
  sudo zypper in -y cmake
}
build_deps(){
# Install epel-release so that cmake3 will be available for installation

  COMMAND="sudo zypper in -y gcc gcc-c++ libuuid1 libuuid-devel"
  INSTALLED=()
  INSTALLED+=("libbz2-devel")
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
            INSTALLED+=("libcurl-devel")
          elif [ "$FOUND_VALUE" = "libpcap" ]; then
            INSTALLED+=("libpcap-devel")
          elif [ "$FOUND_VALUE" = "openssl" ]; then
            INSTALLED+=("openssl")
            INSTALLED+=("openssl-devel")
          elif [ "$FOUND_VALUE" = "libusb" ]; then
            INSTALLED+=("libusb-devel")
          elif [ "$FOUND_VALUE" = "libpng" ]; then
            INSTALLED+=("libpng-devel")
          elif [ "$FOUND_VALUE" = "bison" ]; then
            install_bison
          elif [ "$FOUND_VALUE" = "flex" ]; then
            INSTALLED+=("flex")
	  elif [ "$FOUND_VALUE" = "automake" ]; then
            INSTALLED+=("automake")
          elif [ "$FOUND_VALUE" = "autoconf" ]; then
            INSTALLED+=("autoconf")
          elif [ "$FOUND_VALUE" = "libtool" ]; then
            INSTALLED+=("libtool")
          elif [ "$FOUND_VALUE" = "python" ]; then
            INSTALLED+=("python3-devel")
          elif [ "$FOUND_VALUE" = "lua" ]; then
            INSTALLED+=("lua-devel")
          elif [ "$FOUND_VALUE" = "gpsd" ]; then
            INSTALLED+=("gpsd-devel")
          elif [ "$FOUND_VALUE" = "libarchive" ]; then
            INSTALLED+=("xz-devel")
          fi
        fi
      done

    fi
  done

  for option in "${INSTALLED[@]}" ; do
    COMMAND="${COMMAND} $option"
  done

  ${COMMAND}

}
