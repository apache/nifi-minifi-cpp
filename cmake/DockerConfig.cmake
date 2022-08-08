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
        -o ${MINIFI_DOCKER_OPTIONS_STR}
        -c DOCKER_BASE_IMAGE=${DOCKER_BASE_IMAGE}
        -c DOCKER_CCACHE_DUMP_LOCATION=${DOCKER_CCACHE_DUMP_LOCATION}
        -c DOCKER_SKIP_TESTS=${DOCKER_SKIP_TESTS}
        -c BUILD_NUMBER=${BUILD_NUMBER}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docker/)

# Create minimal docker image
add_custom_target(
    docker-minimal
    COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerBuild.sh
        -u 1000
        -g 1000
        -t minimal
        -v ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
        -o \"-DENABLE_PYTHON=OFF
             -DENABLE_LIBRDKAFKA=ON
             -DENABLE_AWS=ON
             -DENABLE_AZURE=ON
             -DDISABLE_CONTROLLER=ON
             -DENABLE_SCRIPTING=OFF
             -DDISABLE_PYTHON_SCRIPTING=ON
             -DENABLE_ENCRYPT_CONFIG=OFF \"
        -c DOCKER_BASE_IMAGE=${DOCKER_BASE_IMAGE}
        -c DOCKER_SKIP_TESTS=${DOCKER_SKIP_TESTS}
        -c BUILD_NUMBER=${BUILD_NUMBER}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docker/)

add_custom_target(
    centos
    COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerBuild.sh
        -u 1000
        -g 1000
        -v ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
        -o ${MINIFI_DOCKER_OPTIONS_STR}
        -l ${CMAKE_BINARY_DIR}
        -d centos
        -c BUILD_NUMBER=${BUILD_NUMBER}
        -c DOCKER_CCACHE_DUMP_LOCATION=${DOCKER_CCACHE_DUMP_LOCATION}
        -c DOCKER_SKIP_TESTS=${DOCKER_SKIP_TESTS}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docker/)

add_custom_target(
    fedora
    COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerBuild.sh
        -u 1000
        -g 1000
        -v ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
        -o ${MINIFI_DOCKER_OPTIONS_STR}
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
        -o ${MINIFI_DOCKER_OPTIONS_STR}
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
        -o ${MINIFI_DOCKER_OPTIONS_STR}
        -l ${CMAKE_BINARY_DIR}
        -d focal
        -c BUILD_NUMBER=${BUILD_NUMBER}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docker/)

if (EXISTS ${CMAKE_SOURCE_DIR}/docker/test/integration/features)
    add_subdirectory(${CMAKE_SOURCE_DIR}/docker/test/integration/features)

    add_custom_target(
        docker-verify-all
        COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerVerify.sh ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH} ${ALL_BEHAVE_TESTS})

    add_custom_target(
        docker-verify
        COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerVerify.sh ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH} ${ENABLED_BEHAVE_TESTS})
endif()
