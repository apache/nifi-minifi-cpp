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
if(NOT ZLIB_FOUND)
  set(ZLIB_FOUND "YES" CACHE STRING "" FORCE)
  set(ZLIB_INCLUDE_DIRS "${EXPORTED_ZLIB_INCLUDE_DIRS}" CACHE STRING "" FORCE)
  set(ZLIB_LIBRARIES "${EXPORTED_ZLIB_LIBRARIES}" CACHE STRING "" FORCE)
  set(ZLIB_VERSION_STRING "${EXPORTED_ZLIB_VERSION_STRING}" CACHE STRING "" FORCE)
  set(ZLIB_VERSION_MAJOR "${EXPORTED_ZLIB_VERSION_MAJOR}" CACHE STRING "" FORCE)
  set(ZLIB_VERSION_MINOR "${EXPORTED_ZLIB_VERSION_MINOR}" CACHE STRING "" FORCE)
  set(ZLIB_VERSION_PATCH "${EXPORTED_ZLIB_VERSION_PATCH}" CACHE STRING "" FORCE)
endif()

if(NOT TARGET ZLIB::ZLIB)
  add_library(ZLIB::ZLIB STATIC IMPORTED)
  set_target_properties(ZLIB::ZLIB PROPERTIES
          INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIRS}")
  set_target_properties(ZLIB::ZLIB PROPERTIES
          IMPORTED_LINK_INTERFACE_LANGUAGES "C"
          IMPORTED_LOCATION "${ZLIB_LIBRARIES}")
endif()