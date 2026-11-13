# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


if (NOT (ENABLE_ALL OR ENABLE_AZURE))
    return()
endif()

include(GetAzureSdkCpp)

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES "*.cpp" "storage/*.cpp" "controllerservices/*.cpp" "processors/*.cpp" "utils/*.cpp")

add_minifi_library(minifi-azure SHARED ${SOURCES})

target_compile_features(minifi-azure PUBLIC cxx_std_14)

target_include_directories(minifi-azure BEFORE PRIVATE controllerservices)
target_include_directories(minifi-azure BEFORE PRIVATE processors)
target_include_directories(minifi-azure BEFORE PRIVATE storage)
target_include_directories(minifi-azure BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/extensions/azure)

target_link_libraries(minifi-azure ${LIBMINIFI} Threads::Threads)
target_link_libraries(minifi-azure Azure::azure-storage-files-datalake Azure::azure-storage-blobs Azure::azure-storage-common Azure::azure-core Azure::azure-identity)

if (WIN32)
    target_link_libraries(minifi-azure crypt32.lib bcrypt.lib)
endif()

register_extension(minifi-azure "AZURE EXTENSIONS" AZURE-EXTENSIONS "This enables Azure support" "extensions/azure/tests")
