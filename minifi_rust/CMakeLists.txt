# Licensed to the Apache Software Foundation (ASF) under one
#
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

if (NOT MINIFI_RUST)
    return()
endif()

include(FetchContent)

# Corrosion is a tool for integrating Rust into an existing CMake project
FetchContent_Declare(Corrosion
        URL https://github.com/corrosion-rs/corrosion/archive/refs/tags/v0.6.1.tar.gz
        URL_HASH SHA256=e9e95b1ee2bad52681f347993fb1a5af5cce458c5ce8a2636c9e476e4babf8e3
        SYSTEM)

FetchContent_MakeAvailable(Corrosion)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

corrosion_import_crate(MANIFEST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml)

if (NOT ENABLE_TEST_PROCESSORS)
    set_target_properties(cargo-build_minifi_rs_playground PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

if (MINIFI_EXTENSION_PGP)
    register_rust_extension(minifi_pgp "PGP EXTENSION" MINIFI_EXTENSION_PGP "This enables EncryptContentPGP and DecryptContentPGP")
else()
    set_target_properties(cargo-build_minifi_pgp PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

if (MINIFI_EXTENSION_TENSOR)
    register_rust_extension(minifi_tensor "TENSOR EXTENSION" MINIFI_EXTENSION_TENSOR "This enables sonos/tract based tensor processors")
else()
    set_target_properties(cargo-build_minifi_tensor PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

include(CTest)

# Build the test binaries during the build
add_custom_target(cargo_build_tests ALL
        COMMAND cargo test --no-run
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Building Rust test binaries")

add_test(
        NAME cargo_tests
        COMMAND cargo test
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
