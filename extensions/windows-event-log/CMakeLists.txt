#
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
#

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES  "*.cpp" "wel/*.cpp")

add_library(minifi-wel STATIC ${SOURCES})
set_property(TARGET minifi-wel PROPERTY POSITION_INDEPENDENT_CODE ON)
if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(PUBLIC minifi-wel "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries(minifi-wel "${CMAKE_THREAD_LIBS_INIT}")
endif()

set(PUGI_BYPRODUCT_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/pugixml-install" CACHE STRING "PugiXML install directory")
set(BYPRODUCT "${PUGI_BYPRODUCT_DIR}/lib/pugixml.lib")
  ExternalProject_Add(
    pugixml-external
    GIT_REPOSITORY "https://github.com/zeux/pugixml.git"
    GIT_TAG "v1.9" 
    SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/pugixml-src"
    CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
			   "-DBUILD_TESTS=OFF"
			   "-DBUILD_SHARED_AND_STATIC_LIBS=OFF"
			   "-DBUILD_SHARED_LIBS=OFF"
               "-DCMAKE_INSTALL_PREFIX=${PUGI_BYPRODUCT_DIR}"
    BUILD_BYPRODUCTS ${BYPRODUCT}
  )

include_directories("${PUGI_BYPRODUCT_DIR}/include")
add_dependencies(minifi-wel pugixml-external)
target_link_libraries(minifi-wel ${LIBMINIFI} ${BYPRODUCT})

target_link_libraries(minifi-wel ${CMAKE_DL_LIBS} )
find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})
target_link_libraries (minifi-wel ${ZLIB_LIBRARIES})
if (WIN32)
    set_target_properties(minifi-wel PROPERTIES
        LINK_FLAGS "/WHOLEARCHIVE"
    )
elseif (APPLE)
    set_target_properties(minifi-wel PROPERTIES
        LINK_FLAGS "-Wl,-all_load"
    )
else ()
    set_target_properties(minifi-wel PROPERTIES
        LINK_FLAGS "-Wl,--whole-archive"
    )
endif ()


SET (WEL-EXTENSION minifi-wel PARENT_SCOPE)

register_extension(minifi-wel)
