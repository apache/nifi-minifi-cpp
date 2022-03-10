FROM ubuntu:latest
CMD exec /bin/bash -c "echo Syslog UDP client started; while true; do logger --udp -n minifi-cpp-flow -P 514 sample_log; sleep 1; done"
