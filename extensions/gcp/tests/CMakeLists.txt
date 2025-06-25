#
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
#
file(GLOB GCS_TESTS  "*.cpp")
FOREACH(testfile ${GCS_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_minifi_executable("${testfilename}" "${testfile}")

    if (WIN32)
        target_compile_definitions(${testfilename} PRIVATE _SILENCE_CXX20_REL_OPS_DEPRECATION_WARNING)
    else()
        target_compile_options(${testfilename} PRIVATE -Wno-error=deprecated-declarations)  # Suppress deprecation warnings for std::rel_ops usage
    endif()

    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/standard-processors")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/expression-language")
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/gcp")

    target_include_directories(${testfilename} SYSTEM BEFORE PRIVATE ${googletest_INCLUDE_DIRS})
    target_include_directories(${testfilename} SYSTEM BEFORE PRIVATE ${googletest_SOURCE_DIR}/googletest/include)
    target_include_directories(${testfilename} SYSTEM BEFORE PRIVATE ${googletest_SOURCE_DIR}/googlemock/include)
    createTests("${testfilename}")

    target_link_libraries(${testfilename} minifi-gcp)
    target_link_libraries(${testfilename} minifi-standard-processors)
    target_link_libraries(${testfilename} minifi-expression-language-extensions)
    target_link_libraries(${testfilename} gtest_main gmock)

    gtest_add_tests(TARGET "${testfilename}")
ENDFOREACH()
