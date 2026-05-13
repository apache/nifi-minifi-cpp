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

FetchContent_Declare(asio
        URL https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-38-0.tar.gz
        URL_HASH SHA256=5cf78ede456fd13b95fe692711f4f689be26c826f1f5541c7a1df3aa32bd9dbd
        SYSTEM)

FetchContent_GetProperties(asio)
if(NOT asio_POPULATED)
    FetchContent_Populate(asio)
    add_library(asio INTERFACE)
    target_include_directories(asio SYSTEM INTERFACE ${asio_SOURCE_DIR}/include)
    find_package(Threads)
    target_link_libraries(asio INTERFACE Threads::Threads OpenSSL::SSL OpenSSL::Crypto)
endif()
