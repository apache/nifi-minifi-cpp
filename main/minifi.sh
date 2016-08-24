#!/bin/bash
#
# minifi     This shell script takes care of starting and stopping minifi
#
#
### BEGIN INIT INFO
# Provides: minifi
# Short-Description: start and stop minifi
### END INIT INFO

SCRIPT_DIR=$(dirname "$0")
SCRIPT_NAME=$(basename "$0")
NIFI_HOME=$(cd "${SCRIPT_DIR}" && cd .. && pwd)
SHUTDOWN_WAIT=10

minifi_pid() {
    echo `ps aux | grep target/minifi | grep -v grep | awk '{ print $2 }'`
}

start() {
    pid=$(minifi_pid)
    if [ -n "$pid" ]
    then
        echo "minifi is already running (pid: $pid)"
    else
        # Start minifi
        echo "Starting minifi"
        cd $NIFI_HOME && $NIFI_HOME/target/minifi $@ &
    fi
    return 0
}

stop() {
    pid=$(minifi_pid)
    if [ -n "$pid" ]
    then
        echo "Stopping minifi"
	kill -INT $pid

    let kwait=$SHUTDOWN_WAIT
    count=0
    count_by=5
    until [ `ps -p $pid | grep -c $pid` = '0' ] || [ $count -gt $kwait ]
    do
        echo "Waiting for processes to exit. Timeout before we kill the pid: ${count}/${kwait}"
        sleep $count_by
        let count=$count+$count_by;
    done

    if [ $count -gt $kwait ]; then
        echo "Killing processes which didn't stop after $SHUTDOWN_WAIT seconds"
        kill -9 $pid
    fi
    else
        echo "minifi is not running"
    fi

    return 0
}

case $1 in
    start)
        start
        ;;
    stop)
        stop
        ;;
    restart)
        stop
        start
        ;;
    status)
       pid=$(minifi_pid)
        if [ -n "$pid" ]
        then
           echo "minifi is running with pid: $pid"
        else
           echo "minifi is not running"
        fi
        ;;
esac

exit 0
