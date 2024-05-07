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

function(ADD_DOCKER_VERIFY_PYTHON TAG_PREFIX HAS_MODULES)
    if (HAS_MODULES)
        add_custom_target(
                docker-verify-${TAG_PREFIX}
                COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerVerify.sh --image-tag-prefix ${TAG_PREFIX} ${MINIFI_VERSION_STR} ENABLE_PYTHON_SCRIPTING)
    else()
        add_custom_target(
                docker-verify-${TAG_PREFIX}
                COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerVerify.sh --image-tag-prefix ${TAG_PREFIX} ${MINIFI_VERSION_STR} ENABLE_PYTHON_SCRIPTING --tags_to_exclude NEEDS_NUMPY)
    endif()
endfunction()

function(ADD_CONDA_TO_DOCKER TAG_PREFIX)
    add_custom_target(
            conda_${TAG_PREFIX}_from_rocky_package
            COMMAND DOCKER_BUILDKIT=1 docker build
            --build-arg BASE_IMAGE=apacheminificpp:${TAG_PREFIX}-${MINIFI_VERSION_STR}
            -t apacheminificpp:conda_${TAG_PREFIX}-${MINIFI_VERSION_STR}
            -f ${CMAKE_SOURCE_DIR}/docker/python-verify/conda.Dockerfile
            ${CMAKE_BINARY_DIR})
endfunction()

function(ADD_VENV_TO_DOCKER TAG_PREFIX)
    add_custom_target(
            venv_${TAG_PREFIX}_from_rocky_package
            COMMAND DOCKER_BUILDKIT=1 docker build
            --build-arg BASE_IMAGE=apacheminificpp:${TAG_PREFIX}-${MINIFI_VERSION_STR}
            -t apacheminificpp:venv_${TAG_PREFIX}-${MINIFI_VERSION_STR}
            -f ${CMAKE_SOURCE_DIR}/docker/python-verify/venv.Dockerfile
            ${CMAKE_BINARY_DIR})
endfunction()


CREATE_DOCKER_TARGET_FROM_ROCKY_PACKAGE(debian:bullseye patched_bullseye_py "apt update \\&\\& apt install -y patchelf libpython3-dev python3-venv python3-pip wget \\&\\& patchelf /opt/minifi/minifi-current/extensions/libminifi-python-script-extension.so --replace-needed libpython3.so libpython3.9.so")
CREATE_DOCKER_TARGET_FROM_ROCKY_PACKAGE(ubuntu:jammy patched_jammy_py "apt update \\&\\& apt install -y patchelf libpython3.10-dev python3.10-venv python3-pip wget \\&\\& patchelf /opt/minifi/minifi-current/extensions/libminifi-python-script-extension.so --replace-needed libpython3.so libpython3.10.so.1.0")
CREATE_DOCKER_TARGET_FROM_ROCKY_PACKAGE(rockylinux:8 rocky8_py "yum install -y python3-libs python3-pip python3-devel gcc-c++ wget")
CREATE_DOCKER_TARGET_FROM_ROCKY_PACKAGE(rockylinux:9 rocky9_py "yum install -y python3-libs python3-pip wget")
CREATE_DOCKER_TARGET_FROM_ROCKY_PACKAGE(ubuntu:jammy jammy_py "apt update \\&\\& apt install -y wget")
ADD_CONDA_TO_DOCKER(jammy_py)
ADD_VENV_TO_DOCKER(rocky9_py)

if (EXISTS ${CMAKE_SOURCE_DIR}/docker/test/integration/features)
    ADD_DOCKER_VERIFY_PYTHON(rocky8_py FALSE)
    ADD_DOCKER_VERIFY_PYTHON(rocky9_py FALSE)
    ADD_DOCKER_VERIFY_PYTHON(patched_jammy_py FALSE)
    ADD_DOCKER_VERIFY_PYTHON(patched_bullseye_py FALSE)
    ADD_DOCKER_VERIFY_PYTHON(conda_jammy_py TRUE)
    ADD_DOCKER_VERIFY_PYTHON(venv_rocky9_py TRUE)
endif()
