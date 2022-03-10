FROM ubuntu:latest
CMD exec /bin/bash -c "echo Syslog TCP client started; while true; do logger --tcp -n minifi-cpp-flow -P 514 sample_log; sleep 1; done"
