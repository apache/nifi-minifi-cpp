#!/bin/bash
set -euo pipefail

# Usage: ./create_jks.sh <base directory> <ssl_key_path> <ssl_cert_path> <ca_cert_path>

DIR=$1
SSL_KEY_PATH=$2
SSL_CERT_PATH=$3
CA_CERT_PATH=$4

KEYSTORE="$DIR/keystore.jks"
TRUSTSTORE="$DIR/truststore.jks"
PKCS12_FILE="$DIR/keystore.p12"
PASSWORD="passw0rd1!"

cat "${CA_CERT_PATH}" >> "${SSL_CERT_PATH}"

if [ ! -d "$DIR" ]; then
    mkdir -p "$DIR"
fi

openssl pkcs12 -export \
  -inkey "$SSL_KEY_PATH" \
  -in "$SSL_CERT_PATH" \
  -name "nifi-key" \
  -out "$PKCS12_FILE" \
  -password pass:$PASSWORD

keytool -importkeystore \
  -destkeystore "$KEYSTORE" \
  -deststoretype jks \
  -destalias nifi-key \
  -srckeystore "$PKCS12_FILE" \
  -srcstoretype pkcs12 \
  -srcalias "nifi-key" \
  -storepass "$PASSWORD" \
  -srcstorepass "$PASSWORD" \
  -noprompt

keytool -importcert \
  -alias "nifi-cert" \
  -file "$CA_CERT_PATH" \
  -keystore "$TRUSTSTORE" \
  -storepass "$PASSWORD" \
  -noprompt
