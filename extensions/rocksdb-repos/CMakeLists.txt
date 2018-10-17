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

find_package(RocksDB)

if (NOT ROCKSDB_FOUND OR BUILD_ROCKSDB)
	include_directories(${ROCKSDB_THIRDPARTY_ROOT}/include)
else()
	include_directories(${ROCKSDB_INCLUDE_DIR})
endif()


file(GLOB SOURCES  "*.cpp")

add_library(minifi-rocksdb-repos STATIC ${SOURCES})
set_property(TARGET minifi-rocksdb-repos PROPERTY POSITION_INDEPENDENT_CODE ON)
if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(PUBLIC minifi-rocksdb-repos "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries(minifi-rocksdb-repos "${CMAKE_THREAD_LIBS_INIT}")
endif()


# Include UUID
find_package(UUID REQUIRED)
target_link_libraries(minifi-rocksdb-repos ${LIBMINIFI} ${UUID_LIBRARIES})
target_link_libraries(minifi-rocksdb-repos ${CMAKE_DL_LIBS} )
if (ROCKSDB_FOUND AND NOT BUILD_ROCKSDB)
	target_link_libraries(minifi-rocksdb-repos ${ROCKSDB_LIBRARIES} )
else()
	target_link_libraries(minifi-rocksdb-repos rocksdb )
endif()
find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})
target_link_libraries (minifi-rocksdb-repos ${ZLIB_LIBRARIES})
if (WIN32)
    set_target_properties(minifi-rocksdb-repos PROPERTIES
        LINK_FLAGS "/WHOLEARCHIVE"
    )
elseif (APPLE)
    set_target_properties(minifi-rocksdb-repos PROPERTIES
        LINK_FLAGS "-Wl,-all_load"
    )
else ()
    set_target_properties(minifi-rocksdb-repos PROPERTIES
        LINK_FLAGS "-Wl,--whole-archive"
    )
endif ()


SET (ROCKSDB-REPOS minifi-rocksdb-repos PARENT_SCOPE)

register_extension(minifi-rocksdb-repos)
