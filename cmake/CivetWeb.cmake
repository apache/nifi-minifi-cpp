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

set(CIVETWEB_ENABLE_SSL_DYNAMIC_LOADING "OFF" CACHE STRING "" FORCE)
set(CIVETWEB_BUILD_TESTING "OFF" CACHE STRING "" FORCE)
set(CIVETWEB_ENABLE_DUKTAPE "OFF" CACHE STRING "" FORCE)
set(CIVETWEB_ENABLE_LUA "OFF" CACHE STRING "" FORCE)
set(CIVETWEB_ENABLE_CXX "ON" CACHE STRING "" FORCE)
set(CIVETWEB_ALLOW_WARNINGS "ON" CACHE STRING "" FORCE)
set(CIVETWEB_ENABLE_ASAN "OFF" CACHE STRING "" FORCE)

FetchContent_Declare(civetweb
    GIT_REPOSITORY "https://github.com/civetweb/civetweb.git"
    GIT_TAG "4447b6501d5c568b4c6c0940eac801ec690b2250" # commit containing fix for MSVC issue https://github.com/civetweb/civetweb/issues/1024
)

FetchContent_MakeAvailable(civetweb)

add_dependencies(civetweb-c-library OpenSSL::Crypto OpenSSL::SSL)
add_dependencies(civetweb-cpp OpenSSL::Crypto OpenSSL::SSL)

target_compile_definitions(civetweb-c-library PRIVATE SOCKET_TIMEOUT_QUANTUM=200)
if (NOT WIN32)
  target_compile_options(civetweb-c-library PRIVATE -Wno-error)
  target_compile_options(civetweb-cpp PRIVATE -Wno-error)
endif()

add_library(civetweb::c-library ALIAS civetweb-c-library)
add_library(civetweb::civetweb-cpp ALIAS civetweb-cpp)
