# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
if(TARGET civetweb::civetweb-cpp)
    return()
endif()

include(FetchContent)
find_package(OpenSSL REQUIRED)

set(CIVETWEB_ENABLE_SSL_DYNAMIC_LOADING "OFF" CACHE STRING "" FORCE)
set(CIVETWEB_ENABLE_SERVER_EXECUTABLE OFF CACHE BOOL "" FORCE)
set(CIVETWEB_BUILD_TESTING "OFF" CACHE STRING "" FORCE)
set(CIVETWEB_ENABLE_DUKTAPE "OFF" CACHE STRING "" FORCE)
set(CIVETWEB_ENABLE_LUA "OFF" CACHE STRING "" FORCE)
set(CIVETWEB_ENABLE_CXX "ON" CACHE STRING "" FORCE)
set(CIVETWEB_ALLOW_WARNINGS "ON" CACHE STRING "" FORCE)
set(CIVETWEB_ENABLE_ASAN "OFF" CACHE STRING "" FORCE)
if(MINIFI_ADVANCED_ASAN_BUILD)
    # this defaults to 100kB, which is too little for ASan; change it to 1MB
    set(CIVETWEB_THREAD_STACK_SIZE 1048576 CACHE STRING "" FORCE)
endif()

set(PATCH_FILE_1 "${CMAKE_SOURCE_DIR}/thirdparty/civetweb/openssl3.patch")
set(PATCH_FILE_2 "${CMAKE_SOURCE_DIR}/thirdparty/civetweb/stop-flag-assign-atomic-load.patch")
set(PC ${Bash_EXECUTABLE}  -c "set -x &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_1}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_1}\\\") &&\
        (\\\"${Patch_EXECUTABLE}\\\" -p1 -R -s -f --dry-run -i \\\"${PATCH_FILE_2}\\\" || \\\"${Patch_EXECUTABLE}\\\" -p1 -N -i \\\"${PATCH_FILE_2}\\\")")

FetchContent_Declare(civetweb
    URL      https://github.com/civetweb/civetweb/archive/refs/tags/v1.16.tar.gz
    URL_HASH SHA256=f0e471c1bf4e7804a6cfb41ea9d13e7d623b2bcc7bc1e2a4dd54951a24d60285
    PATCH_COMMAND "${PC}"
    SYSTEM
)

FetchContent_MakeAvailable(civetweb)

target_link_libraries(civetweb-c-library OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(civetweb-cpp PUBLIC OpenSSL::SSL OpenSSL::Crypto)

target_compile_definitions(civetweb-c-library PRIVATE SOCKET_TIMEOUT_QUANTUM=200)
if (NOT WIN32)
    # use localtime_r/gmtime_r instead of the non-thread-safe localtime/gmtime
    target_compile_definitions(civetweb-c-library PRIVATE REENTRANT_TIME)
    # access the stop flag atomically instead of as a plain volatile int, which is a data race between mg_stop() and the civetweb threads;
    # not on Windows, because stop-flag-assign-atomic-load.patch uses the GCC/Clang builtin __atomic_load_n
    target_compile_definitions(civetweb-c-library PRIVATE STOP_FLAG_NEEDS_LOCK)
endif()

if (NOT TARGET civetweb::c-library)
    add_library(civetweb::c-library ALIAS civetweb-c-library)
endif()

if (NOT TARGET civetweb::civetweb-cpp)
    add_library(civetweb::civetweb-cpp ALIAS civetweb-cpp)
endif()

set(CIVETWEB_INCLUDE_DIR "${civetweb_SOURCE_DIR}/include")
set(CIVETWEB_INCLUDE_DIRS "${CIVETWEB_INCLUDE_DIR}")
