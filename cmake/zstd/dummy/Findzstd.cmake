# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

# Dummy zstd find for when we use bundled version
if(NOT zstd_FOUND)
    set(zstd_FOUND "YES" CACHE STRING "" FORCE)
    set(ZSTD_INCLUDE_DIR "${EXPORTED_ZSTD_INCLUDE_DIRS}" CACHE STRING "" FORCE)
    set(ZSTD_INCLUDE_DIRS "${EXPORTED_ZSTD_INCLUDE_DIRS}" CACHE STRING "" FORCE)
    set(ZSTD_LIBRARIES "${EXPORTED_ZSTD_LIBRARIES}" CACHE STRING "" FORCE)
    set(ZSTD_LIBRARY "${EXPORTED_ZSTD_LIBRARIES}" CACHE STRING "" FORCE)
endif()

if(NOT TARGET zstd::zstd)
    add_library(zstd::zstd STATIC IMPORTED)
    set_target_properties(zstd::zstd PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${ZSTD_INCLUDE_DIRS}")
    set_target_properties(zstd::zstd PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${ZSTD_LIBRARIES}")
endif()
