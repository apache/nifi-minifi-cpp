
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


if (NOT WIN32)

set(ODBC_FOUND "YES" CACHE STRING "" FORCE)
set(ODBC_INCLUDE_DIR "${ODBC_BYPRODUCT_DIR}/include" CACHE STRING "" FORCE)
set(ODBC_LIBRARIES "${ODBC_BYPRODUCT_DIR}/lib/libodbc.a" CACHE STRING "" FORCE)
set(ODBC_LIBRARY "${ODBC_BYPRODUCT_DIR}/lib/libodbc.a" CACHE STRING "" FORCE)

 if(NOT TARGET ODBC::ODBC )
    add_library(ODBC::ODBC UNKNOWN IMPORTED)
    set_target_properties(ODBC::ODBC PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${ODBC_INCLUDE_DIRS}")
    
      set_target_properties(ODBC::ODBC PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION "${ODBC_LIBRARIES}")
    
  endif()
endif()
