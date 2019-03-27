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
  set(ZLIB_FOUND "YES" CACHE STRING "" FORCE)
    
  set(ZLIB_INCLUDE_DIR "${ZLIB_BYPRODUCT_INCLUDE}" CACHE STRING "" FORCE)
  set(ZLIB_INCLUDE_DIRS "${ZLIB_BYPRODUCT_INCLUDE}" CACHE STRING "" FORCE)
  
  set(ZLIB_LIBRARY "${ZLIB_BYPRODUCT}" CACHE STRING "" FORCE)
  set(ZLIB_LIBRARIES "${ZLIB_LIBRARY}" CACHE STRING "" FORCE)
  set(ZLIB_LIBRARY_RELEASE "${ZLIB_LIBRARY}" CACHE STRING "" FORCE)
  set(ZLIB_LIBRARY_DEBUG "${ZLIB_LIBRARY}" CACHE STRING "" FORCE)
  message("ZLIB LIB is located at is ${ZLIB_LIBRARIES}")
  
  if(NOT TARGET ZLIB::ZLIB
      )
    add_library(ZLIB::ZLIB UNKNOWN IMPORTED)
    set_target_properties(ZLIB::ZLIB PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIR}")
          set_target_properties(ZLIB::ZLIB PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION "${ZLIB_LIBRARY}")
    
  endif()