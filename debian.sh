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

verify_enable(){
  feature="$1"
  feature_status=${!1}
  verify_gcc_enable $feature
}
add_os_flags() {
  CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DFAIL_ON_WARNINGS= "
}
bootstrap_cmake(){
  sudo apt-get -y install cmake
}
build_deps(){
  sudo apt-get -y update
  ## need to account for debian
  sudo apt-get install -y libssl1.0-dev > /dev/null
  RETVAL=$?
  if [ "$RETVAL" -ne "0" ]; then
    sudo apt-get install -y libssl-dev > /dev/null
  fi
  COMMAND="sudo apt-get -y install cmake gcc g++ zlib1g-dev uuid uuid-dev"
  export DEBIAN_FRONTEND=noninteractive
  INSTALLED=()
  INSTALLED+=("libbz2-dev")
  sudo apt-get -y update
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
            INSTALLED+=("libcurl4-openssl-dev")
          elif [ "$FOUND_VALUE" = "libpcap" ]; then
            INSTALLED+=("libpcap-dev")
          elif [ "$FOUND_VALUE" = "openssl" ]; then
            INSTALLED+=("openssl")
          elif [ "$FOUND_VALUE" = "libusb" ]; then
            INSTALLED+=("libusb-1.0-0-dev")
            INSTALLED+=("libusb-dev")
          elif [ "$FOUND_VALUE" = "libpng" ]; then
            INSTALLED+=("libpng-dev")
          elif [ "$FOUND_VALUE" = "bison" ]; then
            INSTALLED+=("bison")
          elif [ "$FOUND_VALUE" = "flex" ]; then
            INSTALLED+=("flex")
	  elif [ "$FOUND_VALUE" = "libtool" ]; then
            INSTALLED+=("libtool")
          elif [ "$FOUND_VALUE" = "python" ]; then
            INSTALLED+=("libpython3-dev")
          elif [ "$FOUND_VALUE" = "lua" ]; then
            INSTALLED+=("liblua5.1-0-dev")
          elif [ "$FOUND_VALUE" = "automake" ]; then
            INSTALLED+=("automake")
          elif [ "$FOUND_VALUE" = "gpsd" ]; then
            INSTALLED+=("libgps-dev")
          elif [ "$FOUND_VALUE" = "libarchive" ]; then
            INSTALLED+=("liblzma-dev")
          fi
        fi
      done

    fi
  done
  
  INSTALLED+=("autoconf")

  for option in "${INSTALLED[@]}" ; do
    COMMAND="${COMMAND} $option"
  done

  echo "Ensuring you have all dependencies installed..."
  ${COMMAND}

}
