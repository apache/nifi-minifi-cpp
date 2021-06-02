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

# Create a custom build target called "docker" that will invoke DockerBuild.sh and create the NiFi-MiNiFi-CPP Docker image
add_custom_target(
    docker
    COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerBuild.sh
        -u 1000
        -g 1000
        -v ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
        -i release
        -c ENABLE_ALL=${ENABLE_ALL}
        -c ENABLE_PYTHON=${ENABLE_PYTHON}
        -c ENABLE_OPS=${ENABLE_OPS}
        -c ENABLE_JNI=${ENABLE_JNI}
        -c ENABLE_OPENCV=${ENABLE_OPENCV}
        -c ENABLE_OPC=${ENABLE_OPC}
        -c ENABLE_GPS=${ENABLE_GPS}
        -c ENABLE_COAP=${ENABLE_COAP}
        -c ENABLE_WEL=${ENABLE_WEL}
        -c ENABLE_SQL=${ENABLE_SQL}
        -c ENABLE_MQTT=${ENABLE_MQTT}
        -c ENABLE_PCAP=${ENABLE_PCAP}
        -c ENABLE_LIBRDKAFKA=${ENABLE_LIBRDKAFKA}
        -c ENABLE_SENSORS=${ENABLE_SENSORS}
        -c ENABLE_USB_CAMERA=${ENABLE_USB_CAMERA}
        -c ENABLE_TENSORFLOW=${ENABLE_TENSORFLOW}
        -c ENABLE_AWS=${ENABLE_AWS}
        -c ENABLE_BUSTACHE=${ENABLE_BUSTACHE}
        -c ENABLE_SFTP=${ENABLE_SFTP}
        -c ENABLE_OPENWSMAN=${ENABLE_OPENWSMAN}
        -c ENABLE_AZURE=${ENABLE_AZURE}
        -c DISABLE_CURL=${DISABLE_CURL}
        -c DISABLE_JEMALLOC=${DISABLE_JEMALLOC}
        -c DISABLE_CIVET=${DISABLE_CIVET}
        -c DISABLE_EXPRESSION_LANGUAGE=${DISABLE_EXPRESSION_LANGUAGE}
        -c DISABLE_ROCKSDB=${DISABLE_ROCKSDB}
        -c DISABLE_LIBARCHIVE=${DISABLE_LIBARCHIVE}
        -c DISABLE_LZMA=${DISABLE_LZMA}
        -c DISABLE_BZIP2=${DISABLE_BZIP2}
        -c DISABLE_SCRIPTING=${DISABLE_SCRIPTING}
        -c DISABLE_PYTHON_SCRIPTING=${DISABLE_PYTHON_SCRIPTING}
        -c DISABLE_CONTROLLER=${DISABLE_CONTROLLER}
        -c DOCKER_BASE_IMAGE=${DOCKER_BASE_IMAGE}
        -c BUILD_NUMBER=${BUILD_NUMBER}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docker/)

# Create minimal docker image
add_custom_target(
    docker-minimal
    COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerBuild.sh
        -u 1000
        -g 1000
        -v ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
        -i minimal
        -c DOCKER_BASE_IMAGE=${DOCKER_BASE_IMAGE}
        -c BUILD_NUMBER=${BUILD_NUMBER}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docker/)

add_custom_target(
    centos
    COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerBuild.sh
        -u 1000
        -g 1000
        -v ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
        -i release
        -c ENABLE_JNI=${ENABLE_JNI}
        -l ${CMAKE_BINARY_DIR}
        -d centos
        -c BUILD_NUMBER=${BUILD_NUMBER}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docker/)

add_custom_target(
    debian
    COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerBuild.sh
        -u 1000
        -g 1000
        -v ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
        -i release
        -c ENABLE_JNI=${ENABLE_JNI}
        -l ${CMAKE_BINARY_DIR}
        -d debian
        -c BUILD_NUMBER=${BUILD_NUMBER}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docker/)

add_custom_target(
    fedora
    COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerBuild.sh
        -u 1000
        -g 1000
        -v ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
        -i release
        -c ENABLE_JNI=${ENABLE_JNI}
        -l ${CMAKE_BINARY_DIR}
        -d fedora
        -c BUILD_NUMBER=${BUILD_NUMBER}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docker/)

add_custom_target(
    u18
    COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerBuild.sh
        -u 1000
        -g 1000
        -v ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
        -i release
        -c ENABLE_JNI=${ENABLE_JNI}
        -l ${CMAKE_BINARY_DIR}
        -d bionic
        -c BUILD_NUMBER=${BUILD_NUMBER}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docker/)

add_custom_target(
    u20
    COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerBuild.sh
        -u 1000
        -g 1000
        -v ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
        -i release
        -c ENABLE_JNI=${ENABLE_JNI}
        -l ${CMAKE_BINARY_DIR}
        -d focal
        -c BUILD_NUMBER=${BUILD_NUMBER}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docker/)

add_custom_target(
    docker-verify
    COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerVerify.sh ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})
