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

# Exclude conf for windows since we'll be doing the work in the WiX template

if (WIN32)
    return()
endif()


if (MINIFI_PACKAGING_TYPE STREQUAL "RPM")
    set(MINIFI_HOME_ENV_VALUE "FHS")
    set(MINIFI_EXEC_PATH "/usr/bin/minifi")
elseif (MINIFI_PACKAGING_TYPE STREQUAL "TGZ")
    set(MINIFI_HOME_ENV_VALUE "/opt/minifi-cpp")
    set(MINIFI_EXEC_PATH "/opt/minifi-cpp/bin/minifi")
else()
    message(FATAL_ERROR "Invalid MINIFI_PACKAGING_TYPE")
endif()

if(APPLE)
    configure_file(
            minifi.plist
            ${CMAKE_BINARY_DIR}/bin/minifi.plist
            @ONLY
    )
else()
    configure_file(
            minifi.service.in
            ${CMAKE_BINARY_DIR}/bin/minifi.service
            @ONLY
    )
endif()


configure_file(
        minifi.sh
        ${CMAKE_BINARY_DIR}/bin/minifi.sh
        @ONLY
)

if (MINIFI_PACKAGING_TYPE STREQUAL "RPM")
    install(FILES ${CMAKE_BINARY_DIR}/bin/minifi.service DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/system COMPONENT bin)
elseif (MINIFI_PACKAGING_TYPE STREQUAL "TGZ")
    install(PROGRAMS ${CMAKE_BINARY_DIR}/bin/minifi.sh DESTINATION bin COMPONENT bin)
    if (APPLE)
        install(FILES ${CMAKE_BINARY_DIR}/bin/minifi.plist DESTINATION bin COMPONENT bin)
    else()
        install(FILES ${CMAKE_BINARY_DIR}/bin/minifi.service DESTINATION bin COMPONENT bin)
    endif()
else()
    message(FATAL_ERROR "Invalid MINIFI_PACKAGING_TYPE")
endif()
