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

if (TENSORFLOW_INCLUDE_PATH)
    message("-- Checking for TensorFlow includes in provided TENSORFLOW_INCLUDE_PATH: ${TENSORFLOW_INCLUDE_PATH}")
endif()

if (TENSORFLOW_LIB_PATH)
    message("-- Checking for TensorFlow libs in provided TENSORFLOW_LIB_PATH: ${TENSORFLOW_LIB_PATH}")
endif()

if (GOOGLE_PROTOBUF_INCLUDE_PATH)
    message("-- Checking for Google Protobuf includes in provided GOOGLE_PROTOBUF_INCLUDE_PATH: ${GOOGLE_PROTOBUF_INCLUDE_PATH}")
endif()

if (GOOGLE_PROTOBUF_LIB_PATH)
    message("-- Checking for Google Protobuf libs in provided GOOGLE_PROTOBUF_LIB_PATH: ${GOOGLE_PROTOBUF_LIB_PATH}")
endif()

find_path(TENSORFLOW_INCLUDE_DIR
          NAMES
          tensorflow/core
          tensorflow/cc
          third_party
          HINTS
          ${TENSORFLOW_INCLUDE_PATH}
          /usr/include/tensorflow
          /usr/local/include/google/tensorflow
          /usr/local/include/tensorflow
          /usr/local/include/tensorflow/bazel-bin/tensorflow/include
          /usr/include/google/tensorflow)

find_library(TENSORFLOW_CC_LIBRARY NAMES tensorflow_cc
             HINTS
             ${TENSORFLOW_LIB_PATH}
             ${TENSORFLOW_INCLUDE_PATH}/bazel-bin/tensorflow
             /usr/lib
             /usr/local/lib
             /usr/local/lib/tensorflow_cc)

find_path(GOOGLE_PROTOBUF_INCLUDE NAMES google/protobuf
          HINTS
          ${GOOGLE_PROTOBUF_INCLUDE_PATH}
          ${TENSORFLOW_INCLUDE_PATH}/src
          /usr/include/tensorflow/src
          /usr/local/include/google/tensorflow/src
          /usr/local/include/tensorflow/src
          /usr/local/include/tensorflow/bazel-bin/tensorflow/include/src
          /usr/include/google/tensorflow/src)

find_library(GOOGLE_PROTOBUF_LIBRARY NAMES protobuf
             HINTS
             ${GOOGLE_PROTOBUF_LIB_PATH}
             /usr/lib
             /usr/local/lib
             /usr/lib/x86_64-linux-gnu)

find_package_handle_standard_args(TENSORFLOW DEFAULT_MSG TENSORFLOW_INCLUDE_DIR TENSORFLOW_CC_LIBRARY GOOGLE_PROTOBUF_INCLUDE GOOGLE_PROTOBUF_LIBRARY)

if(TENSORFLOW_FOUND)
    message("-- Found TensorFlow includes: ${TENSORFLOW_INCLUDE_DIR}")
    message("-- Found TensorFlow libs: ${TENSORFLOW_CC_LIBRARY}")
    message("-- Found Google Protobuf includes: ${GOOGLE_PROTOBUF_INCLUDE}")
    message("-- Found Google Protobuf libs: ${GOOGLE_PROTOBUF_LIBRARY}")
    set(TENSORFLOW_LIBRARIES ${TENSORFLOW_CC_LIBRARY} ${GOOGLE_PROTOBUF_LIBRARY})
    set(TENSORFLOW_INCLUDE_DIRS
        ${TENSORFLOW_INCLUDE_DIR}
        ${TENSORFLOW_INCLUDE_DIR}/bazel-genfiles
        ${TENSORFLOW_INCLUDE_DIR}/tensorflow/contrib/makefile/downloads
        ${TENSORFLOW_INCLUDE_DIR}/tensorflow/contrib/makefile/downloads/eigen
        ${TENSORFLOW_INCLUDE_DIR}/tensorflow/contrib/makefile/downloads/gemmlowp
        ${TENSORFLOW_INCLUDE_DIR}/tensorflow/contrib/makefile/downloads/nsync/public
        ${TENSORFLOW_INCLUDE_DIR}/tensorflow/contrib/makefile/gen/protobuf-host/include
        ${GOOGLE_PROTOBUF_INCLUDE})
else()
    message(FATAL_ERROR "TensorFlow or Google Protobuf dependency was not found. Check or set TENSORFLOW_INCLUDE_PATH, TENSORFLOW_LIB_PATH, GOOGLE_PROTOBUF_INCLUDE, GOOGLE_PROTOBUF_LIBRARY to TensorFlow build, Install libtensorflow_cc.so, libprotobuf.so and headers into the system, or disable the TensorFlow extension.")
endif()

mark_as_advanced(TENSORFLOW_INCLUDE_DIR TENSORFLOW_CC_LIBRARY GOOGLE_PROTOBUF_INCLUDE GOOGLE_PROTOBUF_LIBRARY)
