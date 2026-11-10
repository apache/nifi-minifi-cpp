# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

# Dummy zlib find for when we use bundled version
if(NOT BZIP2_FOUND)
    set(BZIP2_FOUND "YES" CACHE STRING "" FORCE)
    set(BZIP2_INCLUDE_DIR "${EXPORTED_BZIP2_INCLUDE_DIRS}" CACHE STRING "" FORCE)
    set(BZIP2_INCLUDE_DIRS "${EXPORTED_BZIP2_INCLUDE_DIRS}" CACHE STRING "" FORCE)
    set(BZIP2_LIBRARIES "${EXPORTED_BZIP2_LIBRARIES}" CACHE STRING "" FORCE)
endif()

if(NOT TARGET BZip2::BZip2)
    add_library(BZip2::BZip2 STATIC IMPORTED)
    set_target_properties(BZip2::BZip2 PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${BZIP2_INCLUDE_DIRS}")
    set_target_properties(BZip2::BZip2 PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${BZIP2_LIBRARIES}")
endif()
