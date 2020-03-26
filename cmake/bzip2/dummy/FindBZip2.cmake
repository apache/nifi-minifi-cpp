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

# Dummy zlib find for when we use bundled version
if(NOT BZIP2_FOUND)
    set(BZIP2_FOUND "YES" CACHE STRING "" FORCE)
    set(BZIP2_INCLUDE_DIR "${EXPORTED_BZIP2_INCLUDE_DIRS}" CACHE STRING "" FORCE)
    set(BZIP2_INCLUDE_DIRS "${EXPORTED_BZIP2_INCLUDE_DIRS}" CACHE STRING "" FORCE)
    set(BZIP2_LIBRARIES "${EXPORTED_BZIP2_LIBRARIES}" CACHE STRING "" FORCE)
endif()

if(NOT TARGET BZip2::BZip2)
    add_library(BZip2::BZip2 STATIC IMPORTED)
    set_target_properties(BZip2::BZip2 PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${BZIP2_INCLUDE_DIRS}")
    set_target_properties(BZip2::BZip2 PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${BZIP2_LIBRARIES}")
endif()
