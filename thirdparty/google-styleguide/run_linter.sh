#!/bin/bash
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# ./run_linter <includedir1> <includedir2> ... <includedirN> -- <srcdir1> <srcdir2> ... <srcdirN>

if [[ "$(uname)" == "Darwin" ]]; then
    SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
else
    SCRIPT=$(readlink -f $0)
    SCRIPT_DIR=`dirname $SCRIPT`
fi

while (( $# )) ; do
    [ x"$1" == x"--" ] && break
    INCLUDE_DIRS="$INCLUDE_DIRS $1"
    shift
done

while (( $# )) ; do
    SOURCE_DIRS="$SOURCE_DIRS $1"
    shift
done

[ x"$INCLUDE_DIRS" == x"" ] && echo "WARNING: No include directories specified."
[ x"$SOURCE_DIRS" == x"" ] && echo "ERROR: No source directories specified." && exit 1

HEADERS=`find $INCLUDE_DIRS -name '*.h' | sort | uniq | tr '\n' ','`
SOURCES=`find $SOURCE_DIRS -name  '*.cpp' | sort | uniq | tr '\n' ' '`
python2 ${SCRIPT_DIR}/cpplint.py --linelength=200 --headers=${HEADERS} ${SOURCES}
