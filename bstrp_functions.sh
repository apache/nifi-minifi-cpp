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

get_cmake_version(){
  CMAKE_VERSION=$(${CMAKE_COMMAND} --version | head -n 1 | awk '{print $3}')

  CMAKE_MAJOR=$(echo "$CMAKE_VERSION" | cut -d. -f1)
  CMAKE_MINOR=$(echo "$CMAKE_VERSION" | cut -d. -f2)
  CMAKE_REVISION=$(echo "$CMAKE_VERSION" | cut -d. -f3)
}

add_cmake_option(){
  eval "$1=$2"
}

add_option(){
  BOOTSTRAP_OPTION_NAME="$1"
  DEFAULT_VALUE="$2"
  CMAKE_OPTION_NAME="$3"
  eval "$BOOTSTRAP_OPTION_NAME=$DEFAULT_VALUE"
  OPTIONS+=("$BOOTSTRAP_OPTION_NAME")
  CMAKE_OPTIONS+=("$BOOTSTRAP_OPTION_NAME:$CMAKE_OPTION_NAME")
  if [ -n "$4" ]; then
    CMAKE_MIN_VERSION+=("$BOOTSTRAP_OPTION_NAME:$4")
  fi

  if [ -n "$5" ]; then
    if [ "$5" = "true" ]; then
      DEPLOY_LIMITS+=("$BOOTSTRAP_OPTION_NAME")
    fi
  fi
}

add_multi_option(){
  eval "$1=$2"
  ARRAY=()
  eval "export $1_OPTIONS=()"
  for i in "${@:3}"; do
    ARRAY+=("$i")
  done
  for i in "${!ARRAY[@]}"; do
    eval "$1_OPTIONS[$i]=${ARRAY[$i]}"
	done
}

set_dependency(){
  DEPENDS_ON+=("$1:$2")
}

set_incompatible_with(){
  INCOMPATIBLE_WITH+=("$1:$2")
  INCOMPATIBLE_WITH+=("$2:$1")
}

print_multi_option_status(){
  feature_status=${!1}
  declare -a VAR_OPTS=()

  declare VAR_OPTS=("$1_OPTIONS[@]")
  VAR_OPTS=("$1_OPTIONS[@]")

  for option in "${!VAR_OPTS}" ; do
    if [ "${option}" = "$feature_status" ]; then
      printf "%b%s%b " "${RED}" "${option}" "${NO_COLOR}"
    else
      printf "%s " "${option}"
    fi
  done
}

ToggleMultiOption(){
  feature_status=${!1}
  declare -a VAR_OPTS=()

  declare VAR_OPTS=("$1_OPTIONS[@]")
  #echo -e "${RED}${feature_status}${NO_COLOR} (${VAR_OPTS_VAL})"
  VAR_OPTS=("$1_OPTIONS[@]")
  invariant=""
  first=""
  # the alternative is to loop through an array but since we're an indirected
  # copy, we'll treat this as a manual circular buffer
  for option in "${!VAR_OPTS}" ; do
  if [ -z "${first}"  ]; then
  	first=${option}
  fi
   if [ "${invariant}" = "next" ]; then
    	eval "$1=${option}"
    	invariant=""
    	break
    fi
    if [ "${option}" = "$feature_status" ]; then
    	invariant="next"
    fi
  done
  if [ "${invariant}" = "next" ]; then
  	eval "$1=${first}"
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
      ToggleFeature "${option}"
    fi
    #	eval "$option=${TRUE}"
  done
}

pause(){
  echo -n "Press [Enter] key to continue..."
  read -r _
}


load_state(){
  if [ -f "${script_directory}/bt_state" ]; then
    . "${script_directory}/bt_state"
    for option in "${OPTIONS[@]}" ; do
      option_value="${!option}"
      if [ "${option_value}" = "${FALSE}" ]; then
        ALL_FEATURES_ENABLED=${FALSE}
      fi
    done
  fi
}

echo_state_variable(){
  VARIABLE_VALUE=${!1}
  echo "$1=\"${VARIABLE_VALUE}\"" >> "${script_directory}/bt_state"
}

save_state(){
  echo "VERSION=1" > "${script_directory}/bt_state"
  echo_state_variable BUILD_IDENTIFIER
  echo_state_variable BUILD_DIR
  echo_state_variable TESTS_ENABLED
  echo_state_variable BUILD_PROFILE
  echo_state_variable USE_SHARED_LIBS
  echo_state_variable ASAN_ENABLED
  echo_state_variable FAIL_ON_WARNINGS
  for option in "${OPTIONS[@]}" ; do
    echo_state_variable "${option}"
  done
}

check_compatibility(){
  for option in "${INCOMPATIBLE_WITH[@]}" ; do
    OPT=${option%%:*}
    if [ "$OPT" = "$1" ]; then
      OTHER_FEATURE=${option#*:}
      OTHER_FEATURE_VALUE=${!OTHER_FEATURE}
      if [ "${OTHER_FEATURE_VALUE}" = "Enabled" ]; then
        echo "false"
        return
      fi
    fi
  done
  for option in "${DEPENDS_ON[@]}" ; do
    OPT=${option%%:*}
    if [ "$OPT" = "$1" ]; then
      OTHER_FEATURE=${option#*:}
      OTHER_FEATURE_VALUE=${!OTHER_FEATURE}
      if [ "${OTHER_FEATURE_VALUE}" != "Enabled" ]; then
        echo "false"
        return
      fi
    fi
  done
  echo "true"
}

verify_enable(){
  COMPATIBLE=$(check_compatibility "$1")
  if [ "$COMPATIBLE" = "true" ]; then
    verify_enable_platform "$1"
  else
    echo "false"
  fi
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
  ALL_FEATURES_ENABLED="Disabled"
  if [ "${VARIABLE_VALUE}" = "Enabled" ]; then
    eval "$1=${FALSE}"
    for option in "${DEPENDS_ON[@]}" ; do
      DEPENDENT_FEATURE=${option%%:*}
      FEATURE=${option#*:}
      if [ "$FEATURE" = "$1" ]; then
        eval "$DEPENDENT_FEATURE=${FALSE}"
      fi
    done
  else
    for option in "${CMAKE_MIN_VERSION[@]}" ; do
      OPT=${option%%:*}
      if [ "$OPT" = "$1" ]; then
        NEEDED_VER=${option#*:}
        NEEDED_MAJOR=$(echo "$NEEDED_VER" | cut -d. -f1)
        NEEDED_MINOR=$(echo "$NEEDED_VER" | cut -d. -f2)
        NEEDED_REVISION=$(echo "$NEEDED_VERSION" | cut -d. -f3)
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
    CAN_ENABLE=$(verify_enable "$1")
    CAN_DEPLOY=$(can_deploy "$1")
    if [ "$CAN_ENABLE" = "true" ]; then
      if [[ "$DEPLOY" = "true" &&  "$CAN_DEPLOY" = "true" ]] || [[ "$DEPLOY" = "false" ]]; then
        eval "$1=${TRUE}"
      fi
    fi
  fi
}


print_feature_status(){
  feature_status=${!1}
  if [ "$feature_status" = "Enabled" ]; then
    echo "Enabled"
  else
    for option in "${CMAKE_MIN_VERSION[@]}" ; do
      OPT=${option%%:*}
      if [ "${OPT}" = "$1" ]; then
        NEEDED_VER=${option#*:}
        NEEDED_MAJOR=$(echo "$NEEDED_VER" | cut -d. -f1)
        NEEDED_MINOR=$(echo "$NEEDED_VER" | cut -d. -f2)
        NEEDED_REVISION=$(echo "$NEEDED_VERSION" | cut -d. -f3)
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
    CAN_ENABLE=$(verify_enable "$1")
    if [ "$CAN_ENABLE" = "true" ]; then
      echo -e "${RED}Disabled${NO_COLOR}"
    else
      echo -e "${RED}Disabled*${NO_COLOR}"
    fi

  fi
}



show_main_menu() {
  clear
  echo "****************************************"
  echo " MiNiFi C++ Main Menu."
  echo "****************************************"
  echo "A. Select MiNiFi C++ Features "
  if [ "$ALL_FEATURES_ENABLED" = "${TRUE}" ]; then
    echo "  All features enabled  ........$(print_feature_status ALL_FEATURES_ENABLED)"
  fi
  echo "B. Select Advanced Features   "
  echo "C. Continue with selected options   "
  echo -e "Q. Exit\r\n"
}

read_main_menu_options(){
  local choice
  echo -n "Enter choice [ A-C ] "
  read -r choice
  choice=$(echo "${choice}" | tr '[:upper:]' '[:lower:]')
  case $choice in
    a) MENU="features" ;;
    b) MENU="advanced" ;;
    c) FEATURES_SELECTED="true" ;;
    q) exit 0;;
    *) echo -e "${RED}Please enter a valid option...${NO_COLOR}" && sleep 1
  esac
}

show_advanced_features_menu() {
  clear
  echo "****************************************"
  echo " MiNiFi C++ Advanced Features."
  echo "****************************************"
  echo "A. Portable Build ..............$(print_feature_status PORTABLE_BUILD)"
  echo "B. Build with Debug symbols ....$(print_feature_status DEBUG_SYMBOLS)"
  echo "C. Build RocksDB from source ...$(print_feature_status BUILD_ROCKSDB)"
  echo -e "R. Return to Main Menu\r\n"
}

read_advanced_menu_options(){
  local choice
  echo -n "Enter choice [ A-C ] "
  read -r choice
  choice=$(echo "${choice}" | tr '[:upper:]' '[:lower:]')
  case $choice in
    a) ToggleFeature PORTABLE_BUILD ;;
    b) ToggleFeature DEBUG_SYMBOLS ;;
    c) ToggleFeature BUILD_ROCKSDB ;;
    r) MENU="main" ;;
    *) echo -e "${RED}Please enter a valid option...${NO_COLOR}" && sleep 1
  esac
}



show_supported_features() {
  clear
  echo "****************************************"
  echo " Select MiNiFi C++ Features to toggle."
  echo "****************************************"
  echo "A. Persistent Repositories .....$(print_feature_status ROCKSDB_ENABLED)"
  echo "B. libcurl features ............$(print_feature_status HTTP_CURL_ENABLED)"
  echo "C. libarchive features .........$(print_feature_status LIBARCHIVE_ENABLED)"
  echo "D. Python Scripting support ....$(print_feature_status PYTHON_SCRIPTING_ENABLED)"
  echo "E. Expression Language support .$(print_feature_status EXPRESSION_LANGUAGE_ENABLED)"
  echo "F. Kafka support ...............$(print_feature_status KAFKA_ENABLED)"
  echo "G. PCAP support ................$(print_feature_status PCAP_ENABLED)"
  echo "H. USB Camera support ..........$(print_feature_status USB_ENABLED)"
  echo "I. GPS support .................$(print_feature_status GPS_ENABLED)"
  echo "J. TensorFlow Support ..........$(print_feature_status TENSORFLOW_ENABLED)"
  echo "K. Bustache Support ............$(print_feature_status BUSTACHE_ENABLED)"
  echo "L. Lua Scripting Support .......$(print_feature_status LUA_SCRIPTING_ENABLED)"
  echo "M. MQTT Support ................$(print_feature_status MQTT_ENABLED)"
  echo "N. COAP Support ................$(print_feature_status COAP_ENABLED)"
  echo "O. SFTP Support ................$(print_feature_status SFTP_ENABLED)"
  echo "S. AWS Support .................$(print_feature_status AWS_ENABLED)"
  echo "T. OpenCV Support ..............$(print_feature_status OPENCV_ENABLED)"
  echo "U. OPC-UA Support...............$(print_feature_status OPC_ENABLED)"
  echo "V. SQL Support..................$(print_feature_status SQL_ENABLED)"
  echo "W. Openwsman Support ...........$(print_feature_status OPENWSMAN_ENABLED)"
  echo "X. Azure Support ...............$(print_feature_status AZURE_ENABLED)"
  if $LINUX; then
    echo "Y. Systemd Support .............$(print_feature_status SYSTEMD_ENABLED)"
  fi
  echo "Z. NanoFi Support ..............$(print_feature_status NANOFI_ENABLED)"
  echo "AA. Splunk Support .............$(print_feature_status SPLUNK_ENABLED)"
  echo "AB. Kubernetes Support .........$(print_feature_status KUBERNETES_ENABLED)"
  echo "AC. Google Cloud Support .......$(print_feature_status GCP_ENABLED)"
  echo "AD. ProcFs Support .............$(print_feature_status PROCFS_ENABLED)"
  echo "AE. Prometheus Support .........$(print_feature_status PROMETHEUS_ENABLED)"
  echo "AF. Elasticsearch Support ......$(print_feature_status ELASTIC_ENABLED)"
  echo "****************************************"
  echo "            Build Options."
  echo "****************************************"
  echo "1. Enable Tests ................$(print_feature_status TESTS_ENABLED)"
  echo "2. Enable all extensions"
  echo "3. Enable JNI Support ..........$(print_feature_status JNI_ENABLED)"
  echo "4. Use Shared Dependency Links .$(print_feature_status USE_SHARED_LIBS)"
  echo "5. Build Profile ...............$(print_multi_option_status BUILD_PROFILE)"
  echo "6. Create ASAN build ...........$(print_feature_status ASAN_ENABLED)"
  echo "7. Treat warnings as errors.....$(print_feature_status FAIL_ON_WARNINGS)"
  echo "8. Enable OpenSSL...............$(print_feature_status OPENSSL_ENABLED)"
  echo "P. Continue with these options"
  if [ "$GUIDED_INSTALL" = "${TRUE}" ]; then
    echo "R. Return to Main Menu"
  fi
  echo "Q. Quit"
  echo "* Extension cannot be installed due to"
  echo "  version of cmake or other software, or"
  echo -e "  incompatibility with other extensions\r\n"
}

read_feature_options(){
  local choice
  echo -n "Enter choice [A-Z or AA-AF or 1-7] "
  read -r choice
  choice=$(echo "${choice}" | tr '[:upper:]' '[:lower:]')
  case $choice in
    a) ToggleFeature ROCKSDB_ENABLED ;;
    b) ToggleFeature HTTP_CURL_ENABLED ;;
    c) ToggleFeature LIBARCHIVE_ENABLED ;;
    d) ToggleFeature PYTHON_SCRIPTING_ENABLED ;;
    e) ToggleFeature EXPRESSION_LANGUAGE_ENABLED ;;
    f) ToggleFeature KAFKA_ENABLED ;;
    g) ToggleFeature PCAP_ENABLED ;;
    h) ToggleFeature USB_ENABLED ;;
    i) ToggleFeature GPS_ENABLED ;;
    j) ToggleFeature TENSORFLOW_ENABLED ;;
    k) ToggleFeature BUSTACHE_ENABLED ;;
    l) ToggleFeature LUA_SCRIPTING_ENABLED ;;
    m) ToggleFeature MQTT_ENABLED ;;
    n) ToggleFeature COAP_ENABLED ;;
    o) ToggleFeature SFTP_ENABLED ;;
    s) ToggleFeature AWS_ENABLED ;;
    t) ToggleFeature OPENCV_ENABLED ;;
    u) ToggleFeature OPC_ENABLED ;;
    v) ToggleFeature SQL_ENABLED ;;
    w) ToggleFeature OPENWSMAN_ENABLED ;;
    x) ToggleFeature AZURE_ENABLED ;;
    y) if $LINUX; then ToggleFeature SYSTEMD_ENABLED; fi ;;
    z) ToggleFeature NANOFI_ENABLED ;;
    aa) ToggleFeature SPLUNK_ENABLED ;;
    ab) ToggleFeature KUBERNETES_ENABLED ;;
    ac) ToggleFeature GCP_ENABLED ;;
    ad) ToggleFeature PROCFS_ENABLED ;;
    ae) ToggleFeature PROMETHEUS_ENABLED ;;
    af) ToggleFeature ELASTIC_ENABLED ;;
    1) ToggleFeature TESTS_ENABLED ;;
    2) EnableAllFeatures ;;
    3) ToggleFeature JNI_ENABLED;;
    4) ToggleFeature USE_SHARED_LIBS;;
    5) ToggleMultiOption BUILD_PROFILE;;
    6) ToggleFeature ASAN_ENABLED;;
    7) ToggleFeature FAIL_ON_WARNINGS;;
    8) ToggleFeature OPENSSL_ENABLED;;
    p) export FEATURES_SELECTED="true" ;;
    r) if [ "$GUIDED_INSTALL" = "${TRUE}" ]; then
        export MENU="main"
      fi
      ;;
    q) exit 0;;
    *) echo -e "${RED}Please enter an option A-Z or AA-AF or 1-7...${NO_COLOR}" && sleep 2
  esac
}
