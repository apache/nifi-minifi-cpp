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
# under the License.
if(USE_CONAN_PACKAGER)
    message("Using Conan Packager to manage installing prebuilt Asio external lib")
    include(${CMAKE_BINARY_DIR}/asio-config.cmake)
elseif(USE_CMAKE_FETCH_CONTENT)
    include(FetchContent)
    message("Using CMAKE's FetchContent to manage source building Asio external lib")
    FetchContent_Declare(asio
            URL https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-28-1.tar.gz
            URL_HASH SHA256=5ff6111ec8cbe73a168d997c547f562713aa7bd004c5c02326f0e9d579a5f2ce)

    FetchContent_GetProperties(asio)
    if(NOT asio_POPULATED)
        FetchContent_Populate(asio)
        add_library(asio INTERFACE)
        add_library(asio::asio ALIAS asio)
        target_include_directories(asio SYSTEM INTERFACE ${asio_SOURCE_DIR}/asio/include)
        find_package(Threads)
        target_link_libraries(asio INTERFACE Threads::Threads)
    endif()
endif()
