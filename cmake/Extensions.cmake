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


define_property(GLOBAL PROPERTY EXTENSION-OPTIONS
  BRIEF_DOCS "Global extension list"
  FULL_DOCS "Global extension list")

set_property(GLOBAL PROPERTY EXTENSION-OPTIONS "")

macro(register_extension extension-name)
  get_property(extensions GLOBAL PROPERTY EXTENSION-OPTIONS)
  set_property(GLOBAL APPEND PROPERTY EXTENSION-OPTIONS ${extension-name})
  target_compile_definitions(${extension-name} PRIVATE "MODULE_NAME=${extension-name}")
  set_target_properties(${extension-name} PROPERTIES
          ENABLE_EXPORTS True
          POSITION_INDEPENDENT_CODE ON)
  if (WIN32)
    set_target_properties(${extension-name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
  else()
    set_target_properties(${extension-name} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  endif()

  if (WIN32)
    install(TARGETS ${extension-name} RUNTIME DESTINATION extensions COMPONENT bin)
  else()
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
      target_link_options(${extension-name} PRIVATE "-Wl,--disable-new-dtags")
    endif()
    set_target_properties(${extension-name} PROPERTIES INSTALL_RPATH "$ORIGIN")
    install(TARGETS ${extension-name} LIBRARY DESTINATION extensions COMPONENT bin)
  endif()
endmacro()

### TESTING MACROS

define_property(GLOBAL PROPERTY EXTENSION-TESTS
  BRIEF_DOCS "Global extension tests"
  FULL_DOCS "Global extension tests")

set_property(GLOBAL PROPERTY EXTENSION-TESTS "")

macro(register_extension_test extension-dir)
  if (NOT SKIP_TESTS)
    get_property(extensions GLOBAL PROPERTY EXTENSION-TESTS)
    set_property(GLOBAL APPEND PROPERTY EXTENSION-TESTS "${extension-dir}")
  endif()
endmacro()

function(registerTest dirName)
  if (NOT SKIP_TESTS)
    add_subdirectory(${dirName})
  endif()
endfunction(registerTest)

### FUNCTION TO CREATE AN EXTENSION

function(createExtension extensionGuard extensionName description dirName)
  add_subdirectory(${dirName})
  ADD_FEATURE_INFO("${extensionName}" ${extensionGuard} "${description}")
  mark_as_advanced(${extensionGuard})
  if (ARGV4)
    register_extension_test(${ARGV4})
  endif(ARGV4)
  if (ARGV5 AND ARGV6)
    if (${ARGV5})
      add_subdirectory(${ARGV6})
    endif()
  endif()
endfunction()


macro(register_extension_linter target-name)
  if (ENABLE_LINTER)
    get_property(extensions GLOBAL PROPERTY EXTENSION-LINTERS)
    set_property(GLOBAL APPEND PROPERTY EXTENSION-LINTERS "${target-name}")
    add_custom_target(${target-name}
    COMMAND python ${CMAKE_SOURCE_DIR}/thirdparty/google-styleguide/run_linter.py -q -i ${CMAKE_CURRENT_LIST_DIR}/)
  endif()
endmacro()
