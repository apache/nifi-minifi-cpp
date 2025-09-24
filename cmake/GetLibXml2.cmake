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

function(get_libxml2 SOURCE_DIR BINARY_DIR)
    if(MINIFI_LIBXML2_SOURCE STREQUAL "CONAN")
        message("Using Conan to install libxml2")
        find_package(libxml2 REQUIRED)
    elseif(MINIFI_LIBXML2_SOURCE STREQUAL "BUILD")
        message("Using CMake to build libxml2 from source")
        include(LibXml2)
    endif()
endfunction(get_libxml2)
