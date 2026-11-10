# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

find_package(spdlog CONFIG QUIET)
find_package(fmt CONFIG QUIET)
find_package(Microsoft.GSL CONFIG QUIET)
find_package(Snappy CONFIG QUIET)
find_package(asio CONFIG QUIET)
find_package(hdr_histogram CONFIG QUIET)
find_package(llhttp CONFIG QUIET)
find_package(taocpp-json CONFIG QUIET)

include(cmake/OpenSSL.cmake)

add_library(jsonsl OBJECT ${PROJECT_SOURCE_DIR}/third_party/jsonsl/jsonsl.c)
set_target_properties(jsonsl PROPERTIES C_VISIBILITY_PRESET hidden POSITION_INDEPENDENT_CODE TRUE)
target_include_directories(jsonsl SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/third_party/jsonsl)
