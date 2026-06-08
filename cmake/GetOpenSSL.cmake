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

if(MINIFI_OPENSSL_SOURCE STREQUAL "CONAN")
    message("Using Conan to install OpenSSL")
    find_package(OpenSSL REQUIRED)
    set(FIND_OPENSSL_PATH "${CMAKE_BINARY_DIR}/FindOpenSSL.cmake" CACHE INTERNAL "Location of the FindOpenSSL file, for other dependencies")
    set(FIND_CRYPTO_PATH "${CMAKE_BINARY_DIR}/FindOpenSSL.cmake" CACHE INTERNAL "Conan's FindOpenSSL finds the Crypto library, too")
elseif(MINIFI_OPENSSL_SOURCE STREQUAL "BUILD")
    message("Using CMake to build OpenSSL from source")
    include(BundledOpenSSL)
    use_openssl(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/ssl")
endif()
