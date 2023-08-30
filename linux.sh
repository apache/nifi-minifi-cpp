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
verify_gcc_enable(){
  #feature="$1"
  [ "$COMPILER_MAJOR" -ge 11 ] && echo true || echo false
}

install_cmake_from_binary() {
  CMAKE_VERSION="3.24.4"
  arch=$(uname -m)
  if [ "$arch" = "x86_64" ]; then
      CMAKE_URL="https://cmake.org/files/v3.24/cmake-3.24.4-linux-x86_64.tar.gz"
      EXPECTED_SHA256="cac77d28fb8668c179ac02c283b058aeb846fe2133a57d40b503711281ed9f19"
  elif [ "$arch" = "aarch64" ]; then
      CMAKE_URL="https://cmake.org/files/v3.24/cmake-3.24.4-linux-aarch64.tar.gz"
      EXPECTED_SHA256="86f823f2636bf715af89da10e04daa476755a799d451baee66247846e95d7bee"
  else
      echo "Unknown architecture: $arch"
      exit 1
  fi

  TMP_DIR=$(mktemp -d)

  install_pkgs wget
  wget -P "$TMP_DIR" "$CMAKE_URL"

  ACTUAL_SHA256=$(sha256sum "$TMP_DIR/cmake-$CMAKE_VERSION-linux-$arch.tar.gz" | cut -d " " -f 1)

  if [ "$ACTUAL_SHA256" != "$EXPECTED_SHA256" ]; then
    echo "ERROR: SHA-256 verification failed. Aborting."
    rm -r "$TMP_DIR"
    exit 1
  fi

  echo "Installing CMake $CMAKE_VERSION to /opt/cmake-$CMAKE_VERSION..."
  set -x
  tar -C "$TMP_DIR" -zxf "$TMP_DIR/cmake-$CMAKE_VERSION-linux-$arch.tar.gz"
  sudo mv "$TMP_DIR/cmake-$CMAKE_VERSION-linux-$arch" /opt/cmake-$CMAKE_VERSION

  sudo ln -s "/opt/cmake-$CMAKE_VERSION/bin/cmake" /usr/local/bin/cmake
  set +x

  echo "CMake $CMAKE_VERSION has been installed successfully."
  rm -r "$TMP_DIR"
}
