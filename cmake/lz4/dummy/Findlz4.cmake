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

# Dummy lz4 find for when we use bundled version
if(NOT LZ4_FOUND)
    set(LZ4_FOUND "YES" CACHE STRING "" FORCE)
    set(LZ4_INCLUDE_DIR "${EXPORTED_LZ4_INCLUDE_DIRS}" CACHE STRING "" FORCE)
    set(LZ4_INCLUDE_DIRS "${EXPORTED_LZ4_INCLUDE_DIRS}" CACHE STRING "" FORCE)
    set(LZ4_LIBRARIES "${EXPORTED_LZ4_LIBRARIES}" CACHE STRING "" FORCE)
endif()

if(NOT TARGET lz4::lz4)
    add_library(lz4::lz4 STATIC IMPORTED)
    set_target_properties(lz4::lz4 PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${LZ4_INCLUDE_DIRS}")
    set_target_properties(lz4::lz4 PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${LZ4_LIBRARIES}")
endif()
