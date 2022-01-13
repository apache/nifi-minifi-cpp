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

if (NOT (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND ENABLE_KUBERNETES) OR OPENSSL_OFF)
    return()
endif()

include(KubernetesClientC)

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES "*.cpp" "controllerservice/*.cpp" "processors/*.cpp")
add_library(minifi-kubernetes-extensions SHARED ${SOURCES})
target_link_libraries(minifi-kubernetes-extensions ${LIBMINIFI} kubernetes CURL::libcurl)

register_extension(minifi-kubernetes-extensions "KUBERNETES EXTENSIONS" KUBERNETES-EXTENSIONS "This enables Kubernetes support" "extensions/kubernetes/tests")
register_extension_linter(minifi-kubernetes-extensions-linter)
