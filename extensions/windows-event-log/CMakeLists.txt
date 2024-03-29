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

if (NOT (WIN32 AND (ENABLE_ALL OR ENABLE_WEL)))
    return()
endif()

include(BundledPugiXml)
use_bundled_pugixml(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES  "*.cpp" "wel/*.cpp")

add_minifi_library(minifi-wel SHARED ${SOURCES})

target_link_libraries(minifi-wel ${LIBMINIFI} Threads::Threads)
target_link_libraries(minifi-wel PUGI::libpugixml ZLIB::ZLIB Wevtapi.lib)

register_extension(minifi-wel "WEL EXTENSIONS" WEL-EXTENSION "Enables the suite of Windows Event Log extensions." "extensions/windows-event-log/tests")
register_extension_linter(minifi-wel-extension-linter)
