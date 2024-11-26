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

function(set_cpp_version)
    if (MSVC)
        if ((MSVC_VERSION GREATER "1930") OR (MSVC_VERSION EQUAL "1930"))
            add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/std:c++latest>)
            add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/permissive->)
        else()
            message(STATUS "The Visual Studio C++ compiler ${CMAKE_CXX_COMPILER} is not supported. Please use Visual Studio 2022 or newer.")
        endif()
        set(CMAKE_CXX_STANDARD 20 PARENT_SCOPE)
    else()
        include(CheckCXXCompilerFlag)
        CHECK_CXX_COMPILER_FLAG("-std=c++20" COMPILER_SUPPORTS_CXX20)
        CHECK_CXX_COMPILER_FLAG("-std=c++2a" COMPILER_SUPPORTS_CXX2A)
        if(COMPILER_SUPPORTS_CXX20)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20" PARENT_SCOPE)
        elseif(COMPILER_SUPPORTS_CXX2A)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++2a" PARENT_SCOPE)
        else()
            message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no support for -std=c++20 or -std=c++2a. Please use a more recent C++ compiler version.")
        endif()
        set(CMAKE_CXX_STANDARD 20 PARENT_SCOPE)
    endif()

    set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)
endfunction(set_cpp_version)
