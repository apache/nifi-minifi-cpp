# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

# Dummy lz4 find for when we use bundled version
if(NOT LZ4_FOUND)
    set(LZ4_FOUND "YES" CACHE STRING "" FORCE)
    set(LZ4_INCLUDE_DIR "${EXPORTED_LZ4_INCLUDE_DIRS}" CACHE STRING "" FORCE)
    set(LZ4_INCLUDE_DIRS "${EXPORTED_LZ4_INCLUDE_DIRS}" CACHE STRING "" FORCE)
    set(LZ4_LIBRARIES "${EXPORTED_LZ4_LIBRARIES}" CACHE STRING "" FORCE)
endif()

if(NOT TARGET lz4::lz4)
    add_library(lz4::lz4 STATIC IMPORTED)
    set_target_properties(lz4::lz4 PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${LZ4_INCLUDE_DIRS}")
    set_target_properties(lz4::lz4 PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${LZ4_LIBRARIES}")
endif()
