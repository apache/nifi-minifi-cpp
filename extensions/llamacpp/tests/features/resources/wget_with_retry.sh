#!/bin/sh
set -u

URL=$1
DEST=$2

for i in 1 2 3 4 5 6; do
    if wget "${URL}" --directory-prefix="${DEST}"; then
        break
    elif [ $i -lt 6 ]; then
        sleep $((i * 5))
        echo "Attempt $i failed, retrying in $((i * 5))s..."
    fi
done

[ -f "${DEST}/$(basename "${URL}")" ] || exit 1
