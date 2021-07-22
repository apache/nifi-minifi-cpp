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

find_package(PNG)
if(PNG_FOUND)
    set(PNG_LINK_FLAGS ${PNG_LIBRARIES})
else()
    pkg_check_modules(PNG libpng)
    if(PNG_FOUND)
        set(PNG_INCLUDE_DIR ${PNG_INCLUDE_DIRS})
        set(PNG_LINK_FLAGS ${PNG_LDFLAGS})
    else()
        find_path(PNG_INCLUDE_DIR png.h)
        if(PNG_INCLUDE_DIR)
            set(PNG_FOUND ON)
            set(PNG_LINK_FLAGS -lpng)
        endif()
    endif()
endif()

if (NOT PNG_FOUND)
    message(FATAL_ERROR "A compatible PNG library is required to build GetUSBCamera.")
endif()

if(NOT TARGET PNG::PNG)
    add_library(PNG::PNG UNKNOWN IMPORTED)
    set_target_properties(PNG::PNG PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${PNG_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES ${PNG_LINK_FLAGS})
endif()

file(GLOB SOURCES  "*.cpp")

add_library(minifi-usb-camera-extensions SHARED ${SOURCES})

target_link_libraries(minifi-usb-camera-extensions ${LIBMINIFI} Threads::Threads)
target_link_libraries(minifi-usb-camera-extensions libuvc PNG::PNG)

SET (USB-CAMERA-EXTENSIONS minifi-usb-camera-extensions PARENT_SCOPE)

register_extension(minifi-usb-camera-extensions)
register_extension_linter(minifi-usb-camera-extensions-linter)
