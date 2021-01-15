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
#  Variables defined here
#  NLOHMANN_JSON_FOUND              System has PCAP libs/headers
#  NLOHMANN_JSON_LIBRARIES          The PCAP libraries
#  NLOHMANN_JSON_INCLUDE_DIR        The location of PCAP headers

include(FindPackageHandleStandardArgs)

# Look for the header file.
find_path(NLOHMANN_JSON_INCLUDE_DIR
    NAMES
        nlohmann/json.hpp
    HINTS
        "${NLOHMANN_JSON_DIR}/include")

find_package_handle_standard_args(NLOHMANN_JSON
    FOUND_VAR NLOHMANN_JSON_FOUND
    REQUIRED_VARS
        NLOHMANN_JSON_INCLUDE_DIR
)

mark_as_advanced(
    NLOHMANN_JSON_INCLUDE_DIR
)

if (NLOHMANN_JSON_FOUND)
    set(NLOHMANN_JSON_INCLUDE_DIRS ${NLOHMANN_JSON_INCLUDE_DIR})
    message("${NLOHMANN_JSON_INCLUDE_DIRS}")

    if (NOT TARGET nlohmann_json)
        add_library(nlohmann_json INTERFACE)
        add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
        set_target_properties(nlohmann_json PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${NLOHMANN_JSON_INCLUDE_DIRS}")

        target_sources(nlohmann_json INTERFACE ${NLOHMANN_JSON_INCLUDE_DIRS}/nlohmann/json.hpp)
        target_include_directories(nlohmann_json INTERFACE ${NLOHMANN_JSON_INCLUDE_DIRS})
        target_compile_features(nlohmann_json INTERFACE cxx_std_11)
        set(nlohmann_json_FOUND "YES" CACHE STRING "" FORCE)
        install(TARGETS nlohmann_json EXPORT nlohmann_json DESTINATION include)

        if(WIN32 AND NOT CYGWIN)
            set(INSTALL_CMAKE_DIR ${CMAKE_INSTALL_PREFIX}/CMake)
        else()
        include(GNUInstallDirs)
            set(INSTALL_CMAKE_DIR ${LIB_INSTALL_DIR}/cmake/nlohmann_json)
        endif()
        install(FILES ${NLOHMANN_JSON_INCLUDE_DIRS}/nlohmann/json.hpp DESTINATION include)
        install(EXPORT nlohmann_json DESTINATION ${INSTALL_CMAKE_DIR})
  endif ()
endif ()
