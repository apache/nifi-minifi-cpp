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
    aix=false;
    darwin=false;
    case "$(uname)" in
        CYGWIN*)
            cygwin=true
            ;;
        AIX*)
            aix=true
            ;;
        Darwin)
            darwin=true
            ;;
    esac
    # For AIX, set an environment variable
    if ${aix}; then
         export LDR_CNTRL=MAXDATA=0xB0000000@DSA
         echo ${LDR_CNTRL}
    fi
}

install() {
    detectOS
    if [ "${darwin}" = "true"  ] || [ "${cygwin}" = "true" ]; then
        echo 'Installing Apache MiNiFi as a service is not supported on OS X or Cygwin.'
        exit 1
    fi

    echo "Uninstalling any previous versions"
    uninstall

    target_dir="/usr/local/lib/systemd/system"
    mkdir -p "${target_dir}" || die "Unable to create ${target_dir}. Cannot install MiNiFi as a service."

    echo "Installing MiNiFi as a systemd service to ${target_dir}"
    cp -fv "${bin_dir}"/minifi.service "${target_dir}"
    chmod 644 "${target_dir}"/minifi.service
    sed -i "s|/opt/minifi-cpp|${MINIFI_HOME}|g" "${target_dir}"/minifi.service
    systemctl daemon-reload
    systemctl enable minifi.service
}

uninstall() {
    detectOS
    if [ "${darwin}" = "true"  ] || [ "${cygwin}" = "true" ]; then
        echo 'Apache MiNiFi as a service is not supported on OS X or Cygwin.'
        exit 1
    fi

    # Uninstall legacy init.d service files, if exists
    rm -fv "/etc/init.d/minifi" || :
    rm -fv "/etc/rc2.d/S65minifi" || :
    rm -fv "/etc/rc2.d/K65minifi" || :

    # Uninstall systemd service files, if exists
    rm -fv "/usr/local/lib/systemd/system/minifi.service" || :
    systemctl daemon-reload
}

check_service_installed() {
    if [ ! -f "/usr/local/lib/systemd/system/minifi.service" ]; then
        echo "MiNiFi is not installed as a service. Please run 'minifi.sh install' first."
        exit 1
    fi
}

case "$1" in
    start)
      check_service_installed
      systemctl start minifi.service
      echo "MiNiFi started"
      ;;
    stop)
      check_service_installed
      systemctl stop minifi.service
      echo "MiNiFi stopped"
      ;;
    run)
      exec "${minifi_executable}"
      ;;
    status)
      check_service_installed
      systemctl status minifi.service
      ;;
    restart)
      check_service_installed
      systemctl restart minifi.service
      echo "MiNiFi restarted"
      ;;
    install)
      install "$@"
      echo "Service minifi installed. Please start it using 'minifi.sh start' or 'systemctl start minifi.service'"
      ;;
    uninstall)
      uninstall "$@"
      echo "Service minifi uninstalled. Please remove the ${MINIFI_HOME} directory manually."
      ;;
    *)
      echo "Usage: minifi.sh {start|stop|run|restart|status|install|uninstall}"
      ;;
esac
