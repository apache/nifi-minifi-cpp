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

set(PROJECT_VERSION_STR ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})
include(ProcessorCount)
ProcessorCount(PROCESSOR_COUNT)
set(DOCKER_VERIFY_THREADS "${PROCESSOR_COUNT}" CACHE STRING "Number of threads that docker-verify-modular can utilize")

# Create a custom build target called "docker" that will invoke DockerBuild.sh and create the NiFi-MiNiFi-CPP Docker image
add_custom_target(
    docker
    COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerBuild.sh
        -u 1000
        -g 1000
        -v ${PROJECT_VERSION_STR}
        -o ${MINIFI_DOCKER_OPTIONS_STR}
        -c DOCKER_BASE_IMAGE=${DOCKER_BASE_IMAGE}
        -c DOCKER_CCACHE_DUMP_LOCATION=${DOCKER_CCACHE_DUMP_LOCATION}
        -c DOCKER_SKIP_TESTS=${DOCKER_SKIP_TESTS}
        -c CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -c BUILD_NUMBER=${BUILD_NUMBER}
        -c DOCKER_PLATFORMS=${DOCKER_PLATFORMS}
        -c DOCKER_PUSH=${DOCKER_PUSH}
        -c DOCKER_TAGS=${DOCKER_TAGS}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docker/)

# Create minimal docker image
add_custom_target(
    docker-minimal
    COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerBuild.sh
        -u 1000
        -g 1000
        -p minimal
        -v ${PROJECT_VERSION_STR}
        -o \"-DENABLE_OPC=OFF
             -DENABLE_SYSTEMD=OFF
             -DENABLE_CIVET=OFF
             -DENABLE_SQL=OFF
             -DENABLE_MQTT=OFF
             -DENABLE_KAFKA=OFF
             -DENABLE_LUA_SCRIPTING=OFF
             -DENABLE_PYTHON_SCRIPTING=OFF
             -DENABLE_AWS=OFF
             -DENABLE_AZURE=OFF
             -DENABLE_ENCRYPT_CONFIG=OFF
             -DENABLE_SPLUNK=OFF
             -DENABLE_ELASTICSEARCH=OFF
             -DENABLE_GCP=OFF
             -DENABLE_KUBERNETES=OFF
             -DENABLE_PROMETHEUS=OFF
             -DENABLE_GRPC_FOR_LOKI=OFF
             -DENABLE_PROCFS=OFF
             -DENABLE_OPENCV=OFF
             -DENABLE_BUSTACHE=OFF
             -DENABLE_SFTP=OFF
             -DENABLE_COUCHBASE=OFF
             -DENABLE_TEST_PROCESSORS=OFF
             -DENABLE_ROCKSDB=ON
             -DENABLE_LIBARCHIVE=ON
             -DENABLE_LZMA=ON
             -DENABLE_BZIP2=ON
             -DCI_BUILD=${CI_BUILD}\"
        -c DOCKER_BASE_IMAGE=${DOCKER_BASE_IMAGE}
        -c DOCKER_SKIP_TESTS=${DOCKER_SKIP_TESTS}
        -c BUILD_NUMBER=${BUILD_NUMBER}
        -c CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -c DOCKER_PLATFORMS=${DOCKER_PLATFORMS}
        -c DOCKER_PUSH=${DOCKER_PUSH}
        -c DOCKER_TAGS=${DOCKER_TAGS}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docker/)

add_custom_target(
    centos
    COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerBuild.sh
        -u 1000
        -g 1000
        -v ${PROJECT_VERSION_STR}
        -o ${MINIFI_DOCKER_OPTIONS_STR}
        -l ${CMAKE_BINARY_DIR}
        -d centos
        -c BUILD_NUMBER=${BUILD_NUMBER}
        -c DOCKER_CCACHE_DUMP_LOCATION=${DOCKER_CCACHE_DUMP_LOCATION}
        -c DOCKER_SKIP_TESTS=${DOCKER_SKIP_TESTS}
        -c CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -c DOCKER_PLATFORMS=${DOCKER_PLATFORMS}
        -c DOCKER_PUSH=${DOCKER_PUSH}
        -c DOCKER_TAGS=${DOCKER_TAGS}
        -c DOCKER_BASE_IMAGE=${DOCKER_BASE_IMAGE}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docker/)

add_custom_target(
    rocky-test
    COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerBuild.sh
        -u 1000
        -g 1000
        -v ${PROJECT_VERSION_STR}
        -o ${MINIFI_DOCKER_OPTIONS_STR}
        -d rockylinux
        -c BUILD_NUMBER=${BUILD_NUMBER}
        -c DOCKER_CCACHE_DUMP_LOCATION=${DOCKER_CCACHE_DUMP_LOCATION}
        -c DOCKER_SKIP_TESTS=OFF
        -c CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -c DOCKER_PLATFORMS=${DOCKER_PLATFORMS}
        -c DOCKER_PUSH=${DOCKER_PUSH}
        -c DOCKER_TAGS=${DOCKER_TAGS}
        -c DOCKER_BASE_IMAGE=${DOCKER_BASE_IMAGE}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docker/)


add_custom_target(
    rocky
    COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerBuild.sh
        -u 1000
        -g 1000
        -v ${PROJECT_VERSION_STR}
        -o ${MINIFI_DOCKER_OPTIONS_STR}
        -l ${CMAKE_BINARY_DIR}
        -d rockylinux
        -c BUILD_NUMBER=${BUILD_NUMBER}
        -c DOCKER_CCACHE_DUMP_LOCATION=${DOCKER_CCACHE_DUMP_LOCATION}
        -c DOCKER_SKIP_TESTS=${DOCKER_SKIP_TESTS}
        -c DOCKER_CREATE_RPM=${DOCKER_CREATE_RPM}
        -c CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -c DOCKER_PLATFORMS=${DOCKER_PLATFORMS}
        -c DOCKER_PUSH=${DOCKER_PUSH}
        -c DOCKER_TAGS=${DOCKER_TAGS}
        -c DOCKER_BASE_IMAGE=${DOCKER_BASE_IMAGE}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docker/)

set(ENABLED_TAGS "CORE")
foreach(MINIFI_OPTION ${MINIFI_OPTIONS})
    string(FIND ${MINIFI_OPTION} "ENABLE" my_index)
    if(my_index EQUAL -1)
        continue()
    elseif(${${MINIFI_OPTION}})
        set(ENABLED_TAGS "${ENABLED_TAGS},${MINIFI_OPTION}")
    endif()
endforeach()

set(DISABLED_TAGS "SKIP_CI")

add_custom_target(
    docker-verify-modular
    COMMAND ${CMAKE_SOURCE_DIR}/docker/RunBehaveTests.sh ${PROJECT_VERSION_STR} ${ENABLED_TAGS} --tags_to_exclude=${DISABLED_TAGS} --parallel_processes=${DOCKER_VERIFY_THREADS})

function(CREATE_DOCKER_TARGET_FROM_ROCKY_PACKAGE BASE_IMAGE TAG_PREFIX INSTALL_PACKAGE_CMD)
    add_custom_target(
            ${TAG_PREFIX}_from_rocky_package
            COMMAND DOCKER_BUILDKIT=1 docker build
            --build-arg MINIFI_VERSION=${MINIFI_VERSION_STR}
            --build-arg BASE_IMAGE=${BASE_IMAGE}
            --build-arg ARCHIVE_LOCATION=nifi-minifi-cpp-${MINIFI_VERSION_STR}-bin-rockylinux.tar.gz
            --build-arg INSTALL_PACKAGE_CMD=${INSTALL_PACKAGE_CMD}
            -t apacheminificpp:${TAG_PREFIX}-${MINIFI_VERSION_STR}
            -f ${CMAKE_SOURCE_DIR}/docker/installed/installed.Dockerfile
            ${CMAKE_BINARY_DIR})
endfunction()

function(CREATE_DOCKER_TARGET_FROM_RPM_PACKAGE BASE_IMAGE TAG_PREFIX INSTALL_PACKAGE_CMD)
    add_custom_target(
            ${TAG_PREFIX}_from_rpm_package
            COMMAND DOCKER_BUILDKIT=1 docker build
            --build-arg MINIFI_VERSION=${MINIFI_VERSION_STR}
            --build-arg BASE_IMAGE=${BASE_IMAGE}
            --build-arg ARCHIVE_LOCATION=nifi-minifi-cpp-${MINIFI_VERSION_STR}-bin-rockylinux.rpm
            --build-arg INSTALL_PACKAGE_CMD=${INSTALL_PACKAGE_CMD}
            -t apacheminificpp:${TAG_PREFIX}-rpm-${MINIFI_VERSION_STR}
            -f ${CMAKE_SOURCE_DIR}/docker/installed/rpm.Dockerfile
            ${CMAKE_BINARY_DIR})
endfunction()

include(VerifyPackageWithDocker)
