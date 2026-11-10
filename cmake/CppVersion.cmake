# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

function(set_cpp_version)
    if (MSVC)
        if ((MSVC_VERSION GREATER "1930") OR (MSVC_VERSION EQUAL "1930"))
            add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/std:c++latest>)
            add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/permissive->)
        else()
            message(FATAL_ERROR "The Visual Studio C++ compiler ${CMAKE_CXX_COMPILER} is not supported. Please use Visual Studio 2022 or newer.")
        endif()
        set(CMAKE_CXX_STANDARD 23 PARENT_SCOPE)
    else()
        include(CheckCXXCompilerFlag)
        CHECK_CXX_COMPILER_FLAG("-std=c++23" COMPILER_SUPPORTS_CXX23)
        if(COMPILER_SUPPORTS_CXX23)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++23" PARENT_SCOPE)
            set(CMAKE_CXX_STANDARD 23 PARENT_SCOPE)
        else()
            message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no support for -std=c++23. Please use a more recent C++ compiler version.")
        endif()
    endif()

    set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)
endfunction(set_cpp_version)
