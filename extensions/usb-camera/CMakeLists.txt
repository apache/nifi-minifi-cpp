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

set(CMAKE_EXE_LINKER_FLAGS "-Wl,--export-all-symbols")
set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--export-symbols")

find_package(PkgConfig)
pkg_check_modules(LIBUSB libusb-1.0)

find_package(png QUIET)
if(PNG_FOUND)
    set(PNG_LINK_FLAGS ${PNG_LIBRARIES})
else()
    pkg_check_modules(PNG QUIET libpng)
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

include_directories(${CMAKE_SOURCE_DIR}/libminifi/include
                    ${CMAKE_SOURCE_DIR}/thirdparty/spdlog-20170710/include
                    ${CMAKE_SOURCE_DIR}/thirdparty/libuvc-0.0.6/include
                    ./include/)

file(GLOB SOURCES  "*.cpp")

add_library(minifi-usb-camera-extensions STATIC ${SOURCES})
set_property(TARGET minifi-usb-camera-extensions PROPERTY POSITION_INDEPENDENT_CODE ON)
if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(PUBLIC minifi-usb-camera-extensions "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries(minifi-usb-camera-extensions "${CMAKE_THREAD_LIBS_INIT}")
endif()

find_package(UUID REQUIRED)
target_link_libraries(minifi-usb-camera-extensions ${LIBMINIFI} ${UUID_LIBRARIES})

target_link_libraries(minifi-usb-camera-extensions ${CMAKE_DL_LIBS})
target_link_libraries(minifi-usb-camera-extensions ${PNG_LINK_FLAGS})
target_link_libraries(minifi-usb-camera-extensions uvc_static)
target_link_libraries(minifi-usb-camera-extensions ${LIBUSB_LIBRARIES})

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})
target_link_libraries (minifi-usb-camera-extensions ${ZLIB_LIBRARIES})

if (WIN32)
    set_target_properties(minifi-usb-camera-extensions PROPERTIES
        LINK_FLAGS "/WHOLEARCHIVE"
    )
elseif (APPLE)
    set_target_properties(minifi-usb-camera-extensions PROPERTIES
        LINK_FLAGS "-Wl,-all_load"
    )
else ()
    set_target_properties(minifi-usb-camera-extensions PROPERTIES
        LINK_FLAGS "-Wl,--whole-archive"
    )
endif ()


SET (USB-CAMERA-EXTENSIONS minifi-usb-camera-extensions PARENT_SCOPE)

register_extension(minifi-usb-camera-extensions)

add_custom_target(usb-camera-extension-linter
    COMMAND ${CMAKE_SOURCE_DIR}/thirdparty/google-styleguide/run_linter.sh
            ${CMAKE_SOURCE_DIR}/libminifi/include/
            ${CMAKE_CURRENT_LIST_DIR}/include --
            ${CMAKE_CURRENT_LIST_DIR}/)

register_extension_linter(usb-camera-extension-linter)