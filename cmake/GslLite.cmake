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
    message("Using Conan Packager to manage installing prebuilt gsl-lite external lib")
    include(${CMAKE_BINARY_DIR}/gsl-lite-config.cmake)

    # Add necessary definitions based on the value of STRICT_GSL_CHECKS, see gsl-lite README for more details
    list(APPEND gsl-lite_DEFINITIONS_RELEASE gsl_CONFIG_DEFAULTS_VERSION=1)

    target_compile_definitions(gsl::gsl-lite INTERFACE ${gsl-lite_DEFINITIONS_RELEASE})
elseif(USE_CMAKE_FETCH_CONTENT)
    message("Using CMAKE's FetchContent to manage source building gsl-lite external lib")

    include(FetchContent)

    FetchContent_Declare(gsl-lite
        URL      https://github.com/gsl-lite/gsl-lite/archive/refs/tags/v0.39.0.tar.gz
        URL_HASH SHA256=f80ec07d9f4946097a1e2554e19cee4b55b70b45d59e03a7d2b7f80d71e467e9
    )
    FetchContent_MakeAvailable(gsl-lite)

    # Add necessary definitions based on the value of STRICT_GSL_CHECKS, see gsl-lite README for more details
    list(APPEND GslDefinitions gsl_CONFIG_DEFAULTS_VERSION=1)
    list(APPEND GslDefinitionsNonStrict gsl_CONFIG_CONTRACT_VIOLATION_THROWS gsl_CONFIG_NARROW_THROWS_ON_TRUNCATION=1)
    if (STRICT_GSL_CHECKS STREQUAL "AUDIT")
        list(APPEND GslDefinitions gsl_CONFIG_CONTRACT_CHECKING_AUDIT)
    endif()
    if (NOT STRICT_GSL_CHECKS)  # OFF (or any other falsey string) matches, AUDIT/ON/DEBUG_ONLY don't match
        list(APPEND GslDefinitions ${GslDefinitionsNonStrict})
    endif()
    if (STRICT_GSL_CHECKS STREQUAL "DEBUG_ONLY")
        list(APPEND GslDefinitions $<$<NOT:$<CONFIG:Debug>>:${GslDefinitionsNonStrict}>)
    endif()
    target_compile_definitions(gsl-lite INTERFACE ${GslDefinitions})


endif()
