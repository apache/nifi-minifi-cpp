# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(NOT LIBLZMA_FOUND)
    set(LIBLZMA_FOUND "YES" CACHE STRING "" FORCE)

    set(LIBLZMA_INCLUDE_DIR "${EXPORTED_LIBLZMA_INCLUDE_DIR}" CACHE STRING "" FORCE)
    set(LIBLZMA_INCLUDE_DIRS "${EXPORTED_LIBLZMA_INCLUDE_DIR}" CACHE STRING "" FORCE)

    if (WIN32)
        if (EXISTS "${EXPORTED_LIBLZMA_LIB_DIR}/lzma.lib")
            set(LIBLZMA_LIBRARIES "${EXPORTED_LIBLZMA_LIB_DIR}/lzma.lib" CACHE STRING "" FORCE)
        elseif (EXISTS "${EXPORTED_LIBLZMA_LIB_DIR}/${CMAKE_BUILD_TYPE}/lzma.lib")
            set(LIBLZMA_LIBRARIES "${EXPORTED_LIBLZMA_LIB_DIR}/${CMAKE_BUILD_TYPE}/lzma.lib" CACHE STRING "" FORCE)
        else()
            message(FATAL_ERROR "Could not find lzma.lib")
        endif()
    else()
        set(LIBLZMA_LIBRARIES "${EXPORTED_LIBLZMA_LIBRARY}" CACHE STRING "" FORCE)
    endif()
endif()
