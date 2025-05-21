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

include(FetchContent)

FetchContent_Declare(lua
    URL         "https://github.com/lua/lua/archive/refs/tags/v5.4.6.tar.gz"
    URL_HASH    "SHA256=11c228cf9b9564d880b394f8069ad829d01e39756567f79c347a6b89fed44771"
    SYSTEM
)

FetchContent_GetProperties(lua)
if(NOT lua_POPULATED)
    FetchContent_Populate(lua)
    # lua.org tarball:
    #set(lua_tarball_src_path "src/")
    # github.com tarball:
    set(lua_tarball_src_path "")

    file(GLOB LUA_SOURCES "${lua_SOURCE_DIR}/${lua_tarball_src_path}*.c")
    # the github tarball contains onelua.c, which is a concatenated version of all source files, we don't need it
    list(REMOVE_ITEM LUA_SOURCES "${lua_SOURCE_DIR}/${lua_tarball_src_path}onelua.c")
    add_library(lua STATIC ${LUA_SOURCES})

    file(MAKE_DIRECTORY "${lua_BINARY_DIR}/include")
    foreach(HEADER lua.h luaconf.h lualib.h lauxlib.h)
        file(COPY "${lua_SOURCE_DIR}/${lua_tarball_src_path}${HEADER}" DESTINATION "${lua_BINARY_DIR}/include")
    endforeach()
    set(LUA_INCLUDE_DIR "${lua_BINARY_DIR}/include" CACHE STRING "" FORCE)
endif()
