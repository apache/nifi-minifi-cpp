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

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

set (OTHER_SOURCES "")
if (NOT DISABLE_PYTHON_SCRIPTING)
  set (OTHER_SOURCES "pythonloader/PyProcLoader.cpp")
endif()

file(GLOB SOURCES  "ExecuteScript.cpp" "ScriptFlowFile.cpp" "ScriptProcessContext.cpp" "${OTHER_SOURCES}")

add_library(minifi-script-extensions STATIC ${SOURCES})
set_property(TARGET minifi-script-extensions PROPERTY POSITION_INDEPENDENT_CODE ON)

target_link_libraries(minifi-script-extensions ${LIBMINIFI} Threads::Threads)

if (NOT DISABLE_PYTHON_SCRIPTING)
    find_package(PythonLibs 3.5)
    if (NOT PYTHONLIBS_FOUND)
    	find_package(PythonLibs 3.0 REQUIRED)
    endif()

    include_directories(${PYTHON_INCLUDE_DIR})
    include_directories(SYSTEM ../../thirdparty/pybind11/include)

    include_directories(python)
    add_definitions(-DPYTHON_SUPPORT)

    file(GLOB PY_SOURCES  "python/*.cpp")
    add_library(minifi-python-extensions STATIC ${PY_SOURCES})

    target_link_libraries(minifi-python-extensions ${LIBMINIFI})
    target_link_libraries(minifi-python-extensions ${PYTHON_LIBRARIES})
    target_link_libraries(minifi-script-extensions minifi-python-extensions)
endif()

if (ENABLE_LUA_SCRIPTING)
    SET(CMAKE_FIND_PACKAGE_SORT_ORDER NATURAL)
    SET(CMAKE_FIND_PACKAGE_SORT_DIRECTION ASC)
    find_package(Lua 5.1 REQUIRED)
    if ("${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.4")
        message(FATAL_ERROR "No supported LUA version was found. Supported versions: 5.1, 5.2, 5.3")
    endif()

    include_directories(${LUA_INCLUDE_DIR})

    include_directories(lua)
    add_definitions(-DLUA_SUPPORT)

    file(GLOB LUA_SOURCES  "lua/*.cpp")
    add_library(minifi-lua-extensions STATIC ${LUA_SOURCES})

    target_link_libraries(minifi-lua-extensions ${LIBMINIFI})
    target_link_libraries(minifi-lua-extensions ${LUA_LIBRARIES} sol)
    target_link_libraries(minifi-script-extensions minifi-lua-extensions)

    target_compile_features(minifi-lua-extensions PUBLIC cxx_std_14)
endif()

SET (SCRIPTING-EXTENSIONS minifi-script-extensions PARENT_SCOPE)

register_extension(minifi-script-extensions)
register_extension_linter(minifi-script-extensions-linter)
