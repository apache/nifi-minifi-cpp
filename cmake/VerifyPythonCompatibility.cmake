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

if (NOT (ENABLE_ALL OR ENABLE_PYTHON_SCRIPTING))
    return()
endif()

set(MINIFI_VERSION_STR ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})

function(ADD_DOCKER_TARGET_FROM_CENTOS BASE_IMAGE TAG_PREFIX INSTALL_PACKAGE_CMD)
    add_custom_target(
            ${TAG_PREFIX}_docker_from_centos_build
            COMMAND DOCKER_BUILDKIT=1 docker build
            --build-arg MINIFI_VERSION=${MINIFI_VERSION_STR}
            --build-arg BASE_IMAGE=${BASE_IMAGE}
            --build-arg ARCHIVE_LOCATION=nifi-minifi-cpp-${MINIFI_VERSION_STR}-bin-centos.tar.gz
            --build-arg INSTALL_PACKAGE_CMD=${INSTALL_PACKAGE_CMD}
            -t apacheminificpp:${TAG_PREFIX}-${MINIFI_VERSION_STR}
            -f ${CMAKE_SOURCE_DIR}/docker/python-verify/installed.Dockerfile
            ${CMAKE_BINARY_DIR})
endfunction()

function(ADD_DOCKER_VERIFY_PYTHON TAG_PREFIX HAS_MODULES)
    if (HAS_MODULES)
        add_custom_target(
                docker-verify-python-${TAG_PREFIX}
                COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerVerify.sh --image-tag-prefix ${TAG_PREFIX} ${MINIFI_VERSION_STR} ENABLE_PYTHON_SCRIPTING)
    else()
        add_custom_target(
                docker-verify-python-${TAG_PREFIX}
                COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerVerify.sh --image-tag-prefix ${TAG_PREFIX} ${MINIFI_VERSION_STR} ENABLE_PYTHON_SCRIPTING --tags_to_exclude NEEDS_NUMPY)
    endif()
endfunction()

function(ADD_CONDA_TO_DOCKER TAG_PREFIX)
    add_custom_target(
            conda_${TAG_PREFIX}_docker_from_centos_build
            COMMAND DOCKER_BUILDKIT=1 docker build
            --build-arg BASE_IMAGE=apacheminificpp:${TAG_PREFIX}-${MINIFI_VERSION_STR}
            -t apacheminificpp:conda_${TAG_PREFIX}-${MINIFI_VERSION_STR}
            -f ${CMAKE_SOURCE_DIR}/docker/python-verify/conda.Dockerfile
            ${CMAKE_BINARY_DIR})
endfunction()

function(ADD_VENV_TO_DOCKER TAG_PREFIX)
    add_custom_target(
            venv_${TAG_PREFIX}_docker_from_centos_build
            COMMAND DOCKER_BUILDKIT=1 docker build
            --build-arg BASE_IMAGE=apacheminificpp:${TAG_PREFIX}-${MINIFI_VERSION_STR}
            -t apacheminificpp:venv_${TAG_PREFIX}-${MINIFI_VERSION_STR}
            -f ${CMAKE_SOURCE_DIR}/docker/python-verify/venv.Dockerfile
            ${CMAKE_BINARY_DIR})
endfunction()


ADD_DOCKER_TARGET_FROM_CENTOS(debian:bullseye patched_bullseye "apt update \\&\\& apt install -y patchelf libpython3-dev python3-venv python3-pip wget \\&\\& patchelf /opt/minifi/minifi-current/extensions/libminifi-python-script-extension.so --replace-needed libpython3.so libpython3.9.so")
ADD_DOCKER_TARGET_FROM_CENTOS(ubuntu:jammy patched_jammy "apt update \\&\\& apt install -y patchelf libpython3.10-dev python3.10-venv python3-pip wget \\&\\& patchelf /opt/minifi/minifi-current/extensions/libminifi-python-script-extension.so --replace-needed libpython3.so libpython3.10.so.1.0")
ADD_DOCKER_TARGET_FROM_CENTOS(rockylinux:8 rocky8 "yum install -y python3-libs python3-pip python3-devel gcc-c++ wget")
ADD_DOCKER_TARGET_FROM_CENTOS(rockylinux:9 rocky9 "yum install -y python3-libs python3-pip wget")
ADD_DOCKER_TARGET_FROM_CENTOS(ubuntu:jammy jammy "apt update \\&\\& apt install -y wget")
ADD_CONDA_TO_DOCKER(jammy)
ADD_VENV_TO_DOCKER(rocky9)

if (EXISTS ${CMAKE_SOURCE_DIR}/docker/test/integration/features)
    ADD_DOCKER_VERIFY_PYTHON(rocky8 FALSE)
    ADD_DOCKER_VERIFY_PYTHON(rocky9 FALSE)
    ADD_DOCKER_VERIFY_PYTHON(patched_jammy FALSE)
    ADD_DOCKER_VERIFY_PYTHON(patched_bullseye FALSE)
    ADD_DOCKER_VERIFY_PYTHON(conda_jammy TRUE)
    ADD_DOCKER_VERIFY_PYTHON(venv_rocky9 TRUE)
endif()
