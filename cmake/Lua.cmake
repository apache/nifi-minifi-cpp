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
    URL         "https://www.lua.org/ftp/lua-5.4.6.tar.gz"
    URL_HASH    "SHA256=7d5ea1b9cb6aa0b59ca3dde1c6adcb57ef83a1ba8e5432c0ecd06bf439b3ad88"
)

FetchContent_GetProperties(lua)
if(NOT lua_POPULATED)
    FetchContent_Populate(lua)

    file(GLOB LUA_SOURCES "${lua_SOURCE_DIR}/src/*.c")
    add_library(lua STATIC ${LUA_SOURCES})

    file(MAKE_DIRECTORY "${lua_BINARY_DIR}/include")
    foreach(HEADER lua.h luaconf.h lualib.h lauxlib.h lua.hpp)
        file(COPY "${lua_SOURCE_DIR}/src/${HEADER}" DESTINATION "${lua_BINARY_DIR}/include")
    endforeach()
    set(LUA_INCLUDE_DIR "${lua_BINARY_DIR}/include" CACHE STRING "" FORCE)
endif()
