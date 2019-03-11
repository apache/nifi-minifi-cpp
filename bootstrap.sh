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

script_directory="$(cd "$(dirname "$0")" && pwd)"

#RED='\033[0;41;30m'
RED='\033[0;101m'
NO_COLOR='\033[0;0;39m'
CORES=1
BUILD="false"
PACKAGE="false"
BUILD_IDENTIFIER=""
TRUE="Enabled"
FALSE="Disabled"
FEATURES_SELECTED="false"
AUTO_REMOVE_EXTENSIONS="true"
export NO_PROMPT="false"
ALL_FEATURES_ENABLED=${FALSE}
BUILD_DIR="build"

DEPLOY="false"
OPTIONS=()
CMAKE_OPTIONS_ENABLED=()
CMAKE_OPTIONS_DISABLED=()
CMAKE_MIN_VERSION=()
DEPLOY_LIMITS=()
USER_DISABLE_TESTS="${FALSE}"

DEPENDENCIES=()

. "${script_directory}/bstrp_functions.sh"

MENU="features"
GUIDED_INSTALL=${FALSE}
while :; do
  case $1 in
    -n|--noprompt)
      NO_PROMPT="true"
      ;;
    -s|--skiptests)
      USER_DISABLE_TESTS="${TRUE}"
      ;;
    -e|--enableall)
      NO_PROMPT="true"
      FEATURES_SELECTED="true"
      EnableAllFeatures
      ;;
    -c|--clear)
      rm ${script_directory}/bt_state > /dev/null 2>&1
      ;;
    -d|--deploy)
      NO_PROMPT="true"
      DEPLOY="true"
      FEATURES_SELECTED="true"
      EnableAllFeatures
      ;;
    "--build_dir="* )
      BUILD_DIR="${1#*=}"
      ;;
    -t|--travis)
      NO_PROMPT="true"
      FEATURES_SELECTED="true"
      ;;
    -p|--package)
      CORES=$(grep -c ^processor /proc/cpuinfo 2>/dev/null || sysctl -n hw.ncpu)
      BUILD="true"
      PACKAGE="true"
      ;;
    -i|--install)
      GUIDED_INSTALL="Enabled"
      EnableAllFeatures
      MENU="main"
      ALL_FEATURES_ENABLED=${TRUE}
      ;;
    -b|--build)
      CORES=$(grep -c ^processor /proc/cpuinfo 2>/dev/null || sysctl -n hw.ncpu)
      BUILD="true"
      ;;
    "--build_identifier="* )
      BUILD_IDENTIFIER="${1#*=}"
      ;;
    *) break
  esac
  shift
done

if [ -x "$(command -v hostname)" ]; then
  HOSTNAME=`hostname`
  PING_RESULT=`ping -c 1 ${HOSTNAME} 2>&1`
  if [[ "$PING_RESULT" = *unknown* ]]; then
    cntinu="N"
    read -p "Cannot resolve your host name -- ${HOSTNAME} -- tests may fail, Continue?  [ Y/N ] " cntinu
    if [ "$cntinu" = "Y" ] || [ "$cntinu" = "y" ]; then
      echo "Continuing..."
    else
      exit
    fi
  fi
fi


if [ "$NO_PROMPT" = "true" ]; then
  agree="N"
  echo "****************************************"
  echo "Welcome, this bootstrap script will update your system to install MiNiFi C++"
  echo "You have opted to skip prompts. "
fi



if [ -f /etc/os-release ]; then
  . /etc/os-release
  OS=$NAME
  VER=$VERSION_ID
elif type lsb_release >/dev/null 2>&1; then
  OS=$(lsb_release -si)
  VER=$(lsb_release -sr)
elif [ -f /etc/lsb-release ]; then
  . /etc/lsb-release
  OS=$DISTRIB_ID
  VER=$DISTRIB_RELEASE
elif [ -f /etc/debian_version ]; then
  OS=Debian
  VER=$(cat /etc/debian_version)
elif [ -f /etc/SUSE-brand ]; then
  VER=`cat /etc/SUSE-brand | tr '\n' ' ' | sed s/.*=\ //`
  OS=`cat /etc/SUSE-brand | tr '\n' ' ' | sed s/VERSION.*//`
elif [ -f /etc/SUSE-release ]; then
  VER=`cat /etc/SUSE-release | tr '\n' ' ' | sed s/.*=\ //`
  OS=`cat /etc/SUSE-release | tr '\n' ' ' | sed s/VERSION.*//`
elif [ -f /etc/redhat-release ]; then
  # Older Red Hat, CentOS, etc.
  ...
else
  OS=$(uname -s)
  VER=$(uname -r)
fi
OS_MAJOR=`echo $VER | cut -d. -f1`
OS_MINOR=`echo $VER | cut -d. -f2`
OS_REVISION=`echo $EVR	 | cut -d. -f3`

### Verify the compiler version

COMPILER_VERSION="0.0.0"

COMPILER_COMMAND=""

if [ -x "$(command -v g++)" ]; then
  COMPILER_COMMAND="g++"
  COMPILER_VERSION=`${COMPILER_COMMAND} -dumpversion`
fi

COMPILER_MAJOR=`echo $COMPILER_VERSION | cut -d. -f1`
COMPILER_MINOR=`echo $COMPILER_VERSION | cut -d. -f2`
COMPILER_REVISION=`echo $COMPILER_VERSION | cut -d. -f3`


if [[ "$OS" = "Darwin" ]]; then
  . "${script_directory}/darwin.sh"
else
  . "${script_directory}/linux.sh"
  if [[ "$OS" = Deb* ]]; then
    . "${script_directory}/debian.sh"
  elif [[ "$OS" = Rasp* ]]; then
    . "${script_directory}/aptitude.sh"
  elif [[ "$OS" = Pop* ]]; then
    . "${script_directory}/aptitude.sh"
  elif [[ "$OS" = Ubuntu* ]]; then
    . "${script_directory}/aptitude.sh"
  elif [[ "$OS" = *SUSE* ]]; then
    . "${script_directory}/suse.sh"
  elif [[ "$OS" = *SLE* ]]; then
    if [[ "$VER" = 11* ]]; then
      echo "Please install SLES11 manually...exiting"
      exit
    else
      . "${script_directory}/suse.sh"
    fi
  elif [[ "$OS" = Red* ]]; then
    . "${script_directory}/rheldistro.sh"
  elif [[ "$OS" = Amazon* ]]; then
    . "${script_directory}/centos.sh"
  elif [[ "$OS" = CentOS* ]]; then
    . "${script_directory}/centos.sh"
  elif [[ "$OS" = Fedora* ]]; then
    . "${script_directory}/fedora.sh"
  fi
fi
### verify the cmake version

CMAKE_COMMAND=""

if [ -x "$(command -v cmake3)" ]; then
  CMAKE_COMMAND="cmake3"
elif [ -x "$(command -v cmake)" ]; then
  CMAKE_COMMAND="cmake"
fi

if [ -z "${CMAKE_COMMAND}" ]; then
  echo "CMAKE is not installed, attempting to install it..."
  bootstrap_cmake
  if [ -x "$(command -v cmake3)" ]; then
    CMAKE_COMMAND="cmake3"
  elif [ -x "$(command -v cmake)" ]; then
    CMAKE_COMMAND="cmake"
  fi
fi


## before we begin, let's ensure that cmake exists

CMAKE_VERSION=`${CMAKE_COMMAND} --version | head -n 1 | awk '{print $3}'`

CMAKE_MAJOR=`echo $CMAKE_VERSION | cut -d. -f1`
CMAKE_MINOR=`echo $CMAKE_VERSION | cut -d. -f2`
CMAKE_REVISION=`echo $CMAKE_VERSION | cut -d. -f3`



add_cmake_option PORTABLE_BUILD ${TRUE}
add_cmake_option DEBUG_SYMBOLS ${FALSE}
add_cmake_option BUILD_ROCKSDB ${TRUE}
## uses the source from the third party directory
add_enabled_option ROCKSDB_ENABLED ${TRUE} "DISABLE_ROCKSDB"
## need libcurl installed
add_enabled_option HTTP_CURL_ENABLED ${TRUE} "DISABLE_CURL"
add_dependency HTTP_CURL_ENABLED "libcurl"
add_dependency HTTP_CURL_ENABLED "openssl"

# third party directory
add_enabled_option LIBARCHIVE_ENABLED ${TRUE} "DISABLE_LIBARCHIVE"
add_dependency LIBARCHIVE_ENABLED "libarchive"

add_enabled_option EXECUTE_SCRIPT_ENABLED ${TRUE} "DISABLE_SCRIPTING"
add_dependency EXECUTE_SCRIPT_ENABLED "python"
add_dependency EXECUTE_SCRIPT_ENABLED "lua"

add_enabled_option EXPRESSION_LANGAUGE_ENABLED ${TRUE} "DISABLE_EXPRESSION_LANGUAGE"
add_dependency EXPRESSION_LANGAUGE_ENABLED "bison"
add_dependency EXPRESSION_LANGAUGE_ENABLED "flex"

add_disabled_option PCAP_ENABLED ${FALSE} "ENABLE_PCAP"
add_dependency PCAP_ENABLED "libpcap"

add_disabled_option USB_ENABLED ${FALSE} "ENABLE_USB_CAMERA"
add_dependency USB_ENABLED "libusb"
add_dependency USB_ENABLED "libpng"

add_disabled_option GPS_ENABLED ${FALSE} "ENABLE_GPS"
add_dependency GPS_ENABLED "gpsd"

add_disabled_option KAFKA_ENABLED ${FALSE} "ENABLE_LIBRDKAFKA" "3.4.0"

add_disabled_option MQTT_ENABLED ${FALSE} "ENABLE_MQTT"

add_disabled_option PYTHON_ENABLED ${FALSE} "ENABLE_PYTHON"
add_dependency PYTHON_ENABLED "python"

add_disabled_option COAP_ENABLED ${TRUE} "ENABLE_COAP"
add_dependency COAP_ENABLED "automake"
add_dependency COAP_ENABLED "autoconf"
add_dependency COAP_ENABLED "libtool"

add_disabled_option JNI_ENABLED ${FALSE} "ENABLE_JNI"

TESTS_DISABLED=${FALSE}

add_disabled_option SQLITE_ENABLED ${FALSE} "ENABLE_SQLITE"

USE_SHARED_LIBS=${TRUE} 


# Since the following extensions have limitations on

add_disabled_option BUSTACHE_ENABLED ${FALSE} "ENABLE_BUSTACHE" "2.6" ${TRUE}
add_dependency BUSTACHE_ENABLED "boost"
## currently need to limit on certain platforms
add_disabled_option TENSORFLOW_ENABLED ${FALSE} "ENABLE_TENSORFLOW" "2.6" ${TRUE}
add_dependency TENSORFLOW_ENABLED "tensorflow"

if [ "$GUIDED_INSTALL" == "${TRUE}" ]; then
  EnableAllFeatures
  ALL_FEATURES_ENABLED=${TRUE}
fi

BUILD_DIR_D=${BUILD_DIR}
OVERRIDE_BUILD_IDENTIFIER=${BUILD_IDENTIFIER}

load_state

if [ "$USER_DISABLE_TESTS" == "${TRUE}" ]; then
   ToggleFeature TESTS_DISABLED
fi


if [ "${OVERRIDE_BUILD_IDENTIFIER}" != "${BUILD_IDENTIFIER}" ]; then
  BUILD_IDENTIFIER=${OVERRIDE_BUILD_IDENTIFIER}
fi

if [ "$BUILD_DIR_D" != "build" ] && [ "$BUILD_DIR_D" != "$BUILD_DIR" ]; then
  read -p "Build dir will override stored state, $BUILD_DIR. Press any key to continue " overwrite
  BUILD_DIR=$BUILD_DIR_D

fi

if [ ! -d "${BUILD_DIR}" ]; then
  mkdir ${BUILD_DIR}/
else

  overwrite="Y"
  if [ "$NO_PROMPT" = "false" ] && [ "$FEATURES_SELECTED" = "false" ]; then
    echo "CMAKE Build dir (${BUILD_DIR}) exists, should we overwrite your build directory before we begin?"
    read -p "If you have already bootstrapped, bootstrapping again isn't necessary to run make [ Y/N ] " overwrite
  fi
  if [ "$overwrite" = "N" ] || [ "$overwrite" = "n" ]; then
    echo "Exiting ...."
    exit
  else
    rm ${BUILD_DIR}/CMakeCache.txt > /dev/null 2>&1
  fi
fi

## change to the directory


pushd ${BUILD_DIR}

while [ ! "$FEATURES_SELECTED" == "true" ]
do
  if [ "$MENU"  == "main" ]; then
    show_main_menu
    read_main_menu_options
  elif [ "$MENU" == "advanced" ]; then
    show_advanced_features_menu
    read_advanced_menu_options
  else
    show_supported_features
    read_feature_options
  fi
done
### ensure we have all dependencies

save_state

build_deps


## just in case

CMAKE_VERSION=`${CMAKE_COMMAND} --version | head -n 1 | awk '{print $3}'`

CMAKE_MAJOR=`echo $CMAKE_VERSION | cut -d. -f1`
CMAKE_MINOR=`echo $CMAKE_VERSION | cut -d. -f2`
CMAKE_REVISION=`echo $CMAKE_VERSION | cut -d. -f3`


CMAKE_BUILD_COMMAND="${CMAKE_COMMAND} "

build_cmake_command(){

  for option in "${OPTIONS[@]}" ; do
    option_value="${!option}"
    if [ "$option_value" = "${TRUE}" ]; then
      # option is enabled
      FOUND=""
      FOUND_VALUE=""
      for cmake_opt in "${CMAKE_OPTIONS_ENABLED[@]}" ; do
        KEY=${cmake_opt%%:*}
        VALUE=${cmake_opt#*:}
        if [ "$KEY" = "$option" ]; then
          FOUND="1"
          FOUND_VALUE="$VALUE"
        fi
      done
      if [ "$FOUND" = "1" ]; then
        CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -D${FOUND_VALUE}=ON"
      fi
    else
      FOUND=""
      FOUND_VALUE=""
      if [ -z "$FOUND" ]; then
        for cmake_opt in "${CMAKE_OPTIONS_DISABLED[@]}" ; do
          KEY=${cmake_opt%%:*}
          VALUE=${cmake_opt#*:}
          if [ "$KEY" = "$option" ]; then
            FOUND="1"
            FOUND_VALUE="$VALUE"
          fi
        done
      fi
      if [ "$FOUND" = "1" ]; then
        CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -D${FOUND_VALUE}=ON"
      fi
    fi
  done
  if [ "${DEBUG_SYMBOLS}" = "${TRUE}" ]; then
    CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DCMAKE_BUILD_TYPE=RelWithDebInfo"
  fi

  if [ "${TESTS_DISABLED}" = "${TRUE}" ]; then
    CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DSKIP_TESTS=true "
  else
    # user may have disabled tests previously, so let's force them to be re-enabled
    CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DSKIP_TESTS= "
  fi
  
  if [ "${USE_SHARED_LIBS}" = "${TRUE}" ]; then
    CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DUSE_SHARED_LIBS=ON "
  else
    CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DUSE_SHARED_LIBS= "
  fi
  


  if [ "${PORTABLE_BUILD}" = "${TRUE}" ]; then
    CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DPORTABLE=ON "
  else
    CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DPORTABLE=OFF "
  fi

  if [ "${BUILD_ROCKSDB}" = "${TRUE}" ]; then
    CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DBUILD_ROCKSDB=ON "
  else
    CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DBUILD_ROCKSDB= "
  fi

  CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DBUILD_IDENTIFIER=${BUILD_IDENTIFIER}"

  add_os_flags

  curl -V | grep OpenSSL &> /dev/null
  if [ $? == 0 ]; then
    echo "Using libcurl-openssl..."
  else
    CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DUSE_CURL_NSS=true .."
  fi

  CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} .."

  continue_with_plan="Y"
  if [ ! "$NO_PROMPT" = "true" ]; then
    read -p "Command will be '${CMAKE_BUILD_COMMAND}', run this? [ Y/N ] " continue_with_plan
  fi
  if [ "$continue_with_plan" = "N" ] || [ "$continue_with_plan" = "n" ]; then
    echo "Exiting ...."
    exit
  fi
}


build_cmake_command

### run the cmake command
${CMAKE_BUILD_COMMAND}

if [ "$BUILD" = "true" ]; then
  make -j${CORES}
fi

if [ "$PACKAGE" = "true" ]; then
  make package
fi


popd

