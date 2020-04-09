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

function(use_bundled_libchunkio SOURCE_DIR BINARY_DIR)
    # Define patch step
    set(PC "${Patch_EXECUTABLE}" -p1 -i "${SOURCE_DIR}/thirdparty/chunkio/chunkio.patch")

    # Define byproducts
    if (WIN32)
        set(LIB_CIO "src/${CMAKE_BUILD_TYPE}/chunkio-static.lib")
	set(LIB_CRC "deps/crc32/${CMAKE_BUILD_TYPE}/cio-crc32.lib")
    else()
        set(LIB_CIO "src/libchunkio-static.a")
	set(LIB_CRC "deps/crc32/libcio-crc32.a")
    endif()

    set(CIO_SOURCE_DIR "${BINARY_DIR}/thirdparty/libchunkio-src")
    set(CIO_BINARY_DIR "${BINARY_DIR}/thirdparty/libchunkio-build")
    set(CIO_BYPRODUCT "${CIO_BINARY_DIR}/${LIB_CIO}" CACHE STRING "" FORCE)
    set(CRC_BYPRODUCT "${CIO_BINARY_DIR}/${LIB_CRC}" CACHE STRING "" FORCE)

    # Build project
    ExternalProject_Add(
      libchunkio-external
      GIT_REPOSITORY "https://github.com/edsiper/chunkio.git"
      GIT_TAG "135add5cb95d9517a491fe6822ba8e9abd1eb9dd"
      SOURCE_DIR "${CIO_SOURCE_DIR}"
      BINARY_DIR "${CIO_BINARY_DIR}"
      PATCH_COMMAND ${PC}
      INSTALL_COMMAND ${CMAKE_COMMAND} -E echo "skip install step"
      BUILD_BYPRODUCTS "${CIO_BYPRODUCT} ${CRC_BYPRODUCT}"
      EXCLUDE_FROM_ALL TRUE
    )
     
    # Set variables
    set(LIBCIO_INCLUDE_DIRS "${CIO_SOURCE_DIR}/include" "${CIO_SOURCE_DIR}/deps" "${CIO_SOURCE_DIR}/deps/monkey/include" CACHE STRING "" FORCE)
    
    # Create imported targets
    add_library(libcio STATIC IMPORTED)
    add_library(libcrc32 STATIC IMPORTED)
    add_library(libchunkio INTERFACE) 
    
    set_property(TARGET libcio PROPERTY IMPORTED_LOCATION "${CIO_BYPRODUCT}")
    set_property(TARGET libcrc32 PROPERTY IMPORTED_LOCATION "${CRC_BYPRODUCT}")
   
    target_include_directories(libchunkio INTERFACE "${LIBCIO_INCLUDE_DIRS}")
    target_link_libraries(libchunkio INTERFACE libcio INTERFACE libcrc32)

    add_dependencies(libchunkio libchunkio-external)
endfunction(use_bundled_libchunkio)
