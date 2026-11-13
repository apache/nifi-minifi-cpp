# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(NOT LIBXML2_FOUND)
    set(LIBXML2_FOUND "YES" CACHE STRING "" FORCE)
    set(LIBXML2_INCLUDE_DIR "${EXPORTED_LIBXML2_INCLUDE_DIR}" CACHE STRING "" FORCE)
    set(LIBXML2_INCLUDE_DIRS "${EXPORTED_LIBXML2_INCLUDE_DIR}" CACHE STRING "" FORCE)
    set(LIBXML2_LIBRARIES "${EXPORTED_LIBXML2_LIBRARY}" CACHE STRING "" FORCE)
endif()

if(NOT TARGET LibXml2::LibXml2)
    add_library(LibXml2::LibXml2 STATIC IMPORTED)
    set_target_properties(LibXml2::LibXml2 PROPERTIES IMPORTED_LOCATION "${LIBXML2_LIBRARIES}")
    set_property(TARGET LibXml2::LibXml2 APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${LIBXML2_INCLUDE_DIR}")
endif()
