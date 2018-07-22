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

include_directories(${CMAKE_SOURCE_DIR}/libminifi/include
                    ${CMAKE_SOURCE_DIR}/libminifi/include/core
                    ${CMAKE_SOURCE_DIR}/thirdparty/spdlog-20170710/include
                    ${CMAKE_SOURCE_DIR}/thirdparty/concurrentqueue
                    ${CMAKE_SOURCE_DIR}/thirdparty/yaml-cpp-yaml-cpp-0.5.3/include
                    ${CIVET_THIRDPARTY_ROOT}/include
                    ${CMAKE_SOURCE_DIR}/thirdparty/jsoncpp/include
                    ${CMAKE_SOURCE_DIR}/thirdparty/
                    ./include)

set(BUILD_CIVET_TESTING OFF)
add_subdirectory(${CIVET_THIRDPARTY_ROOT}
                 ${CIVET_BINARY_ROOT}
                 EXCLUDE_FROM_ALL)

file(GLOB SOURCES  "processors/*.cpp")

add_library(minifi-civet-extensions STATIC ${SOURCES})
set_property(TARGET minifi-civet-extensions PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(minifi-civet-extensions c-library civetweb-cpp)

if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(PUBLIC minifi-civet-extensions "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries(minifi-civet-extensions "${CMAKE_THREAD_LIBS_INIT}")
endif()


if (WIN32)
    set_target_properties(minifi-civet-extensions PROPERTIES
        LINK_FLAGS "/WHOLEARCHIVE"
    )
elseif (APPLE)
    set_target_properties(minifi-civet-extensions PROPERTIES
        LINK_FLAGS "-Wl,-all_load"
    )
else ()
    set_target_properties(minifi-civet-extensions PROPERTIES
        LINK_FLAGS "-Wl,--whole-archive"
    )
endif ()


SET (civet-EXTENSIONS minifi-civet-extensions PARENT_SCOPE)

register_extension(minifi-civet-extensions)

