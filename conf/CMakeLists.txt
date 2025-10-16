# Licensed to the Apache Software Foundation (ASF) under one
#
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
#

if (MINIFI_PACKAGING_TYPE STREQUAL "RPM")
    set(MINIFI_PATH_FLOW_CONFIG "/${CMAKE_INSTALL_SYSCONFDIR}/${PROJECT_NAME}/config.yml")
    set(MINIFI_PATH_EXTENSIONS "/usr/${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME}/extensions/*")
    set(MINIFI_PATH_PROVENANCE_REPO "/${CMAKE_INSTALL_LOCALSTATEDIR}/lib/${PROJECT_NAME}/provenance_repository")
    set(MINIFI_PATH_FLOWFILE_REPO "/${CMAKE_INSTALL_LOCALSTATEDIR}/lib/${PROJECT_NAME}/flowfile_repository")
    set(MINIFI_PATH_CONTENT_REPO "/${CMAKE_INSTALL_LOCALSTATEDIR}/lib/${PROJECT_NAME}/content_repository")
    set(MINIFI_PATH_ASSET_DIR "/${CMAKE_INSTALL_LOCALSTATEDIR}/lib/${PROJECT_NAME}/asset")
    set(MINIFI_PATH_PYTHON_PROCESSORS "/${CMAKE_INSTALL_LOCALSTATEDIR}/lib/${PROJECT_NAME}/minifi-python")
    set(MINIFI_PATH_PYTHON_VENV "/${CMAKE_INSTALL_LOCALSTATEDIR}/lib/${PROJECT_NAME}/minifi-python-env")
    set(MINIFI_PATH_LOG_DIR "/${CMAKE_INSTALL_LOCALSTATEDIR}/log/${PROJECT_NAME}/")
elseif (MINIFI_PACKAGING_TYPE STREQUAL "TGZ")
    set(MINIFI_PATH_FLOW_CONFIG "./conf/config.yml")
    set(MINIFI_PATH_EXTENSIONS "../extensions/*")
    set(MINIFI_PATH_PROVENANCE_REPO "\${MINIFI_HOME}/provenance_repository")
    set(MINIFI_PATH_FLOWFILE_REPO "\${MINIFI_HOME}/flowfile_repository")
    set(MINIFI_PATH_CONTENT_REPO "\${MINIFI_HOME}/content_repository")
    set(MINIFI_PATH_ASSET_DIR "\${MINIFI_HOME}/asset")
    set(MINIFI_PATH_PYTHON_PROCESSORS "\${MINIFI_HOME}/minifi-python/")
    set(MINIFI_PATH_PYTHON_VENV "\${MINIFI_HOME}/minifi-python-env")
    set(MINIFI_PATH_LOG_DIR "\${MINIFI_HOME}/logs")
else()
    message(FATAL_ERROR "Invalid MINIFI_PACKAGING_TYPE")
endif()

configure_file(
        minifi.properties.in
        ${CMAKE_BINARY_DIR}/conf/minifi.properties
        @ONLY
)

configure_file(
        minifi-log.properties.in
        ${CMAKE_BINARY_DIR}/conf/minifi-log.properties
        @ONLY
)

configure_file(
        minifi-uid.properties.in
        ${CMAKE_BINARY_DIR}/conf/minifi-uid.properties
        @ONLY
)

# Exclude conf for windows since we'll be doing the work in the WiX template
if (WIN32)
    return()
endif()

if (MINIFI_PACKAGING_TYPE STREQUAL "RPM")
    install(FILES
            ${CMAKE_BINARY_DIR}/conf/minifi.properties
            ${CMAKE_BINARY_DIR}/conf/minifi-log.properties
            ${CMAKE_BINARY_DIR}/conf/minifi-uid.properties
            DESTINATION /${CMAKE_INSTALL_SYSCONFDIR}/${PROJECT_NAME}
            COMPONENT bin)
elseif (MINIFI_PACKAGING_TYPE STREQUAL "TGZ")
    install(FILES
            ${CMAKE_BINARY_DIR}/conf/minifi.properties
            ${CMAKE_BINARY_DIR}/conf/minifi-log.properties
            ${CMAKE_BINARY_DIR}/conf/minifi-uid.properties
            DESTINATION conf
            COMPONENT bin)
else()
    message(FATAL_ERROR "Invalid MINIFI_PACKAGING_TYPE")
endif()
