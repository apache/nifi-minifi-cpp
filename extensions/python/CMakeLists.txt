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

if (NOT (ENABLE_ALL OR ENABLE_PYTHON_SCRIPTING))
    return()
endif()

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

if (NOT WIN32)
    add_minifi_library(minifi-python-lib-loader-extension SHARED pythonlibloader/PythonLibLoader.cpp)
    set_target_properties(minifi-python-lib-loader-extension PROPERTIES HAS_CUSTOM_INITIALIZER TRUE)
    target_link_libraries(minifi-python-lib-loader-extension PRIVATE ${LIBMINIFI})
endif()

file(GLOB SOURCES "*.cpp" "types/*.cpp" "pythonloader/PyProcLoader.cpp")

add_minifi_library(minifi-python-script-extension SHARED ${SOURCES})
set_target_properties(minifi-python-script-extension PROPERTIES HAS_CUSTOM_INITIALIZER TRUE)

target_link_libraries(minifi-python-script-extension PRIVATE ${LIBMINIFI} Threads::Threads)

include(GenericPython)
if(APPLE)
    target_compile_definitions(minifi-python-lib-loader-extension PUBLIC Py_LIMITED_API=0x03090000)
    target_compile_definitions(minifi-python-script-extension PUBLIC Py_LIMITED_API=0x03090000)
    target_link_options(minifi-python-script-extension PRIVATE "-Wl,-undefined,dynamic_lookup")
elseif(WIN32)
    target_compile_definitions(minifi-python-script-extension PUBLIC Py_LIMITED_API=0x03060000)
else()
    target_compile_definitions(minifi-python-lib-loader-extension PUBLIC Py_LIMITED_API=0x03060000)
    target_compile_definitions(minifi-python-script-extension PUBLIC Py_LIMITED_API=0x03060000)
endif()
target_compile_definitions(minifi-python-script-extension PUBLIC PY_SSIZE_T_CLEAN)

target_sources(minifi-python-script-extension PRIVATE ${PY_SOURCES})

# On Linux and macOS the python library is loaded dynamically in the minifi-python-loader extension before the python script extension is loaded
if (WIN32)
    target_link_libraries(minifi-python-script-extension PUBLIC ${Python_LIBRARIES})
endif()

target_include_directories(minifi-python-script-extension PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/python")
target_include_directories(minifi-python-script-extension PUBLIC ${Python_INCLUDE_DIRS})

get_target_property(TARGET_EXT minifi-python-script-extension SUFFIX)

if (WIN32)
    add_custom_command(
        TARGET minifi-python-script-extension
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/extensions/python/pythonprocessors" "${CMAKE_BINARY_DIR}/pythonprocessors"
        COMMENT "Copy python processors to the build directory"
    )
    add_custom_command(
        TARGET minifi-python-script-extension
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/extensions/python/pythonprocessor-examples" "${CMAKE_BINARY_DIR}/pythonprocessor-examples"
        COMMENT "Copy python processor examples (not intended to be used in production) to the build directory"
    )
else()
    add_custom_command(
        TARGET minifi-python-script-extension
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            "libminifi-python-script-extension$<TARGET_FILE_SUFFIX:minifi-python-script-extension>"
            "${CMAKE_BINARY_DIR}/bin/minifi_native.so"
        COMMENT "Creating symlink for minifi-python-script-extension"
    )


    if (MINIFI_PACKAGING_TYPE STREQUAL "RPM")
        install(
                FILES "${CMAKE_BINARY_DIR}/bin/minifi_native.so"
                DESTINATION ${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME}/extensions/
                COMPONENT minifi_python_native_module
        )
    elseif (MINIFI_PACKAGING_TYPE STREQUAL "TGZ")
        install(
                FILES "${CMAKE_BINARY_DIR}/bin/minifi_native.so"
                DESTINATION extensions
                COMPONENT minifi_python_native_module
        )
    else()
        message(FATAL_ERROR "Invalid MINIFI_PACKAGING_TYPE")
    endif()

endif()

if (NOT WIN32)
    register_extension(minifi-python-lib-loader-extension "PYTHON LIB LOADER" PYTHON-LIB-LOADER-EXTENSIONS "This enables library that loads python library for python symbols")
endif()
register_extension(minifi-python-script-extension "PYTHON SCRIPTING ENGINE" PYTHON-SCRIPTING-EXTENSIONS "This enables python script engine" "extensions/python/tests")
