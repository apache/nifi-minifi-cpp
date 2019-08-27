#!/bin/sh

function create_ca {
  if [ $# -ne 2 ]; then
    exit -1
  fi
  name=$1
  cn=$2

  openssl genrsa -out "$name.key"
  openssl req -x509 -new -nodes -key "$name.key" -sha256 -days 10950 -subj "/C=US/ST=CA/O=Example, Inc./CN=$cn" -out "$name.crt"
}

function create_cert {
  if [ $# -ne 3 ]; then
    exit -1
  fi
  ca=$1
  name=$2
  cn=$3

  openssl genrsa -out "$name.key"
  openssl req -new -sha256 -key "$name.key" -subj "/C=US/ST=CA/O=Example, Inc./CN=$cn" -out "$name.csr"
  serial_arg=""
  if [ ! -f "$ca.srl" ]; then
    serial_arg="-CAcreateserial"
  fi
  openssl x509 -req -in "$name.csr" -CA "$ca.crt" -CAkey "$ca.key" $serial_arg -out "$name.crt" -days 3650 -sha256
  cat "$name.key" "$name.crt" > "$name.pem"
  openssl pkcs12 -export -out "$name.p12" -inkey "$name.key" -in "$name.crt" -password pass:Password12
  rm "$name.csr" "$name.key" "$name.crt"
}

# Generate good CA
create_ca "goodCA" "Good Root Certificate Authority"

# Generate server cert with good case CA
create_cert "goodCA" "server" "localhost"

# Generate good client cert with good CA
create_cert "goodCA" "goodCA_goodClient" "good.example.com"

# Generate bad client cert with good CA
create_cert "goodCA" "goodCA_badClient" "bad.example.com"

# Generate bad CA
create_ca "badCA" "Bad Root Certificate Authority"

# Generate good client cert with bad CA
create_cert "badCA" "badCA_goodClient" "good.example.com"

# Cleanup
rm goodCA.key goodCA.srl badCA.crt badCA.key badCA.srl
