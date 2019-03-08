#
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
#


include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)
include_directories(protocols nanofi controllerservice server)
include_directories(../http-curl/)

file(GLOB CSOURCES "nanofi/*.c")
file(GLOB SOURCES "*.cpp" "protocols/*.cpp" "processors/*.cpp" "controllerservice/*.cpp" "server/*.cpp" )

add_library(nanofi-coap-c STATIC ${CSOURCES})
add_library(minifi-coap STATIC ${SOURCES})
set_property(TARGET minifi-coap PROPERTY POSITION_INDEPENDENT_CODE ON)

if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries(minifi-coap "${CMAKE_THREAD_LIBS_INIT}")
endif()

  set(BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}/extensions/coap")
  # determine version of GNUTLSs
  set(BYPRODUCT "${BASE_DIR}/extensions/coap/thirdparty/libcoap-src/.libs/libcoap-2.a")
  set(DIR "${BASE_DIR}/extensions/coap/thirdparty/libcoap-src")
  ExternalProject_Add(
    coap-external
    GIT_REPOSITORY "https://github.com/obgm/libcoap.git"
    GIT_TAG "00486a4f46e0278dd24a8ff3411416ff420cde29" 
    PREFIX "${BASE_DIR}/extensions/coap/thirdparty/libcoap"
    BUILD_IN_SOURCE true
    SOURCE_DIR "${DIR}"
    BUILD_COMMAND make
    CMAKE_COMMAND ""
    UPDATE_COMMAND ""
    INSTALL_COMMAND ${CMAKE_COMMAND}  -E echo "Skipping install step."
    CONFIGURE_COMMAND ""
    PATCH_COMMAND ./autogen.sh && ./configure --disable-examples --disable-dtls --disable-tests --disable-documentation
    STEP_TARGETS build
    EXCLUDE_FROM_ALL TRUE
  )
  add_definitions("-DWITH_POSIX=1")
  
  add_library(coaplib STATIC IMPORTED)
  set_target_properties(coaplib PROPERTIES IMPORTED_LOCATION "${BYPRODUCT}")
  add_dependencies(coaplib coap-external)
  set(COAP_FOUND "YES" CACHE STRING "" FORCE)
  set(COAP_INCLUDE_DIRS "${DIR}/include" CACHE STRING "" FORCE)
  set(COAP_LIBRARIES coaplib CACHE STRING "" FORCE)
  set(COAP_LIBRARY coaplib CACHE STRING "" FORCE)
  set(COAP_LIBRARY coaplib CACHE STRING "" FORCE)
target_link_libraries(minifi-coap ${CMAKE_DL_LIBS})

include_directories(${COAP_INCLUDE_DIRS})

target_link_libraries (nanofi-coap-c ${COAP_LIBRARIES})
target_link_libraries (nanofi-coap-c ${HTTP-CURL})
target_link_libraries (minifi-coap nanofi-coap-c ${COAP_LIBRARIES})
if (WIN32)
    set_target_properties(minifi-coap PROPERTIES
        LINK_FLAGS "/WHOLEARCHIVE"
    )
elseif (APPLE)
    set_target_properties(minifi-coap PROPERTIES
        LINK_FLAGS "-Wl,-all_load"
    )
else ()
    set_target_properties(minifi-coap PROPERTIES
        LINK_FLAGS "-Wl,--whole-archive"
    )
endif ()

SET (COAP-EXTENSION minifi-coap PARENT_SCOPE)
register_extension(minifi-coap)
