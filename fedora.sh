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
  :
}
bootstrap_cmake(){
  sudo yum -y install wget
  wget https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
  sudo yum -y install epel-release-latest-7.noarch.rpm
  sudo yum -y install cmake3
}
build_deps(){
# Install epel-release so that cmake3 will be available for installation
  sudo yum -y install wget
  wget https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
  sudo yum -y install epel-release-latest-7.noarch.rpm

  COMMAND="sudo yum -y install gcc gcc-c++ libuuid libuuid-devel"
  INSTALLED=()
  INSTALLED+=("bzip2-devel")
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
            INSTALLED+=("bison")
          elif [ "$FOUND_VALUE" = "flex" ]; then
            INSTALLED+=("flex")
          elif [ "$FOUND_VALUE" = "python" ]; then
            INSTALLED+=("python3-devel")
          elif [ "$FOUND_VALUE" = "lua" ]; then
            INSTALLED+=("lua-devel")
	  elif [ "$FOUND_VALUE" = "libtool" ]; then
            INSTALLED+=("libtool")
          elif [ "$FOUND_VALUE" = "autoconf" ]; then
            INSTALLED+=("autoconf")
          elif [ "$FOUND_VALUE" = "automake" ]; then
	    INSTALLED+=("automake")
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
