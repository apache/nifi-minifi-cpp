# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

include(FetchContent)

FetchContent_Declare(lua
    URL         "https://github.com/lua/lua/archive/refs/tags/v5.4.8.tar.gz"
    URL_HASH    "SHA256=d85b70a65f43c5d2254944d58d625e822c8e2e10d9c6a3bd9b5b657e46376a19"
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
    add_library(lua::lua ALIAS lua)

    file(MAKE_DIRECTORY "${lua_BINARY_DIR}/include")
    foreach(HEADER lua.h luaconf.h lualib.h lauxlib.h)
        file(COPY "${lua_SOURCE_DIR}/${lua_tarball_src_path}${HEADER}" DESTINATION "${lua_BINARY_DIR}/include")
    endforeach()
    set(lua_INCLUDE_DIR "${lua_BINARY_DIR}/include" CACHE STRING "" FORCE)
endif()
