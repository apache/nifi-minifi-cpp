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

include_directories(../../thirdparty/librdkafka-0.11.1/src ./../thirdparty/librdkafka-0.11.1/src-cpp)

file(GLOB SOURCES  "*.cpp")

add_library(minifi-rdkafka-extensions STATIC ${SOURCES})
set_property(TARGET minifi-rdkafka-extensions PROPERTY POSITION_INDEPENDENT_CODE ON)
if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(PUBLIC minifi-rdkafka-extensions "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries(minifi-rdkafka-extensions "${CMAKE_THREAD_LIBS_INIT}")
endif()


# Include UUID
find_package(UUID REQUIRED)
target_link_libraries(minifi-rdkafka-extensions ${LIBMINIFI} ${UUID_LIBRARIES})
target_link_libraries(minifi-rdkafka-extensions ${CMAKE_DL_LIBS} )
if (LibRdKafka_FOUND AND NOT BUILD_LIBRDKAFKA)
	target_link_libraries(minifi-rdkafka-extensions ${LibRdKafka_LIBRARIES} )
else()
	target_link_libraries(minifi-rdkafka-extensions rdkafka )
endif()
include_directories(${ZLIB_INCLUDE_DIRS})
target_link_libraries (minifi-rdkafka-extensions ${ZLIB_LIBRARIES})
if (WIN32)
    set_target_properties(minifi-rdkafka-extensions PROPERTIES
        LINK_FLAGS "/WHOLERDKAFKA"
    )
elseif (APPLE)
    set_target_properties(minifi-rdkafka-extensions PROPERTIES
        LINK_FLAGS "-Wl,-all_load"
    )
else ()
    set_target_properties(minifi-rdkafka-extensions PROPERTIES
        LINK_FLAGS "-Wl,--whole-rdkafka"
    )
endif ()


SET (RDKAFKA-EXTENSIONS minifi-rdkafka-extensions PARENT_SCOPE)

register_extension(minifi-rdkafka-extensions)

