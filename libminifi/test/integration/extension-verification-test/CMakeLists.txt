# Licensed to the Apache Software Foundation (ASF) under one
#
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

add_minifi_executable(ExtensionVerificationTests ExtensionVerificationTests.cpp)
createIntegrationTests(ExtensionVerificationTests)
target_link_libraries(ExtensionVerificationTests core-minifi Catch2WithMain Threads::Threads)
add_test(NAME ExtensionVerificationTests COMMAND ExtensionVerificationTests)

function(create_loading_test_extension extension-name extension-main)
    add_library(${extension-name} SHARED ${extension-main})
    add_dependencies(ExtensionVerificationTests ${extension-name})
    target_compile_definitions(${extension-name} PRIVATE MODULE_NAME=${extension-name})
    set_target_properties(${extension-name} PROPERTIES ENABLE_EXPORTS True POSITION_INDEPENDENT_CODE ON)
    if(WIN32)
        set_target_properties(${extension-name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
            WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
    else()
        set_target_properties(${extension-name} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    endif()
endfunction()

function(create_c_test_extension extension-name custom_api_version)
    set(extension-name "test-extension-loading-${extension-name}")
    create_loading_test_extension(${extension-name} CApiExtension.cpp)
    target_link_libraries(${extension-name} minifi-cpp-extension-lib)
    target_compile_definitions(${extension-name} PRIVATE MINIFI_TEST_API_VERSION=${custom_api_version})
    target_compile_definitions(${extension-name}
            PRIVATE "EXTENSION_NAME=${extension-name}" "EXTENSION_VERSION=${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
endfunction()

add_library(dummy-core-minifi INTERFACE)
target_include_directories(dummy-core-minifi INTERFACE $<TARGET_PROPERTY:minifi-extension-framework,INTERFACE_INCLUDE_DIRECTORIES>)
if (WIN32)
    set(DUMMY_IMPORT_LIB_FILE ${CMAKE_CURRENT_BINARY_DIR}/dummy-cpp-api.lib)
    add_custom_target(dummy_cpp_api_import_lib
            COMMAND lib.exe /def:${CMAKE_CURRENT_SOURCE_DIR}/dummy-cpp-api.def /out:${DUMMY_IMPORT_LIB_FILE} /machine:x64
            BYPRODUCTS ${DUMMY_IMPORT_LIB_FILE}
    )

    add_dependencies(dummy-core-minifi dummy_cpp_api_import_lib)
    target_link_libraries(dummy-core-minifi INTERFACE ${DUMMY_IMPORT_LIB_FILE})
elseif (APPLE)
    target_link_options(dummy-core-minifi INTERFACE -undefined dynamic_lookup)
endif()

create_loading_test_extension(test-extension-loading-invalid-build-id-cpp CppApiExtension.cpp)
target_link_libraries(test-extension-loading-invalid-build-id-cpp dummy-core-minifi)
target_compile_definitions(test-extension-loading-invalid-build-id-cpp PRIVATE MINIFI_CREATE_EXTENSION_FN=MinifiCreateCppExtension_NonMatchingBuildId)

create_loading_test_extension(test-extension-loading-valid-build-id-cpp CppApiExtension.cpp)
target_link_libraries(test-extension-loading-valid-build-id-cpp dummy-core-minifi)
target_compile_definitions(test-extension-loading-valid-build-id-cpp PRIVATE MINIFI_CREATE_EXTENSION_FN=MinifiCreateCppExtension_MatchingBuildId)

create_c_test_extension(max-version 10)
create_c_test_extension(valid-version 7)
create_c_test_extension(min-version 5)
create_c_test_extension(greater-version 11)
create_c_test_extension(smaller-version 4)
create_loading_test_extension(test-extension-loading-missing-init InvalidMissingInitExtension.cpp)

create_loading_test_extension(test-extension-loading-create-not-called CreateNotCalled.cpp)
target_link_libraries(test-extension-loading-create-not-called minifi-cpp-extension-lib)
target_compile_definitions(test-extension-loading-create-not-called PRIVATE MINIFI_TEST_API_VERSION=10)
