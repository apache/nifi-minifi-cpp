# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

include(FetchContent)

FetchContent_Declare(asio
        URL https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-38-0.tar.gz
        URL_HASH SHA256=5cf78ede456fd13b95fe692711f4f689be26c826f1f5541c7a1df3aa32bd9dbd
        SYSTEM)

FetchContent_GetProperties(asio)
if(NOT asio_POPULATED)
    FetchContent_Populate(asio)
    add_library(asio INTERFACE)
    target_include_directories(asio SYSTEM INTERFACE ${asio_SOURCE_DIR}/include)
    find_package(Threads)
    target_link_libraries(asio INTERFACE Threads::Threads OpenSSL::SSL OpenSSL::Crypto)
endif()

if (NOT TARGET asio::asio)
    add_library(asio::asio ALIAS asio)
endif()
