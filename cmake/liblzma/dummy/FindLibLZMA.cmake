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

if(NOT LIBLZMA_FOUND)
    set(LIBLZMA_FOUND "YES" CACHE STRING "" FORCE)

    set(LIBLZMA_INCLUDE_DIR "${EXPORTED_LIBLZMA_INCLUDE_DIR}" CACHE STRING "" FORCE)
    set(LIBLZMA_INCLUDE_DIRS "${EXPORTED_LIBLZMA_INCLUDE_DIR}" CACHE STRING "" FORCE)

    if (WIN32)
        if (EXISTS "${EXPORTED_LIBLZMA_LIB_DIR}/liblzma.lib")
            set(LIBLZMA_LIBRARIES "${EXPORTED_LIBLZMA_LIB_DIR}/liblzma.lib" CACHE STRING "" FORCE)
        elseif (EXISTS "${EXPORTED_LIBLZMA_LIB_DIR}/${CMAKE_BUILD_TYPE}/liblzma.lib")
            set(LIBLZMA_LIBRARIES "${EXPORTED_LIBLZMA_LIB_DIR}/${CMAKE_BUILD_TYPE}/liblzma.lib" CACHE STRING "" FORCE)
        else()
            message(FATAL_ERROR "Could not find liblzma.lib")
        endif()
    else()
        set(LIBLZMA_LIBRARIES "${EXPORTED_LIBLZMA_LIBRARY}" CACHE STRING "" FORCE)
    endif()
endif()
