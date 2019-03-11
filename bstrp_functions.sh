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

pause(){
  read -p "Press [Enter] key to continue..." fackEnterKey
}


load_state(){
  if [ -f ${script_directory}/bt_state ]; then
    . ${script_directory}/bt_state
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
  echo "$1=\"${VARIABLE_VALUE}\"" >> ${script_directory}/bt_state
}

save_state(){
  echo "VERSION=1" > ${script_directory}/bt_state
  echo_state_variable BUILD_IDENTIFIER
  echo_state_variable BUILD_DIR
  echo_state_variable TESTS_DISABLED
  echo_state_variable USE_SHARED_LIBS
  for option in "${OPTIONS[@]}" ; do
    echo_state_variable $option
  done
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
  read -p "Enter choice [ A-C ] " choice
  choice=$(echo ${choice} | tr '[:upper:]' '[:lower:]')
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
  read -p "Enter choice [ A-C ] " choice
  choice=$(echo ${choice} | tr '[:upper:]' '[:lower:]')
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
  echo "M. SQLite Support ..............$(print_feature_status SQLITE_ENABLED)"
  echo "N. Python Support ..............$(print_feature_status PYTHON_ENABLED)"
  echo "O. COAP Support ................$(print_feature_status COAP_ENABLED)"
  echo "****************************************"
  echo "            Build Options."
  echo "****************************************"
  echo "1. Disable Tests ...............$(print_feature_status TESTS_DISABLED)"
  echo "2. Enable all extensions"
  echo "3. Enable JNI Support ..........$(print_feature_status JNI_ENABLED)"
  echo "4. Use Shared Dependency Links .$(print_feature_status USE_SHARED_LIBS)"
  echo "P. Continue with these options"
  if [ "$GUIDED_INSTALL" = "${TRUE}" ]; then
    echo "R. Return to Main Menu"
  fi
  echo "Q. Quit"
  echo "* Extension cannot be installed due to"
  echo -e "  version of cmake or other software\r\n"
}

read_feature_options(){
  local choice
  read -p "Enter choice [ A - P or 1-3 ] " choice
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
    m) ToggleFeature SQLITE_ENABLED ;;
    n) if [ "$USE_SHARED_LIBS" = "${TRUE}" ]; then
         ToggleFeature PYTHON_ENABLED
       else
         echo -e "${RED}Please ensure static linking is enabled for Python Support...${NO_COLOR}" && sleep 2
   	   fi
   	   ;;
    o) ToggleFeature COAP_ENABLED ;;
    1) ToggleFeature TESTS_DISABLED ;;
    2) EnableAllFeatures ;;
    3) ToggleFeature JNI_ENABLED;;
    4) if [ "$PYTHON_ENABLED" = "${FALSE}" ]; then
         ToggleFeature USE_SHARED_LIBS
       else
         echo -e "${RED}Python support must be disabled before changing this value...${NO_COLOR}" && sleep 2
   	   fi
       ;;
    p) FEATURES_SELECTED="true" ;;
    r) if [ "$GUIDED_INSTALL" = "${TRUE}" ]; then
        MENU="main"
      fi
      ;;
    q) exit 0;;
    *) echo -e "${RED}Please enter an option A-P or 1-4...${NO_COLOR}" && sleep 2
  esac
}

