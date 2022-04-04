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

if(NOT $CACHE{CURL_FOUND})
    set(CURL_FOUND "YES" CACHE STRING "" FORCE)
    set(CURL_INCLUDE_DIR "${EXPORTED_CURL_INCLUDE_DIR}" CACHE STRING "" FORCE)
    set(CURL_INCLUDE_DIRS "${CURL_INCLUDE_DIR}" CACHE STRING "" FORCE)
    set(CURL_LIBRARY "${EXPORTED_CURL_LIBRARY}" CACHE STRING "" FORCE)
    set(CURL_LIBRARIES "${CURL_LIBRARY}" CACHE STRING "" FORCE)
endif()

if(NOT TARGET CURL::libcurl)
    add_library(CURL::libcurl STATIC IMPORTED)
    set_target_properties(CURL::libcurl PROPERTIES IMPORTED_LOCATION "${CURL_LIBRARIES}")
    set_property(TARGET CURL::libcurl APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIRS}")
endif()
