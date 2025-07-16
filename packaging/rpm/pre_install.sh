#!/bin/sh

if ! getent group minificpp >/dev/null; then
    groupadd -r minificpp
fi

if ! getent passwd minificpp >/dev/null; then
    useradd -r -g minificpp -s /sbin/nologin -d /var/lib/nifi-minifi-cpp -c "NiFi MiNiFi C++ Service Account" minificpp
fi

exit 0