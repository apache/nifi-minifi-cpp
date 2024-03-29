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

if (WIN32 OR NOT (ENABLE_ALL OR ENABLE_GPS))
    return()
endif()

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

find_package(LibGPS REQUIRED)

include_directories(${LIBGPS_INCLUDE_DIRS})

file(GLOB SOURCES  "*.cpp")

add_minifi_library(minifi-gps SHARED ${SOURCES})

# Include UUID
target_link_libraries(minifi-gps ${LIBMINIFI} )
target_link_libraries(minifi-gps ${LIBGPS_LIBRARIES})

register_extension(minifi-gps "GPS EXTENSIONS" GPS-EXTENSION "Enables LibGPS Functionality and the GetGPS processor." "extensions/gps/tests")
register_extension_linter(minifi-gps-linter)
