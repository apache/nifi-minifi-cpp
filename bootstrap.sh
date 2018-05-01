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
#!/bin/bash

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

DEPLOY="false"
OPTIONS=()
CMAKE_OPTIONS_ENABLED=()
CMAKE_OPTIONS_DISABLED=()
CMAKE_MIN_VERSION=()
DEPLOY_LIMITS=()

DEPENDENCIES=()

add_option(){
  eval "$1=$2"
  OPTIONS+=("$1")
  CMAKE_OPTIONS_ENABLED+=("$1:$3")
  CMAKE_OPTIONS_DISABLED+=("$1:$4")
}

add_enabled_option(){
  eval "$1=$2"
  OPTIONS+=("$1")
  CMAKE_OPTIONS_DISABLED+=("$1:$3")
}
add_cmake_option(){
  eval "$1=$2"
}

add_disabled_option(){
  eval "$1=$2"
  OPTIONS+=("$1")
  CMAKE_OPTIONS_ENABLED+=("$1:$3")
  if [ ! -z "$4" ]; then
    CMAKE_MIN_VERSION+=("$1:$4")
  fi

  if [ ! -z "$5" ]; then
    if [ "$5" = "true" ]; then
      DEPLOY_LIMITS+=("$1")
    fi
  fi
}

add_dependency(){
  DEPENDENCIES+=("$1:$2")
}

### parse the command line arguments


EnableAllFeatures(){
  for option in "${OPTIONS[@]}" ; do
    feature_status=${!option}
    if [ "$feature_status" = "${FALSE}" ]; then
      ToggleFeature $option
    fi
    #	eval "$option=${TRUE}"
  done
}

while :; do
  case $1 in
    -n|--noprompt)
      NO_PROMPT="true"
      ;;
    -e|--enableall)
      NO_PROMPT="true"
      FEATURES_SELECTED="true"
      EnableAllFeatures
      ;;
    -d|--deploy)
      NO_PROMPT="true"
      DEPLOY="true"
      FEATURES_SELECTED="true"
      EnableAllFeatures
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
  echo "Welcome, this boostrap script will update your system to install MiNIFi C++"
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
if [[ "$OS" = "Darwin" ]]; then
  . "${script_directory}/darwin.sh"
elif [[ "$OS" = Deb* ]]; then
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
elif [[ "$OS" = CentOS* ]]; then
  . "${script_directory}/centos.sh"
elif [[ "$OS" = Fedora* ]]; then
  . "${script_directory}/fedora.sh"
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


# Since the following extensions have limitations on

add_disabled_option BUSTACHE_ENABLED ${FALSE} "ENABLE_BUSTACHE" "2.6" ${TRUE}
add_dependency BUSTACHE_ENABLED "boost"
## currently need to limit on certain platforms
add_disabled_option TENSORFLOW_ENABLED ${FALSE} "ENABLE_TENSORFLOW" "2.6" ${TRUE}
add_dependency TENSORFLOW_ENABLED "tensorflow"


pause(){
  read -p "Press [Enter] key to continue..." fackEnterKey
}

can_deploy(){
  for option in "${DEPLOY_LIMITS[@]}" ; do
    OPT=${option%%:*}
    if [ "${OPT}" = "$1" ]; then
      echo "false"
    fi
  done
  echo "true"
}

ToggleFeature(){
  VARIABLE_VALUE=${!1}
  if [ $VARIABLE_VALUE = "Enabled" ]; then
    eval "$1=${FALSE}"
  else
    for option in "${CMAKE_MIN_VERSION[@]}" ; do
      OPT=${option%%:*}
      if [ "$OPT" = "$1" ]; then
        NEEDED_VER=${option#*:}
        NEEDED_MAJOR=`echo $NEEDED_VER | cut -d. -f1`
        NEEDED_MINOR=`echo $NEEDED_VER | cut -d. -f2`
        NEEDED_REVISION=`echo $NEEDED_VERSION | cut -d. -f3`
        if (( NEEDED_MAJOR > CMAKE_MAJOR )); then
          return 1
        fi

        if (( NEEDED_MINOR > CMAKE_MINOR )); then
          return 1
        fi

        if (( NEEDED_REVISION > CMAKE_REVISION )); then
          return 1
        fi
      fi
    done
    CAN_ENABLE=$(verify_enable $1)
    CAN_DEPLOY=$(can_deploy $1)
    if [ "$CAN_ENABLE" = "true" ]; then
      if [[ "$DEPLOY" = "true" &&  "$CAN_DEPLOY" = "true" ]] || [[ "$DEPLOY" = "false" ]]; then
        eval "$1=${TRUE}"
      fi
    fi
  fi
}


print_feature_status(){
  feature="$1"
  feature_status=${!1}
  if [ "$feature_status" = "Enabled" ]; then
    echo "Enabled"
  else
    for option in "${CMAKE_MIN_VERSION[@]}" ; do
      OPT=${option%%:*}
      if [ "${OPT}" = "$1" ]; then
        NEEDED_VER=${option#*:}
        NEEDED_MAJOR=`echo $NEEDED_VER | cut -d. -f1`
        NEEDED_MINOR=`echo $NEEDED_VER | cut -d. -f2`
        NEEDED_REVISION=`echo $NEEDED_VERSION | cut -d. -f3`
        if (( NEEDED_MAJOR > CMAKE_MAJOR )); then
          echo -e "${RED}Disabled*${NO_COLOR}"
          return 1
        fi

        if (( NEEDED_MINOR > CMAKE_MINOR )); then
          echo -e "${RED}Disabled*${NO_COLOR}"
          return 1
        fi

        if (( NEEDED_REVISION > CMAKE_REVISION )); then
          echo -e "${RED}Disabled*${NO_COLOR}"
          return 1
        fi
      fi
    done
    CAN_ENABLE=$(verify_enable $1)
    if [ "$CAN_ENABLE" = "true" ]; then
      echo -e "${RED}Disabled${NO_COLOR}"
    else
      echo -e "${RED}Disabled*${NO_COLOR}"
    fi

  fi
}


if [ ! -d "build" ]; then
  mkdir build/
else

  overwrite="Y"
  if [ "$NO_PROMPT" = "false" ] && [ "$FEATURES_SELECTED" = "false" ]; then
    echo "CMAKE Build dir exists, should we overwrite your build directory before we begin?"
    read -p "If you have already bootstrapped, bootstrapping again isn't necessary to run make [ Y/N ] " overwrite
  fi
  if [ "$overwrite" = "N" ] || [ "$overwrite" = "n" ]; then
    echo "Exiting ...."
    exit
  else
    rm build/CMakeCache.txt > /dev/null 2>&1
  fi
fi

## change to the directory

pushd build


show_supported_features() {
  clear
  echo "****************************************"
  echo " Select MiNiFi C++ Features to toggle."
  echo "****************************************"
  echo "A. Persistent Repositories .....$(print_feature_status ROCKSDB_ENABLED)"
  echo "B. Lib Curl Features ...........$(print_feature_status HTTP_CURL_ENABLED)"
  echo "C. Lib Archive Features ........$(print_feature_status LIBARCHIVE_ENABLED)"
  echo "D. Execute Script support ......$(print_feature_status EXECUTE_SCRIPT_ENABLED)"
  echo "E. Expression Language support .$(print_feature_status EXPRESSION_LANGAUGE_ENABLED)"
  echo "F. Kafka support ...............$(print_feature_status KAFKA_ENABLED)"
  echo "G. PCAP support ................$(print_feature_status PCAP_ENABLED)"
  echo "H. USB Camera support ..........$(print_feature_status USB_ENABLED)"
  echo "I. GPS support .................$(print_feature_status GPS_ENABLED)"
  echo "J. TensorFlow Support ..........$(print_feature_status TENSORFLOW_ENABLED)"
  echo "K. Bustache Support ............$(print_feature_status BUSTACHE_ENABLED)"
  echo "L. MQTT Support ................$(print_feature_status MQTT_ENABLED)"
  echo "M. Enable all extensions"
  echo "N. Portable Build ..............$(print_feature_status PORTABLE_BUILD)"
  echo "O. Build with Debug symbols ....$(print_feature_status DEBUG_SYMBOLS)"
  echo "P. Continue with these options"
  echo "Q. Exit"
  echo "* Extension cannot be installed due to"
  echo -e "  version of cmake or other software\r\n"
}
read_options(){
  local choice
  read -p "Enter choice [ A - N ] " choice
  choice=$(echo ${choice} | tr '[:upper:]' '[:lower:]')
  case $choice in
    a) ToggleFeature ROCKSDB_ENABLED ;;
    b) ToggleFeature HTTP_CURL_ENABLED ;;
    c) ToggleFeature LIBARCHIVE_ENABLED ;;
    d) ToggleFeature EXECUTE_SCRIPT_ENABLED ;;
    e) ToggleFeature EXPRESSION_LANGAUGE_ENABLED ;;
    f) ToggleFeature KAFKA_ENABLED ;;
    g) ToggleFeature PCAP_ENABLED ;;
    h) ToggleFeature USB_ENABLED ;;
    i) ToggleFeature GPS_ENABLED ;;
    j) ToggleFeature TENSORFLOW_ENABLED ;;
    k) ToggleFeature BUSTACHE_ENABLED ;;
    l) ToggleFeature MQTT_ENABLED ;;
    m) EnableAllFeatures ;;
    n) ToggleFeature PORTABLE_BUILD ;;
    o) ToggleFeature DEBUG_SYMBOLS ;;
    p) FEATURES_SELECTED="true" ;;
    q) exit 0;;
    *) echo -e "${RED}Please enter an option A-L...${NO_COLOR}" && sleep 2
  esac
}

while [ ! "$FEATURES_SELECTED" == "true" ]
do
  show_supported_features
  read_options
done

### ensure we have all dependencies


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
        CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -D${FOUND_VALUE}=TRUE"
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
        CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -D${FOUND_VALUE}=TRUE"
      fi
    fi
  done
  if [ "${DEBUG_SYMBOLS}" = "${TRUE}" ]; then
    CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DCMAKE_BUILD_TYPE=RelWithDebInfo"
  fi

  if [ "${PORTABLE_BUILD}" = "${TRUE}" ]; then
    CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DPORTABLE=ON "
  else
    CMAKE_BUILD_COMMAND="${CMAKE_BUILD_COMMAND} -DPORTABLE=OFF "
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
  if [ "$overwrite" = "N" ] || [ "$overwrite" = "n" ]; then
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
