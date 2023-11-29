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

file(GLOB SOURCES "*.cpp" "types/*.cpp" "pythonloader/PyProcLoader.cpp")

add_library(minifi-python-script-extension SHARED ${SOURCES})

target_link_libraries(minifi-python-script-extension PRIVATE ${LIBMINIFI} Threads::Threads)

include(GenericPython)
target_compile_definitions(minifi-python-script-extension PUBLIC Py_LIMITED_API=0x03060000)
target_compile_definitions(minifi-python-script-extension PUBLIC PY_SSIZE_T_CLEAN)

target_sources(minifi-python-script-extension PRIVATE ${PY_SOURCES})
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(minifi-python-script-extension PUBLIC -Wl,--no-as-needed ${Python_LIBRARIES} -Wl,--as-needed)
else()
    target_link_libraries(minifi-python-script-extension PUBLIC ${Python_LIBRARIES})
endif()

target_include_directories(minifi-python-script-extension PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/python")
target_include_directories(minifi-python-script-extension PUBLIC ${Python_INCLUDE_DIRS})

get_target_property(TARGET_EXT minifi-python-script-extension SUFFIX)

if (NOT WIN32)
    add_custom_command(
        TARGET minifi-python-script-extension
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            "libminifi-python-script-extension$<TARGET_FILE_SUFFIX:minifi-python-script-extension>"
            "${CMAKE_BINARY_DIR}/bin/minifi_native$<TARGET_FILE_SUFFIX:minifi-python-script-extension>"
        COMMENT "Creating symlink for minifi-python-script-extension"
    )

    install(
        FILES "${CMAKE_BINARY_DIR}/bin/minifi_native$<TARGET_FILE_SUFFIX:minifi-python-script-extension>"
        DESTINATION extensions
        COMPONENT minifi_python_native_module
    )
endif()

register_extension(minifi-python-script-extension "PYTHON SCRIPTING ENGINE" PYTHON-SCRIPTING-EXTENSIONS "This enables python script engine" "extensions/python/tests")
register_extension_linter(minifi-python-script-extension-linter)
