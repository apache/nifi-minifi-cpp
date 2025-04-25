#!/usr/bin/env bash
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

CMAKE_GLOBAL_MIN_VERSION_MAJOR=3
CMAKE_GLOBAL_MIN_VERSION_MINOR=24
CMAKE_GLOBAL_MIN_VERSION_REVISION=0

export RED='\033[0;101m'
export NO_COLOR='\033[0;0;39m'
export TRUE="Enabled"
export FALSE="Disabled"
export DEPLOY="false"
export NO_PROMPT="false"
export FEATURES_SELECTED="false"
export ALL_FEATURES_ENABLED=${FALSE}
export BUILD_DIR="build"
export BUILD_IDENTIFIER=""
export OPTIONS=()
export CMAKE_OPTIONS=()
export CMAKE_MIN_VERSION=()
export INCOMPATIBLE_WITH=()
export DEPLOY_LIMITS=()
export DEPENDENCIES=()
export DEPENDS_ON=()

CORES=1
BUILD="false"
PACKAGE="false"
USER_DISABLE_TESTS="${FALSE}"
USE_NINJA="false"

. "${script_directory}/bstrp_functions.sh"
SKIP_CMAKE=${FALSE}
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
    -g|--useninja)
      USE_NINJA="${TRUE}"
      ;;
    -e|--enableall)
      NO_PROMPT="true"
      FEATURES_SELECTED="true"
      EnableAllFeatures
      ;;
    -c|--clear)
      rm "${script_directory}/bt_state" > /dev/null 2>&1
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
    -t|--continous-integration)
      NO_PROMPT="true"
      FEATURES_SELECTED="true"
      SKIP_CMAKE="${TRUE}"
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
  HOSTNAME=$(hostname)
  PING_RESULT=$(ping -c 1 "${HOSTNAME}" 2>&1)
  if [[ "$PING_RESULT" = *unknown* ]]; then
    cntinu="N"
    read -r -p "Cannot resolve your host name -- ${HOSTNAME} -- tests may fail, Continue?  [ Y/N ] " cntinu
    if [ "$cntinu" = "Y" ] || [ "$cntinu" = "y" ]; then
      echo "Continuing..."
    else
      exit
    fi
  fi
fi


if [ "$NO_PROMPT" = "true" ]; then
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
  VER=$(tr '\n' ' ' < /etc/SUSE-brand | sed s/.*=\ //)
  OS=$(tr '\n' ' ' < /etc/SUSE-brand | sed s/VERSION.*//)
elif [ -f /etc/SUSE-release ]; then
  VER=$(tr '\n' ' ' < /etc/SUSE-release | sed s/.*=\ //)
  OS=$(tr '\n' ' ' < /etc/SUSE-release | sed s/VERSION.*//)
elif [ -f /etc/redhat-release ]; then
  # Older Red Hat, CentOS, etc.
  echo "Unsupported old release!"
else
  OS=$(uname -s)
  VER=$(uname -r)
fi
OS_MAJOR=$(echo "$VER" | cut -d. -f1)
export OS_MAJOR
OS_MINOR=$(echo "$VER" | cut -d. -f2)
export OS_MINOR
OS_REVISION=$(echo "$EVR" | cut -d. -f3)
export OS_REVISION

if [[ "$OSTYPE" =~ .*linux.* ]]; then
  LINUX=true
else
  LINUX=false
fi

if [[ "$OS" = "Darwin" ]]; then
  . "${script_directory}/darwin.sh"
else
  . "${script_directory}/linux.sh"
  if [[ "$OS" = Deb* ]]; then
    . "${script_directory}/debian.sh"
  elif [[ "$OS" = Ubuntu* || "$OS" = Rasp* || "$OS" = Pop* ]]; then
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
  elif [[ "$OS" = Amazon* || "$OS" = Alma* || "$OS" = Rocky* || "$OS" = CentOS* ]]; then
    . "${script_directory}/centos.sh"
  elif [[ "$OS" = Fedora* ]]; then
    . "${script_directory}/fedora.sh"
  elif [[ "$OS" = Arch* || "$OS" = Manjaro* ]]; then
    . "${script_directory}/arch.sh"
  fi
fi

### Verify the compiler version
COMPILER_VERSION="0.0.0"
COMPILER_COMMAND="${CXX:-g++}"

if [ -z "$(command -v "$COMPILER_COMMAND")" ]; then
  echo "Couldn't find compiler, attempting to install GCC"
  bootstrap_compiler
fi
if [ -x "$(command -v "${COMPILER_COMMAND}")" ]; then
  COMPILER_VERSION=$(${COMPILER_COMMAND} -dumpversion)
else
  echo "Couldn't find compiler, please install one, or set CXX appropriately" 1>&2
  exit 1
fi

COMPILER_MAJOR=$(echo "$COMPILER_VERSION" | cut -d. -f1)
export COMPILER_MAJOR
COMPILER_MINOR=$(echo "$COMPILER_VERSION" | cut -d. -f2)
export COMPILER_MINOR
COMPILER_REVISION=$(echo "$COMPILER_VERSION" | cut -d. -f3)
export COMPILER_REVISION

### verify the cmake version
CMAKE_COMMAND=""
if [ -x "$(command -v cmake3)" ]; then
  CMAKE_COMMAND="cmake3"
elif [ -x "$(command -v cmake)" ]; then
  CMAKE_COMMAND="cmake"
fi

if [ -n "${CMAKE_COMMAND}" ]; then
  get_cmake_version
fi

if [ -z "${CMAKE_COMMAND}" ] ||
  version_is_less_than "$CMAKE_MAJOR" "$CMAKE_MINOR" "$CMAKE_REVISION" "$CMAKE_GLOBAL_MIN_VERSION_MAJOR" "$CMAKE_GLOBAL_MIN_VERSION_MINOR" "$CMAKE_GLOBAL_MIN_VERSION_REVISION"; then
  echo "CMake is not installed or too old, attempting to install it..."
  bootstrap_cmake
  if [ -x "$(command -v cmake3)" ]; then
    CMAKE_COMMAND="cmake3"
  elif [ -x "$(command -v cmake)" ]; then
    CMAKE_COMMAND="cmake"
  fi

  get_cmake_version
fi

# RHEL8: If cmake3 is too old, try cmake
if [ "$CMAKE_COMMAND" = "cmake3" ] &&
   version_is_less_than "$CMAKE_MAJOR" "$CMAKE_MINOR" "$CMAKE_REVISION" "$CMAKE_GLOBAL_MIN_VERSION_MAJOR" "$CMAKE_GLOBAL_MIN_VERSION_MINOR" "$CMAKE_GLOBAL_MIN_VERSION_REVISION" &&
   [ -x "$(command -v cmake)" ]; then
  CMAKE_COMMAND="cmake"
  get_cmake_version
fi

if version_is_less_than "$CMAKE_MAJOR" "$CMAKE_MINOR" "$CMAKE_REVISION" "$CMAKE_GLOBAL_MIN_VERSION_MAJOR" "$CMAKE_GLOBAL_MIN_VERSION_MINOR" "$CMAKE_GLOBAL_MIN_VERSION_REVISION"; then
  echo "Failed to install or update CMake, exiting..."
  exit
fi


add_cmake_option PORTABLE_BUILD ${TRUE}
add_cmake_option DEBUG_SYMBOLS ${FALSE}
add_cmake_option BUILD_ROCKSDB ${TRUE}
## uses the source from the third party directory
add_option ROCKSDB_ENABLED ${TRUE} "ENABLE_ROCKSDB"

# third party directory
add_option LIBARCHIVE_ENABLED ${TRUE} "ENABLE_LIBARCHIVE"
add_dependency LIBARCHIVE_ENABLED "libarchive"

add_option PYTHON_SCRIPTING_ENABLED ${TRUE} "ENABLE_PYTHON_SCRIPTING"
add_dependency PYTHON_SCRIPTING_ENABLED "python"
add_option LUA_SCRIPTING_ENABLED ${TRUE} "ENABLE_LUA_SCRIPTING"
add_dependency LUA_SCRIPTING_ENABLED "lua"

add_option EXPRESSION_LANGUAGE_ENABLED ${TRUE} "ENABLE_EXPRESSION_LANGUAGE"
add_dependency EXPRESSION_LANGUAGE_ENABLED "bison"
add_dependency EXPRESSION_LANGUAGE_ENABLED "flex"

add_option AWS_ENABLED ${TRUE} "ENABLE_AWS"

add_option KAFKA_ENABLED ${TRUE} "ENABLE_KAFKA"

add_option KUBERNETES_ENABLED ${TRUE} "ENABLE_KUBERNETES"

add_option MQTT_ENABLED ${TRUE} "ENABLE_MQTT"

add_option OPENCV_ENABLED ${FALSE} "ENABLE_OPENCV"

add_option SFTP_ENABLED ${FALSE} "ENABLE_SFTP"
add_dependency SFTP_ENABLED "libssh2"

add_option SQL_ENABLED ${TRUE} "ENABLE_SQL"

# Since the following extensions have limitations on
add_option BUSTACHE_ENABLED ${FALSE} "ENABLE_BUSTACHE" "2.6" ${TRUE}

add_option OPC_ENABLED ${TRUE} "ENABLE_OPC"
add_dependency OPC_ENABLED "mbedtls"

add_option AZURE_ENABLED ${TRUE} "ENABLE_AZURE"

if $LINUX; then
  add_option SYSTEMD_ENABLED ${TRUE} "ENABLE_SYSTEMD"
fi

add_option SPLUNK_ENABLED ${TRUE} "ENABLE_SPLUNK"

add_option GCP_ENABLED ${TRUE} "ENABLE_GCP"

add_option ELASTIC_ENABLED ${TRUE} "ENABLE_ELASTICSEARCH"

add_option GRAFANA_LOKI_ENABLED ${FALSE} "ENABLE_GRAFANA_LOKI"

add_option PROCFS_ENABLED ${TRUE} "ENABLE_PROCFS"

add_option PROMETHEUS_ENABLED ${TRUE} "ENABLE_PROMETHEUS"

add_option COUCHBASE_ENABLED ${FALSE} "ENABLE_COUCHBASE"

add_option LLAMACPP_ENABLED ${FALSE} "ENABLE_LLAMACPP"

USE_SHARED_LIBS=${TRUE}
ASAN_ENABLED=${FALSE}
MINIFI_FAIL_ON_WARNINGS=${FALSE}
TESTS_ENABLED=${TRUE}

## name, default, values
add_multi_option BUILD_PROFILE "RelWithDebInfo" "RelWithDebInfo" "Debug" "MinSizeRel" "Release"

if [ "$GUIDED_INSTALL" == "${TRUE}" ]; then
  EnableAllFeatures
  ALL_FEATURES_ENABLED=${TRUE}
fi

BUILD_DIR_D=${BUILD_DIR}
OVERRIDE_BUILD_IDENTIFIER=${BUILD_IDENTIFIER}

load_state

if [ "$USER_DISABLE_TESTS" == "${TRUE}" ]; then
   ToggleFeature TESTS_ENABLED
fi


if [ "${OVERRIDE_BUILD_IDENTIFIER}" != "${BUILD_IDENTIFIER}" ]; then
  BUILD_IDENTIFIER=${OVERRIDE_BUILD_IDENTIFIER}
fi

if [ "$BUILD_DIR_D" != "build" ] && [ "$BUILD_DIR_D" != "$BUILD_DIR" ]; then
  echo -n "Build dir will override stored state, $BUILD_DIR. Press any key to continue "
  read -r overwrite
  BUILD_DIR=$BUILD_DIR_D

fi

if [ ! -d "${BUILD_DIR}" ]; then
  mkdir "${BUILD_DIR}/"
else

  overwrite="Y"
  if [ "$NO_PROMPT" = "false" ] && [ "$FEATURES_SELECTED" = "false" ]; then
    echo "CMAKE Build dir (${BUILD_DIR}) exists, should we overwrite your build directory before we begin?"
    echo -n "If you have already bootstrapped, bootstrapping again isn't necessary to run make [ Y/N ] "
    read -r overwrite
  fi
  if [ "$overwrite" = "N" ] || [ "$overwrite" = "n" ]; then
    echo "Exiting ...."
    exit
  else
    rm "${BUILD_DIR}/CMakeCache.txt" > /dev/null 2>&1
  fi
fi

## change to the directory
pushd "${BUILD_DIR}" || exit 1

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
CMAKE_VERSION=$(${CMAKE_COMMAND} --version | head -n 1 | awk '{print $3}')

CMAKE_MAJOR=$(echo "$CMAKE_VERSION" | cut -d. -f1)
CMAKE_MINOR=$(echo "$CMAKE_VERSION" | cut -d. -f2)
CMAKE_REVISION=$(echo "$CMAKE_VERSION" | cut -d. -f3)


CMAKE_BUILD_COMMAND="${CMAKE_COMMAND} "

if [ "${USE_NINJA}" = "${TRUE}" ]; then
	 echo "use ninja"
   CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DFORCE_COLORED_OUTPUT=ON -GNinja "
fi

build_cmake_command(){
  for option in "${OPTIONS[@]}" ; do
    for cmake_opt in "${CMAKE_OPTIONS[@]}" ; do
      KEY=${cmake_opt%%:*}
      VALUE=${cmake_opt#*:}
      if [ "$KEY" = "$option" ]; then
        FOUND="1"
        FOUND_VALUE="$VALUE"
      fi
    done
    if [ "$FOUND" = "1" ]; then
      set_value=OFF
      option_value="${!option}"
      if { [[ "$option_value" = "${FALSE}" ]] && [[ "$FOUND_VALUE" == "DISABLE"* ]]; } || \
         { [[ "$option_value" = "${TRUE}" ]] && [[ "$FOUND_VALUE" == "ENABLE"* ]]; } || \
         { [[ "$option_value" = "${TRUE}" ]] && [[ "$FOUND_VALUE" != "ENABLE"* ]] && [[ "$FOUND_VALUE" != "DISABLE"* ]]; }; then
        set_value=ON
      fi
      CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -D${FOUND_VALUE}=${set_value}"
    fi
  done

  if [ "${DEBUG_SYMBOLS}" = "${TRUE}" ]; then
    CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DCMAKE_BUILD_TYPE=RelWithDebInfo"
  fi

  if [ "${TESTS_ENABLED}" = "${TRUE}" ]; then
    # user may have disabled tests previously, so let's force them to be re-enabled
    CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DSKIP_TESTS= "
  else
    CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DSKIP_TESTS=true "
  fi

  if [ "${ASAN_ENABLED}" = "${TRUE}" ]; then
    CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DMINIFI_ADVANCED_ASAN_BUILD=ON "
  else
    CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DMINIFI_ADVANCED_ASAN_BUILD=OFF"
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

  if [ "${MINIFI_FAIL_ON_WARNINGS}" = "${TRUE}" ]; then
    CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DMINIFI_FAIL_ON_WARNINGS=ON "
  else
    CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DMINIFI_FAIL_ON_WARNINGS=OFF"
  fi

  CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DBUILD_IDENTIFIER=${BUILD_IDENTIFIER}"

  CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DCMAKE_BUILD_TYPE=${BUILD_PROFILE}"

  add_os_flags

  CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} .."

  continue_with_plan="Y"
  if [ ! "$NO_PROMPT" = "true" ]; then
    echo -n "Command will be '${CMAKE_BUILD_COMMAND}', run this? [ Y/N ] "
    read -r continue_with_plan
  fi
  if [ "$continue_with_plan" = "N" ] || [ "$continue_with_plan" = "n" ]; then
    echo "Exiting ...."
    exit
  fi
}


build_cmake_command

### run the cmake command
if [ "${SKIP_CMAKE}" = "${TRUE}" ]; then
	echo "Not running ${CMAKE_BUILD_COMMAND} "
else
	${CMAKE_BUILD_COMMAND}
fi

if [ "$BUILD" = "true" ]; then
  make -j"${CORES}"
fi

if [ "$PACKAGE" = "true" ]; then
  make package
fi


popd || exit 2

# vim: shiftwidth=2 tabstop=2 expandtab
