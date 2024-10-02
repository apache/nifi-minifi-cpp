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

function(get_spdlog)
    include(GetFmt)
    get_fmt()

    if(MINIFI_SPDLOG_SOURCE STREQUAL "CONAN")
        message("Using Conan Packager to manage installing prebuilt Spdlog external lib")
        find_package(spdlog REQUIRED)

        add_library(spdlog ALIAS spdlog::spdlog)
    elseif(MINIFI_SPDLOG_SOURCE STREQUAL "BUILD")
        message("Using CMake to build Spdlog from source")
        include(Spdlog)
    endif()
endfunction(get_spdlog)
