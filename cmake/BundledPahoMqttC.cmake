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

function(use_bundled_pahomqttc SOURCE_DIR BINARY_DIR)
    # Define patch step
    set(PC "${Patch_EXECUTABLE}" -p1 -i "${SOURCE_DIR}/thirdparty/paho.mqtt.c/paho.mqtt.c.patch")

    # Define byproducts
    get_property(LIB64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)
    if ("${LIB64}" STREQUAL "TRUE" AND (NOT WIN32 AND NOT APPLE))
        set(LIBSUFFIX 64)
    endif()

    if (WIN32)
        set(BYPRODUCT "lib/libpaho-mqtt3cs-static.lib")
    else()
        set(BYPRODUCT "lib${LIBSUFFIX}/libpaho-mqtt3cs-static.a")
    endif()

    # Set build options
    set(PAHOMQTTC_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/paho.mqtt.c-install"
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            -DPAHO_BUILD_STATIC=TRUE
            -DPAHO_ENABLE_TESTING=FALSE)
    if (OPENSSL_OFF)
        list(APPEND PAHOMQTTC_CMAKE_ARGS -DPAHO_WITH_SSL=FALSE)
    else()
        list(APPEND PAHOMQTTC_CMAKE_ARGS -DPAHO_WITH_SSL=TRUE)
    endif()

    append_third_party_passthrough_args(PAHOMQTTC_CMAKE_ARGS "${PAHOMQTTC_CMAKE_ARGS}")

    # Build project
    ExternalProject_Add(
            paho.mqtt.c-external
            GIT_REPOSITORY "https://github.com/eclipse/paho.mqtt.c.git"
            GIT_TAG "6aa07f575bc9369402b1b252fd280373f8d585ef" # This is not a tagged/released version, but the exact one that was in our repo.
            SOURCE_DIR "${BINARY_DIR}/thirdparty/paho.mqtt.c-src"
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            CMAKE_ARGS ${PAHOMQTTC_CMAKE_ARGS}
            PATCH_COMMAND ${PC}
            BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/paho.mqtt.c-install/${BYPRODUCT}"
            EXCLUDE_FROM_ALL TRUE
    )

    # Set dependencies
    if (NOT OPENSSL_OFF)
        add_dependencies(paho.mqtt.c-external OpenSSL::SSL OpenSSL::Crypto)
    endif()

    # Set variables
    set(PAHOMQTTC_FOUND "YES" CACHE STRING "" FORCE)
    set(PAHOMQTTC_INCLUDE_DIR "${BINARY_DIR}/thirdparty/paho.mqtt.c-install/include" CACHE STRING "" FORCE)
    set(PAHOMQTTC_LIBRARY "${BINARY_DIR}/thirdparty/paho.mqtt.c-install/${BYPRODUCT}" CACHE STRING "" FORCE)
    set(PAHOMQTTC_LIBRARIES ${PAHOMQTTC_LIBRARY} CACHE STRING "" FORCE)

    # Create imported targets
    add_library(paho.mqtt.c STATIC IMPORTED)
    set_target_properties(paho.mqtt.c PROPERTIES IMPORTED_LOCATION "${PAHOMQTTC_LIBRARY}")
    add_dependencies(paho.mqtt.c paho.mqtt.c-external)
    file(MAKE_DIRECTORY ${PAHOMQTTC_INCLUDE_DIR})
    set_property(TARGET paho.mqtt.c APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${PAHOMQTTC_INCLUDE_DIR})
    if (NOT OPENSSL_OFF)
        set_property(TARGET paho.mqtt.c APPEND PROPERTY INTERFACE_LINK_LIBRARIES OpenSSL::SSL OpenSSL::Crypto)
    endif()
endfunction(use_bundled_pahomqttc)
