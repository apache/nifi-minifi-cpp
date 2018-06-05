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

include_directories(../../thirdparty/RTIMULib/RTIMULib)
file(GLOB SOURCES  "*.cpp")

add_library(minifi-sensors STATIC ${SOURCES})

set_property(TARGET minifi-sensors PROPERTY POSITION_INDEPENDENT_CODE ON)
if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(PUBLIC minifi-sensors "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries(minifi-sensors "${CMAKE_THREAD_LIBS_INIT}")
endif()

target_link_libraries(minifi-sensors ${LIBMINIFI} )

if (WIN32)
    set_target_properties(minifi-sensors PROPERTIES
        LINK_FLAGS "/WHOLEARCHIVE"
    )
elseif (APPLE)
    set_target_properties(minifi-sensors PROPERTIES
        LINK_FLAGS "-Wl,-all_load"
    )
else ()
    set_target_properties(minifi-sensors PROPERTIES
        LINK_FLAGS "-Wl,--whole-archive"
    )
endif ()

add_dependencies(minifi-sensors RTIMULib)
target_link_libraries(minifi-sensors RTIMULib)

SET (SENSOR-EXTENSIONS minifi-sensors PARENT_SCOPE)

register_extension(minifi-sensors)
