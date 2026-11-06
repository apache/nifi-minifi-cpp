# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

add_library(minifi-api-common INTERFACE)
target_include_directories(minifi-api-common INTERFACE common/include)
target_link_libraries(minifi-api-common INTERFACE gsl-lite::gsl-lite)
target_compile_definitions(minifi-api-common INTERFACE MINIFI_VERSION_STR="${MINIFI_VERSION_STR}")

add_library(minifi-api INTERFACE)
target_include_directories(minifi-api INTERFACE include)
target_link_libraries(minifi-api INTERFACE minifi-api-common)
target_compile_definitions(minifi-api INTERFACE MINIFI_REGISTER_EXTENSION_FN=MinifiRegisterCppExtension_${BUILD_IDENTIFIER})

add_library(minifi-c-api INTERFACE)
target_include_directories(minifi-c-api INTERFACE include)
if (APPLE)
  target_link_options(minifi-c-api INTERFACE -undefined dynamic_lookup)
endif()

if (WIN32)
  set(IMPORT_LIB_FILE ${CMAKE_CURRENT_BINARY_DIR}/minifi-c-api.lib)
  add_custom_target(minifi_c_api_import_lib
    COMMAND lib.exe /def:${CMAKE_CURRENT_SOURCE_DIR}/minifi-api.def /out:${IMPORT_LIB_FILE} /machine:x64
    BYPRODUCTS ${IMPORT_LIB_FILE}
  )

  add_dependencies(minifi-c-api minifi_c_api_import_lib)
  target_link_libraries(minifi-c-api INTERFACE ${IMPORT_LIB_FILE})
endif()
