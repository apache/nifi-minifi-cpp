# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


if (NOT (ENABLE_ALL OR ENABLE_LUA_SCRIPTING))
    return()
endif()

include(GetLua)
include(GetSol2)

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES  "*.cpp")

add_minifi_library(minifi-lua-script-extension SHARED ${SOURCES})

target_link_libraries(minifi-lua-script-extension PRIVATE ${LIBMINIFI} Threads::Threads)

SET(CMAKE_FIND_PACKAGE_SORT_ORDER NATURAL)
SET(CMAKE_FIND_PACKAGE_SORT_DIRECTION ASC)

target_include_directories(minifi-lua-script-extension PUBLIC ${lua_INCLUDE_DIR} ${sol2_INCLUDE_DIR})
target_link_libraries(minifi-lua-script-extension PUBLIC lua::lua sol2::sol2)
target_compile_features(minifi-lua-script-extension PUBLIC cxx_std_14)

register_extension(minifi-lua-script-extension "LUA SCRIPTING EXTENSIONS" LUA-SCRIPTING-EXTENSIONS "This enables LUA scripting" "extensions/lua/tests")
