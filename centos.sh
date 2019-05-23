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
        elif [ "$feature" = "TENSORFLOW_ENABLED" ]; then
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
    if [ "$OS_MAJOR" = "6" ]; then
        CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DCMAKE_CXX_FLAGS=-lrt "
    fi
}
install_bison() {
    if [ "$OS_MAJOR" = "6" ]; then
        BISON_INSTALLED="false"
        if [ -x "$(command -v bison)" ]; then
            BISON_VERSION=`bison --version | head -n 1 | awk '{print $4}'`
            BISON_MAJOR=`echo $BISON_VERSION | cut -d. -f1`
            if (( BISON_MAJOR >= 3 )); then
                BISON_INSTALLED="true"
            fi
        fi
        if [ "$BISON_INSTALLED" = "false" ]; then
            wget https://ftp.gnu.org/gnu/bison/bison-3.0.4.tar.xz
            tar xvf bison-3.0.4.tar.xz
            pushd bison-3.0.4
            ./configure
            make
            make install
            popd
        fi

    else
        INSTALLED+=("bison")
    fi
}

install_libusb() {
    if [ "$OS_MAJOR" = "6" ]; then
        sudo yum -y install libtool libudev-devel patch
        #	git clone --branch v1.0.18 https://github.com/libusb/libusb.git
        git clone https://github.com/libusb/libusb.git
        pushd libusb
        git checkout v1.0.18
        ./bootstrap.sh
        ./configure
        make
        sudo make install
        popd
        rm -rf libusb
    else
        INSTALLED+=("libusb-devel")
    fi

}



bootstrap_cmake(){
    sudo yum -y install wget
    if [ "$OS_MAJOR" = "6" ]; then
        sudo yum -y install centos-release-scl
        sudo yum -y install devtoolset-6
        sudo yum -y install epel-release
        sudo yum -y install cmake3
        if [ "$NO_PROMPT" = "true" ]; then
            scl enable devtoolset-6 bash ./bootstrap.sh -n
        else
            scl enable devtoolset-6 bash ./bootstrap.sh
        fi
    else
        wget https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
        sudo yum -y install epel-release-latest-7.noarch.rpm
        sudo yum -y install cmake3
    fi
}
build_deps(){
    # Install epel-release so that cmake3 will be available for installation

    COMMAND="sudo yum -y install gcc gcc-c++ libuuid libuuid-devel libtool patch"
    INSTALLED=()
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
                        INSTALLED+=("openssl-static")
                    elif [ "$FOUND_VALUE" = "libusb" ]; then
                        install_libusb
                    elif [ "$FOUND_VALUE" = "libpng" ]; then
                        INSTALLED+=("libpng-devel")
                    elif [ "$FOUND_VALUE" = "bison" ]; then
                        install_bison
                    elif [ "$FOUND_VALUE" = "flex" ]; then
                        INSTALLED+=("flex")
                    elif [ "$FOUND_VALUE" = "automake" ]; then
                        INSTALLED+=("automake")
                    elif [ "$FOUND_VALUE" = "jnibuild" ]; then
                        INSTALLED+=("java-1.8.0-openjdk")
                        INSTALLED+=("java-1.8.0-openjdk-devel")
                        INSTALLED+=("maven")
                    elif [ "$FOUND_VALUE" = "python" ]; then
                        INSTALLED+=("python34-devel")
                    elif [ "$FOUND_VALUE" = "lua" ]; then
                        INSTALLED+=("lua-devel")
                    elif [ "$FOUND_VALUE" = "gpsd" ]; then
                        INSTALLED+=("gpsd-devel")
                    elif [ "$FOUND_VALUE" = "libarchive" ]; then
                        INSTALLED+=("xz-devel")
                        INSTALLED+=("bzip2-devel")
                    elif [ "$FOUND_VALUE" = "libssh2" ]; then
                        INSTALLED+=("libssh2-devel")
                    fi
                fi
            done

        fi
    done
    INSTALLED+=("autoconf")
    for option in "${INSTALLED[@]}" ; do
        COMMAND="${COMMAND} $option"
    done

    ${COMMAND}

}
