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
if(USE_CONAN_PACKAGER)
    message("Using Conan Packager to manage installing prebuilt range-v3 external lib")
    include(${CMAKE_BINARY_DIR}/range-v3-config.cmake)

    # set(RANGE_V3_INCLUDE_DIRS "${range-v3_INCLUDE_DIRS}" CACHE STRING "" FORCE)
    # set(RANGE_V3_LIBRARIES "${range-v3_LIB_DIRS_RELEASE}" CACHE STRING "" FORCE)

elseif(USE_CMAKE_FETCH_CONTENT)
    message("Using CMAKE's FetchContent to manage source building range-v3 external lib")

    include(FetchContent)

    FetchContent_Declare(range-v3_src
        URL      https://github.com/ericniebler/range-v3/archive/refs/tags/0.12.0.tar.gz
        URL_HASH SHA256=015adb2300a98edfceaf0725beec3337f542af4915cec4d0b89fa0886f4ba9cb
    )
    FetchContent_MakeAvailable(range-v3_src)

endif()
