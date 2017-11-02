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

include_directories(${CMAKE_SOURCE_DIR}/libminifi/include
                    ${CMAKE_SOURCE_DIR}/thirdparty/spdlog-20170710/include
                    ${CIVET_THIRDPARTY_ROOT}/include
                    ${CMAKE_SOURCE_DIR}/thirdparty/jsoncpp/include
                    ${CMAKE_SOURCE_DIR}/thirdparty/
                    ./include/)

file(GLOB SOURCES  "*.cpp")

add_library(minifi-archive-extensions STATIC ${SOURCES})
set_property(TARGET minifi-archive-extensions PROPERTY POSITION_INDEPENDENT_CODE ON)
if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(PUBLIC minifi-archive-extensions "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries(minifi-archive-extensions "${CMAKE_THREAD_LIBS_INIT}")
endif()


# Include UUID
find_package(UUID REQUIRED)
target_link_libraries(minifi-archive-extensions ${LIBMINIFI} ${UUID_LIBRARIES} ${JSONCPP_LIB})
add_dependencies(minifi-archive-extensions jsoncpp_project)

#find_package(OpenSSL REQUIRED)
#include_directories(${OPENSSL_INCLUDE_DIR})

target_link_libraries(minifi-archive-extensions ${CMAKE_DL_LIBS})

find_package(LibArchive)
include_directories(${CMAKE_SOURCE_DIR}/thirdparty/libarchive-3.3.2/libarchive)
if (LibArchive_FOUND AND NOT BUILD_LIBARCHIVE)
	target_link_libraries(minifi-archive-extensions ${LibArchive_LIBRARIES})
else()
	target_link_libraries(minifi-archive-extensions archive_static)
endif()

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})
target_link_libraries (minifi-archive-extensions ${ZLIB_LIBRARIES})

if (WIN32)
    set_target_properties(minifi-archive-extensions PROPERTIES
        LINK_FLAGS "/WHOLEARCHIVE"
    )
elseif (APPLE)
    set_target_properties(minifi-archive-extensions PROPERTIES
        LINK_FLAGS "-Wl,-all_load"
    )
else ()
    set_target_properties(minifi-archive-extensions PROPERTIES
        LINK_FLAGS "-Wl,--whole-archive"
    )
endif ()


SET (ARCHIVE-EXTENSIONS minifi-archive-extensions PARENT_SCOPE)

register_extension(minifi-archive-extensions)

add_custom_target(archive-extension-linter
    COMMAND ${CMAKE_SOURCE_DIR}/thirdparty/google-styleguide/run_linter.sh
            ${CMAKE_SOURCE_DIR}/libminifi/include/
            ${CMAKE_CURRENT_LIST_DIR}/include --
            ${CMAKE_CURRENT_LIST_DIR}/)

register_extension_linter(archive-extension-linter)