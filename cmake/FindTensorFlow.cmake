# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

include(FindPackageHandleStandardArgs)
unset(TENSORFLOW_FOUND)

find_path(TENSORFLOW_INCLUDE_DIR
          NAMES
          tensorflow/core
          tensorflow/cc
          third_party
          HINTS
          /usr/local/include/google/tensorflow
          /usr/local/include/tensorflow
          /usr/include/google/tensorflow)

find_library(TENSORFLOW_CC_LIBRARY NAMES tensorflow_cc
             HINTS
             /usr/lib
             /usr/local/lib
             /usr/local/lib/tensorflow_cc)

find_library(TENSORFLOW_FRAMEWORK_LIBRARY NAMES tensorflow_framework
             HINTS
             /usr/lib
             /usr/local/lib
             /usr/local/lib/tensorflow_cc)

find_package_handle_standard_args(TENSORFLOW DEFAULT_MSG TENSORFLOW_INCLUDE_DIR TENSORFLOW_CC_LIBRARY TENSORFLOW_FRAMEWORK_LIBRARY)

if(TENSORFLOW_FOUND)
    set(TENSORFLOW_LIBRARIES ${TENSORFLOW_CC_LIBRARY} ${TENSORFLOW_FRAMEWORK_LIBRARY})
    set(TENSORFLOW_INCLUDE_DIRS
        ${TENSORFLOW_INCLUDE_DIR}
        ${TENSORFLOW_INCLUDE_DIR}/bazel-genfiles
        ${TENSORFLOW_INCLUDE_DIR}/tensorflow/contrib/makefile/downloads
        ${TENSORFLOW_INCLUDE_DIR}/tensorflow/contrib/makefile/downloads/eigen
        ${TENSORFLOW_INCLUDE_DIR}/tensorflow/contrib/makefile/downloads/gemmlowp
        ${TENSORFLOW_INCLUDE_DIR}/tensorflow/contrib/makefile/downloads/nsync/public
        ${TENSORFLOW_INCLUDE_DIR}/tensorflow/contrib/makefile/gen/protobuf-host/include)
endif()

mark_as_advanced(TENSORFLOW_INCLUDE_DIR TENSORFLOW_CC_LIBRARY TENSORFLOW_FRAMEWORK_LIBRARY)