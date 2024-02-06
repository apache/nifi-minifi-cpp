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

if (NOT (ENABLE_ALL OR ENABLE_LUA_SCRIPTING))
    return()
endif()

include(Sol2)

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES  "*.cpp")

add_minifi_library(minifi-lua-script-extension SHARED "${SOURCES}")

target_link_libraries(minifi-lua-script-extension PRIVATE ${LIBMINIFI} Threads::Threads)

SET(CMAKE_FIND_PACKAGE_SORT_ORDER NATURAL)
SET(CMAKE_FIND_PACKAGE_SORT_DIRECTION ASC)
find_package(Lua 5.1 REQUIRED)

target_include_directories(minifi-lua-script-extension PRIVATE lua)
target_include_directories(minifi-lua-script-extension PUBLIC ${LUA_INCLUDE_DIR})

target_link_libraries(minifi-lua-script-extension PUBLIC ${LUA_LIBRARIES} sol2)
target_compile_features(minifi-lua-script-extension PUBLIC cxx_std_14)

register_extension(minifi-lua-script-extension "LUA SCRIPTING EXTENSIONS" LUA-SCRIPTING-EXTENSIONS "This enables LUA scripting" "extensions/lua/tests")
register_extension_linter(minifi-lua-script-extension-linter)
