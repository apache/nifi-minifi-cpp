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

if(NOT LIBXML2_FOUND)
    set(LIBXML2_FOUND "YES" CACHE STRING "" FORCE)
    set(LIBXML2_INCLUDE_DIR "${EXPORTED_LIBXML2_INCLUDE_DIR}" CACHE STRING "" FORCE)
    set(LIBXML2_INCLUDE_DIRS "${EXPORTED_LIBXML2_INCLUDE_DIR}" CACHE STRING "" FORCE)
    set(LIBXML2_LIBRARIES "${EXPORTED_LIBXML2_LIBRARY}" CACHE STRING "" FORCE)
endif()

if(NOT TARGET LibXml2::LibXml2)
    add_library(LibXml2::LibXml2 STATIC IMPORTED)
    set_target_properties(LibXml2::LibXml2 PROPERTIES IMPORTED_LOCATION "${LIBXML2_LIBRARIES}")
    set_property(TARGET LibXml2::LibXml2 APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${LIBXML2_INCLUDE_DIR}")
endif()
