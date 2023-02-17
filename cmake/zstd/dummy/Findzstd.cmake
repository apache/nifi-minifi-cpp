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

# Dummy zstd find for when we use bundled version
if(NOT zstd_FOUND)
    set(zstd_FOUND "YES" CACHE STRING "" FORCE)
    set(ZSTD_INCLUDE_DIR "${EXPORTED_ZSTD_INCLUDE_DIRS}" CACHE STRING "" FORCE)
    set(ZSTD_INCLUDE_DIRS "${EXPORTED_ZSTD_INCLUDE_DIRS}" CACHE STRING "" FORCE)
    set(ZSTD_LIBRARIES "${EXPORTED_ZSTD_LIBRARIES}" CACHE STRING "" FORCE)
endif()

if(NOT TARGET zstd::zstd)
    add_library(zstd::zstd STATIC IMPORTED)
    set_target_properties(zstd::zstd PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${ZSTD_INCLUDE_DIRS}")
    set_target_properties(zstd::zstd PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${ZSTD_LIBRARIES}")
endif()
