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

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++14 " COMPILER_SUPPORTS_CXX14)
CHECK_CXX_COMPILER_FLAG("-std=c++1y " COMPILER_SUPPORTS_CXX1Y)

if (ENABLE_LUA_SCRIPTING)
    if(COMPILER_SUPPORTS_CXX14)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
    elseif(COMPILER_SUPPORTS_CXX1Y)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y")
    else()
        message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++14 support and lua is enabled. Please use a different C++ compiler or disable lua.")
    endif()

    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt) 

set (OTHER_SOURCES "")
if (NOT DISABLE_PYTHON_SCRIPTING)
  set (OTHER_SOURCES "pythonloader/PyProcLoader.cpp")
endif()

file(GLOB SOURCES  "ExecuteScript.cpp" "ScriptFlowFile.cpp" "ScriptProcessContext.cpp" "${OTHER_SOURCES}")

add_library(minifi-script-extensions STATIC ${SOURCES})
set_property(TARGET minifi-script-extensions PROPERTY POSITION_INDEPENDENT_CODE ON)
if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(PUBLIC minifi-script-extensions "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries(minifi-script-extensions "${CMAKE_THREAD_LIBS_INIT}")
endif()

target_link_libraries(minifi-script-extensions ${LIBMINIFI})

if (NOT DISABLE_PYTHON_SCRIPTING)
    find_package(PythonLibs 3.5)
    if (NOT PYTHONLIBS_FOUND)
    	find_package(PythonLibs 3.0 REQUIRED)
    endif()

    include_directories(${PYTHON_INCLUDE_DIR})
    include_directories(../../thirdparty/pybind11/include)

    include_directories(python)
    add_definitions(-DPYTHON_SUPPORT)

    file(GLOB PY_SOURCES  "python/*.cpp")
    add_library(minifi-python-extensions STATIC ${PY_SOURCES})

    target_link_libraries(minifi-python-extensions ${LIBMINIFI})
    target_link_libraries(minifi-python-extensions ${PYTHON_LIBRARIES})
    target_link_libraries(minifi-script-extensions minifi-python-extensions)
endif()

if (ENABLE_LUA_SCRIPTING)
    find_package(Lua REQUIRED)

    include_directories(${LUA_INCLUDE_DIR})

    include_directories(lua)
    add_definitions(-DLUA_SUPPORT)

    file(GLOB LUA_SOURCES  "lua/*.cpp")
    add_library(minifi-lua-extensions STATIC ${LUA_SOURCES})

    target_link_libraries(minifi-lua-extensions ${LIBMINIFI})
    target_link_libraries(minifi-lua-extensions ${LUA_LIBRARIES} sol)
    target_link_libraries(minifi-script-extensions minifi-lua-extensions)
endif()

SET (SCRIPTING-EXTENSIONS minifi-script-extensions PARENT_SCOPE)

register_extension(minifi-script-extensions)

