#!/bin/bash

htpasswd -bc /etc/nginx/.htpasswd "${BASIC_USERNAME}" "${BASIC_PASSWORD}"

# shellcheck disable=SC2016
envsubst '${FORWARD_HOST} ${FORWARD_PORT}' < /nginx.conf.template > /etc/nginx/conf.d/default.conf

nginx -g "daemon off;"
