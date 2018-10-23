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
cmake_minimum_required(VERSION 2.6)

find_package(Boost COMPONENTS system filesystem iostreams REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

file(GLOB SOURCES "*.cpp")

add_library(minifi-bustache-extensions STATIC ${SOURCES})
set_property(TARGET minifi-bustache-extensions PROPERTY POSITION_INDEPENDENT_CODE ON)
if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(PUBLIC minifi-bustache-extensions "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries(minifi-bustache-extensions "${CMAKE_THREAD_LIBS_INIT}")
endif()

target_link_libraries(minifi-bustache-extensions ${LIBMINIFI} ${Boost_IOSTREAMS_LIBRARY})
target_link_libraries(minifi-bustache-extensions ${CMAKE_DL_LIBS})
target_link_libraries(minifi-bustache-extensions bustache)

if (WIN32)
    set_target_properties(minifi-bustache-extensions PROPERTIES
        LINK_FLAGS "/WHOLEARCHIVE"
    )
elseif (APPLE)
    set_target_properties(minifi-bustache-extensions PROPERTIES
        LINK_FLAGS "-Wl,-all_load"
    )
else ()
    set_target_properties(minifi-bustache-extensions PROPERTIES
        LINK_FLAGS "-Wl,--whole-archive"
    )
endif ()


SET (BUSTACHE-EXTENSIONS minifi-bustache-extensions PARENT_SCOPE)

register_extension(minifi-bustache-extensions)
