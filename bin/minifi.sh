#!/bin/sh
#
#    Licensed to the Apache Software Foundation (ASF) under one or more
#    contributor license agreements.  See the NOTICE file distributed with
#    this work for additional information regarding copyright ownership.
#    The ASF licenses this file to You under the Apache License, Version 2.0
#    (the "License"); you may not use this file except in compliance with
#    the License.  You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.

# Script structure inspired from Apache Karaf and other Apache projects with similar startup approaches

set -e

PROGNAME=$(basename "$0")
SCRIPTPATH="$( cd "$(dirname "$0")" || exit 1; pwd -P )"
MINIFI_HOME="$(dirname "${SCRIPTPATH}")"
export MINIFI_HOME
bin_dir=${MINIFI_HOME}/bin
minifi_executable=${bin_dir}/minifi

warn() {
    echo "${PROGNAME}: $*"
}

die() {
    warn "$*"
    exit 1
}

detectOS() {
    # OS specific support (must be 'true' or 'false').
    cygwin=false;
    darwin=false;
    case "$(uname)" in
        CYGWIN*)
            cygwin=true
            ;;
        Darwin)
            darwin=true
            ;;
    esac

    if [ "${cygwin}" = "true" ]; then
       echo 'Apache MiNiFi as a service is not supported on Cygwin.'
       exit 1
    fi
}

install_macos() {
    target_dir="/Library/LaunchDaemons"
    echo "Installing MiNiFi as a LaunchDaemon to ${target_dir}"
    cp -fv "${bin_dir}"/minifi.plist "${target_dir}"
    chmod 644 "${target_dir}"/minifi.plist
    sed -i '' "s|/opt/minifi-cpp|${MINIFI_HOME}|g" "${target_dir}"/minifi.plist
    launchctl load -w "${target_dir}"/minifi.plist
}

uninstall_macos() {
    launchctl unload "/Library/LaunchDaemons/minifi.plist" 2>/dev/null
    rm -fv "/Library/LaunchDaemons/minifi.plist" || :
}

check_service_installed_macos() {
    if [ ! -f "/Library/LaunchDaemons/minifi.plist" ]; then
        echo "MiNiFi is not installed as a service. Please run 'minifi.sh install' first."
        exit 1
    fi
}

install_linux() {
    target_dir="/usr/local/lib/systemd/system"
    mkdir -p "${target_dir}" || die "Unable to create ${target_dir}. Cannot install MiNiFi as a service."

    echo "Installing MiNiFi as a systemd service to ${target_dir}"
    cp -fv "${bin_dir}"/minifi.service "${target_dir}"
    chmod 644 "${target_dir}"/minifi.service
    sed -i "s|/opt/minifi-cpp|${MINIFI_HOME}|g" "${target_dir}"/minifi.service
    systemctl daemon-reload
    systemctl enable minifi.service
}

uninstall_linux() {
    # Uninstall legacy init.d service files, if exists
    rm -fv "/etc/init.d/minifi" || :
    rm -fv "/etc/rc2.d/S65minifi" || :
    rm -fv "/etc/rc2.d/K65minifi" || :

    # Uninstall systemd service files, if exists
    rm -fv "/usr/local/lib/systemd/system/minifi.service" || :
    systemctl daemon-reload
}

check_service_installed_linux() {
    if [ ! -f "/usr/local/lib/systemd/system/minifi.service" ]; then
        echo "MiNiFi is not installed as a service. Please run 'minifi.sh install' first."
        exit 1
    fi
}

install() {
    if [ "$(id -u)" -ne 0 ]; then
        die "This script must be run as root to install or uninstall the MiNiFi service. Try 'sudo $0 $*'."
    fi

    echo "Uninstalling any previous versions"
    uninstall

    detectOS
    if [ "${darwin}" = "true"  ]; then
      install_macos
    else
      install_linux
    fi
}

uninstall() {
    if [ "$(id -u)" -ne 0 ]; then
        die "This script must be run as root to install or uninstall the MiNiFi service. Try 'sudo $0 $*'."
    fi

    detectOS
    if [ "${darwin}" = "true"  ]; then
      uninstall_macos
    else
      uninstall_linux
    fi
}

check_service_installed() {
    detectOS
    if [ "${darwin}" = "true"  ]; then
      check_service_installed_macos
    else
      check_service_installed_linux
    fi
}

start_service() {
    check_service_installed
    if [ "${darwin}" = "true"  ]; then
      launchctl start org.apache.nifi.minifi
    else
      systemctl start minifi.service
    fi
    echo "MiNiFi started"
}

stop_service() {
    check_service_installed
    if [ "${darwin}" = "true"  ]; then
      launchctl stop org.apache.nifi.minifi
    else
      systemctl stop minifi.service
    fi
    echo "MiNiFi stopped"
}

restart_service() {
    check_service_installed
    if [ "${darwin}" = "true"  ]; then
      launchctl stop org.apache.nifi.minifi
      launchctl start org.apache.nifi.minifi
    else
      systemctl restart minifi.service
    fi
    echo "MiNiFi restarted"
}

status_service() {
    check_service_installed
    if [ "${darwin}" = "true"  ]; then
      launchctl list | grep org.apache.nifi.minifi
    else
      systemctl status minifi.service
    fi
}

flowStatus() {
    if ! [ -f "${bin_dir}/minificontroller" ]; then
        echo "MiNiFi Controller is not installed"
        return
    fi
    if [ "$#" -lt 2 ]; then
        echo "MiNiFi flowStatus operation requires a flow status query parameter like \"processor:TailFile:health,stats,bulletins\""
        return
    fi

    if [ "$#" -lt 3 ]; then
        exec "${bin_dir}/minificontroller" --flowstatus "$2"
    elif [ "$#" -lt 4 ]; then
        exec "${bin_dir}/minificontroller" --port "$2" --flowstatus "$3"
    else
        exec "${bin_dir}/minificontroller" --host "$2" --port "$3" --flowstatus "$4"
    fi
}

case "$1" in
    start)
      start_service
      ;;
    stop)
      stop_service
      ;;
    run)
      exec "${minifi_executable}"
      ;;
    status)
      status_service
      ;;
    restart)
      restart_service
      ;;
    install)
      install "$@"
      echo "Service minifi installed. Please start it using 'minifi.sh start'"
      ;;
    uninstall)
      uninstall "$@"
      echo "Service minifi uninstalled. Please remove the ${MINIFI_HOME} directory manually."
      ;;
    flowStatus)
      flowStatus "$@"
      ;;
    *)
      echo "Usage: minifi.sh {start|stop|run|restart|status|install|uninstall|flowStatus}"
      ;;
esac
