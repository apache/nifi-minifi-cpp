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

SCRIPT_DIR=$(dirname "$0")
SCRIPT_NAME=$(basename "$0")
PROGNAME=$(basename "$0")
SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"
export MINIFI_HOME="$(dirname ${SCRIPTPATH})"
bin_dir=${MINIFI_HOME}/bin
minifi_executable=${bin_dir}/minifi
pid_file=${bin_dir}/.minifi.pid

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
    os400=false;
    darwin=false;
    case "$(uname)" in
        CYGWIN*)
            cygwin=true
            ;;
        AIX*)
            aix=true
            ;;
        OS400*)
            os400=true
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

init() {
    # Determine if there is special OS handling we must perform
    detectOS
}

# determines the pid
get_pid() {
  # Default to a -1 for pid
  pid=-1
  # Check to see if we have a pid file
  if [ -f ${pid_file} ]; then
    pid=$(cat ${pid_file})
  fi
  echo ${pid}
}

# Performs a check to see if the provided pid is one that currently exists
active_pid() {
  pid=${1}
  kill -s 0 ${pid} > /dev/null 2>&1
  echo $?
}

install() {
    detectOS

    if [ "${darwin}" = "true"  ] || [ "${cygwin}" = "true" ]; then
        echo 'Installing Apache MiNiFi as a service is not supported on OS X or Cygwin.'
        exit 1
    fi

    SVC_NAME=minifi
    if [ "x$2" != "x" ] ; then
        SVC_NAME=$2
    fi

    initd_dir='/etc/init.d'
    SVC_FILE="${initd_dir}/${SVC_NAME}"

    if [ ! -w  "${initd_dir}" ]; then
        echo "Current user does not have write permissions to ${initd_dir}. Cannot install MiNiFi as a service."
        exit 1
    fi

# Create the init script, overwriting anything currently present
cat <<SERVICEDESCRIPTOR > ${SVC_FILE}
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
#
# chkconfig: 2345 20 80
# description: Apache NiFi MiNiFi is a subproject of Apache nifi to collect data where it originates.
#
# Make use of the configured MINIFI_HOME directory and pass service requests to the minifi executable
export MINIFI_HOME=${MINIFI_HOME}
bin_dir=\${MINIFI_HOME}/bin
minifi_executable=\${bin_dir}/minifi
pid_file=${bin_dir}/.minifi.pid

# determines the pid
get_pid() {
  # Default to a -1 for pid
  pid=-1
  # Check to see if we have a pid file
  if [ -f \${pid_file} ]; then
    pid=\$(cat ${pid_file})
  fi
  echo \${pid}
}

# Performs a check to see if the provided pid is one that currently exists
active_pid() {
  pid=\${1}
  kill -s 0 \${pid} > /dev/null 2>&1
  echo \$?
}

saved_pid=\$(get_pid)

case "\$1" in
    start)
      if [ "\${saved_pid}" -gt 0 ]; then
        if [ \$(active_pid \${saved_pid}) -ne 0 ]; then
            echo "PID \${saved_pid} is stale, removing pid file at \${pid_file}";
            if ! rm -f \${pid_file}; then
              echo "Could not remove \${pid_file}. File will need to be manually removed."
              exit 1;
            fi
        else
            echo "MINIFI is currently running (PID: \${saved_pid}) with pid file \${pid_file}."
            exit 0;
        fi
      fi
      \${minifi_executable} &
      pid=\$!
      echo \${pid} > \${pid_file}
      echo Starting MiNiFi with PID \${pid} and pid file \${pid_file}
      ;;
    stop)
      if [ \$(active_pid \${saved_pid}) -ne 0 ]; then
        echo "MiNiFi is not currently running."
      else
        echo "Stopping MiNiFi (PID: \${saved_pid})."
        # Send a SIGINT to MiNiFi so that the handler begins shutdown.
        kill -2 \${saved_pid} > /dev/null 2>&1
        if [ \$? -ne 0 ]; then
          echo "Could not successfully send termination signal to MiNiFi (PID: \${saved_pid})"
          exit 1;
        else
          # Clean up our pid file
          rm -f \${pid_file}
        fi
      fi
      ;;
    run)
      if [ "\${saved_pid}" -gt 0 ]; then
        if ! active_pid \${saved_pid}; then
            echo "PID \${saved_pid} is stale, removing pid file at \${pid_file}";
            if ! rm -f \${pid_file}; then
              echo "Could not remove \${pid_file}. File will need to be manually removed."
              exit 1;
            fi
        else
            echo "MINIFI is currently running (PID: \${saved_pid}) with pid file \${pid_file}."
            exit 0;
        fi
      fi
      echo running
      \${minifi_executable}
      ;;
    status)
        # interpret status as per LSB specifications
        # see:  http://refspecs.linuxbase.org/LSB_3.1.0/LSB-Core-generic/LSB-Core-generic/iniscrptact.html

        if [ "\${saved_pid}" -gt 0 ]; then
          if [ \$(active_pid \${saved_pid}) -ne 0 ]; then
            # program is dead and pid file exists
            echo "Program is not currently running but stale pid file (\${pid_file}) exists.";
            exit 1
          else
            # pid is correct, program is running
            echo "MINIFI is currently running (PID: \${saved_pid}) with pid file \${pid_file}."
            exit 0;
          fi
        else
          # program is not running
          echo "MiNiFi is not currently running."
          exit 3;
        fi
        ;;
    restart)
        echo Restarting MiNiFi service
        \${bin_dir}/minifi.sh stop
        \${bin_dir}/minifi.sh start
        ;;
    *)
        echo "Usage: service minifi {start|stop|restart|status}"
        ;;
esac

SERVICEDESCRIPTOR

    if [ ! -f "${SVC_FILE}" ]; then
        echo "Could not create service file ${SVC_FILE}"
        exit 1
    fi

    # Provide the user execute access on the file
    chmod u+x ${SVC_FILE}

    rm -f "/etc/rc2.d/S65${SVC_NAME}"
    ln -s "/etc/init.d/${SVC_NAME}" "/etc/rc2.d/S65${SVC_NAME}" || { echo "Could not create link /etc/rc2.d/S65${SVC_NAME}"; exit 1; }
    rm -f "/etc/rc2.d/K65${SVC_NAME}"
    ln -s "/etc/init.d/${SVC_NAME}" "/etc/rc2.d/K65${SVC_NAME}" || { echo "Could not create link /etc/rc2.d/K65${SVC_NAME}"; exit 1; }
    echo "Service ${SVC_NAME} installed"
}

saved_pid=$(get_pid)

case "$1" in
    start)
      if [ "${saved_pid}" -gt 0 ]; then
        if [ $(active_pid ${saved_pid}) -ne 0 ]; then
            echo "PID ${saved_pid} is stale, removing pid file at ${pid_file}";
            if ! rm -f ${pid_file}; then
              echo "Could not remove ${pid_file}. File will need to be manually removed."
              exit 1;
            fi
        else
            echo "MINIFI is currently running (PID: ${saved_pid}) with pid file ${pid_file}."
            exit 0;
        fi
      fi
      ${minifi_executable} &
      pid=$!
      echo ${pid} > ${pid_file}
      echo Starting MiNiFi with PID ${pid} and pid file ${pid_file}
      ;;
    stop)
      if [ $(active_pid ${saved_pid}) -le 0 ]; then
        echo "MiNiFi is not currently running."
      else
        echo "Stopping MiNiFi (PID: ${saved_pid})."
        # Send a SIGINT to MiNiFi so that the handler begins shutdown.
        kill -2 ${saved_pid} > /dev/null 2>&1
        if [ $? -ne 0 ]; then
          echo "Could not successfully send termination signal to MiNiFi (PID: ${saved_pid})"
          exit 1;
        else
          # Clean up our pid file
          rm -f ${pid_file}
        fi
      fi
      ;;
    run)
      if [ "${saved_pid}" -gt 0 ]; then
        if [ $(active_pid ${saved_pid}) -ne 0 ]; then
            echo "PID ${saved_pid} is stale, removing pid file at ${pid_file}";
            if ! rm -f ${pid_file}; then
              echo "Could not remove ${pid_file}. File will need to be manually removed."
              exit 1;
            fi
        else
            echo "MINIFI is currently running (PID: ${saved_pid}) with pid file ${pid_file}."
            exit 0;
        fi
      fi
      ${minifi_executable}
      ;;
    status)
      # interpret status as per LSB specifications
      # see:  http://refspecs.linuxbase.org/LSB_3.1.0/LSB-Core-generic/LSB-Core-generic/iniscrptact.html

      if [ "${saved_pid}" -gt 0 ]; then
        if [ $(active_pid ${saved_pid}) -ne 0 ]; then
          # program is dead and pid file exists
          echo "Program is not currently running but stale pid file (${pid_file}) exists.";
          exit 1
        else
          # pid is correct, program is running
          echo "MINIFI is currently running (PID: ${saved_pid}) with pid file ${pid_file}."
          exit 0;
        fi
      else
        # program is not running
        echo "MiNiFi is not currently running."
        exit 3;
      fi
      ;;
    restart)
      echo Restarting MiNiFi service
      ${bin_dir}/minifi.sh stop
      ${bin_dir}/minifi.sh start
      ;;
    install)
      install "$@"
      ;;
    *)
      echo "Usage: minifi.sh {start|stop|run|restart|status|install}"
      ;;
esac
