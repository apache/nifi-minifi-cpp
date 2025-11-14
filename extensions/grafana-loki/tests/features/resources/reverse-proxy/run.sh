#!/bin/sh
# Source: https://gist.github.com/laurentbel/c4c7696890fc71c8061172a932eb52e4
envsubst < nginx-basic-auth.conf > /etc/nginx/conf.d/default.conf
htpasswd -c -b /etc/nginx/.htpasswd "${BASIC_USERNAME}" "${BASIC_PASSWORD}"
exec nginx -g "daemon off;"
