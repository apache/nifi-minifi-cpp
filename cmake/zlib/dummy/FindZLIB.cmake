# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

# Dummy zlib find for when we use bundled version
if(NOT ZLIB_FOUND)
    set(ZLIB_FOUND "YES" CACHE STRING "" FORCE)
    set(ZLIB_INCLUDE_DIR "${EXPORTED_ZLIB_INCLUDE_DIRS}" CACHE STRING "" FORCE)
    set(ZLIB_INCLUDE_DIRS "${EXPORTED_ZLIB_INCLUDE_DIRS}" CACHE STRING "" FORCE)
    set(ZLIB_LIBRARIES "${EXPORTED_ZLIB_LIBRARIES}" CACHE STRING "" FORCE)
endif()

if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB STATIC IMPORTED)
    set_target_properties(ZLIB::ZLIB PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIRS}")
    set_target_properties(ZLIB::ZLIB PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${ZLIB_LIBRARIES}")
    add_dependencies(ZLIB::ZLIB zlibstatic)
endif()
