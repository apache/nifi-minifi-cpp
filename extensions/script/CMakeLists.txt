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

if (NOT (ENABLE_ALL OR ENABLE_SCRIPTING))
    return()
endif()

include(Sol2)

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES  "ExecuteScript.cpp" "ScriptFlowFile.cpp" "ScriptProcessContext.cpp")

add_library(minifi-script-extensions SHARED ${SOURCES})

target_link_libraries(minifi-script-extensions ${LIBMINIFI} Threads::Threads)

if (NOT DISABLE_PYTHON_SCRIPTING)
    find_package(PythonLibs 3.5)
    if (NOT PYTHONLIBS_FOUND)
    	find_package(PythonLibs 3.0 REQUIRED)
    endif()

    include(Pybind11)
    add_definitions(-DPYTHON_SUPPORT)

    file(GLOB PY_SOURCES  "python/*.cpp" "pythonloader/PyProcLoader.cpp")
    target_sources(minifi-script-extensions PRIVATE ${PY_SOURCES})
    target_link_libraries(minifi-script-extensions ${PYTHON_LIBRARIES})
    target_link_libraries(minifi-script-extensions pybind11)

    target_include_directories(minifi-script-extensions PRIVATE python)
    target_include_directories(minifi-script-extensions SYSTEM PRIVATE ${PYTHON_INCLUDE_DIR})
    target_include_directories(minifi-script-extensions SYSTEM PRIVATE ../../thirdparty/pybind11/include)
endif()

if (ENABLE_LUA_SCRIPTING)
    SET(CMAKE_FIND_PACKAGE_SORT_ORDER NATURAL)
    SET(CMAKE_FIND_PACKAGE_SORT_DIRECTION ASC)
    find_package(Lua 5.1 REQUIRED)

    target_include_directories(minifi-script-extensions PRIVATE lua)
    target_include_directories(minifi-script-extensions PUBLIC ${LUA_INCLUDE_DIR})
    add_definitions(-DLUA_SUPPORT)

    file(GLOB LUA_SOURCES  "lua/*.cpp")
    target_sources(minifi-script-extensions PRIVATE ${LUA_SOURCES})

    target_link_libraries(minifi-script-extensions ${LUA_LIBRARIES} sol2)
    target_compile_features(minifi-script-extensions PUBLIC cxx_std_14)
endif()

register_extension(minifi-script-extensions "SCRIPTING EXTENSIONS" SCRIPTING-EXTENSIONS "This enables scripting" "extensions/script/tests")
register_extension_linter(minifi-script-extensions-linter)
