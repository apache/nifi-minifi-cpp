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

set(MINIFI_VERSION_STR ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})

function(ADD_PACKAGE_VERIFY TAG_PREFIX)
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
            docker-verify-${TAG_PREFIX}-modular
            COMMAND ${CMAKE_SOURCE_DIR}/docker/RunBehaveTests.sh --image-tag-prefix ${TAG_PREFIX} ${MINIFI_VERSION_STR} ${ENABLED_TAGS} --tags_to_exclude=${DISABLED_TAGS} --parallel_processes=${DOCKER_VERIFY_THREADS})
    add_custom_target(
            docker-verify-${TAG_PREFIX}-modular-fips
            COMMAND ${CMAKE_SOURCE_DIR}/docker/RunBehaveTests.sh --image-tag-prefix ${TAG_PREFIX} ${MINIFI_VERSION_STR} ${ENABLED_TAGS} --tags_to_exclude=${DISABLED_TAGS} --parallel_processes=${DOCKER_VERIFY_THREADS} --fips)
endfunction()


CREATE_DOCKER_TARGET_FROM_ROCKY_PACKAGE(rockylinux:8 rocky8 "dnf install -y wget python3.12-devel python3.12-pip gcc gcc-c++ findutils")
CREATE_DOCKER_TARGET_FROM_ROCKY_PACKAGE(rockylinux:9 rocky9 "dnf install -y wget python3-devel python3-pip gcc gcc-c++ findutils")
CREATE_DOCKER_TARGET_FROM_ROCKY_PACKAGE(rockylinux/rockylinux:10 rocky10 "dnf install -y wget python3-devel python3-pip gcc gcc-c++ findutils")
CREATE_DOCKER_TARGET_FROM_ROCKY_PACKAGE(ubuntu:jammy jammy "apt update \\&\\& apt install -y wget python3-dev python3-venv python3-pip")
CREATE_DOCKER_TARGET_FROM_ROCKY_PACKAGE(ubuntu:noble noble "apt update \\&\\& apt install -y wget python3-dev python3-venv python3-pip")
CREATE_DOCKER_TARGET_FROM_ROCKY_PACKAGE(debian:bookworm bookworm "apt update \\&\\& apt install -y wget python3-dev python3-venv python3-pip")
CREATE_DOCKER_TARGET_FROM_ROCKY_PACKAGE(debian:bullseye bullseye "apt update \\&\\& apt install -y wget python3-dev python3-venv python3-pip")
CREATE_DOCKER_TARGET_FROM_ROCKY_PACKAGE(debian:trixie trixie "apt update \\&\\& apt install -y wget python3-dev python3-venv python3-pip")
CREATE_DOCKER_TARGET_FROM_RPM_PACKAGE(rockylinux:8 rocky8 "dnf install -y wget python3.12-devel python3.12-pip gcc gcc-c++")
CREATE_DOCKER_TARGET_FROM_RPM_PACKAGE(rockylinux:9 rocky9 "dnf install -y wget python3-devel python3-pip gcc gcc-c++")
CREATE_DOCKER_TARGET_FROM_RPM_PACKAGE(rockylinux/rockylinux:10 rocky10 "dnf install -y wget python3-devel python3-pip gcc gcc-c++")

ADD_PACKAGE_VERIFY(rocky8)
ADD_PACKAGE_VERIFY(rocky9)
ADD_PACKAGE_VERIFY(rocky10)
ADD_PACKAGE_VERIFY(jammy)
ADD_PACKAGE_VERIFY(noble)
ADD_PACKAGE_VERIFY(bookworm)
ADD_PACKAGE_VERIFY(bullseye)
ADD_PACKAGE_VERIFY(trixie)
ADD_PACKAGE_VERIFY(rocky8-rpm)
ADD_PACKAGE_VERIFY(rocky9-rpm)
ADD_PACKAGE_VERIFY(rocky10-rpm)
