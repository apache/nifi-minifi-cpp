# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


if (NOT (ENABLE_ALL OR ENABLE_LLAMACPP))
    return()
endif()

set(ADDITIONAL_LIBS "")
if(MINIFI_LLAMACPP_SOURCE STREQUAL "CONAN")
    find_package(nlohmann_json REQUIRED)
    set(ADDITIONAL_LIBS "nlohmann_json::nlohmann_json")
endif()
include(GetLlamaCpp)

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES "processors/*.cpp" "ExtensionInitializer.cpp")

add_minifi_library(minifi-llamacpp SHARED ${SOURCES})
target_include_directories(minifi-llamacpp PUBLIC "${CMAKE_SOURCE_DIR}/extensions/llamacpp")
target_include_directories(minifi-llamacpp PUBLIC "${LLAMACPP_INCLUDE_DIRS}")

target_link_libraries(minifi-llamacpp minifi-cpp-extension-lib llama-cpp::llama llama-cpp::mtmd llama-cpp::common "${ADDITIONAL_LIBS}")

register_c_api_extension(minifi-llamacpp "LLAMACPP EXTENSION" LLAMACPP-EXTENSION "Provides llama.cpp support" "extensions/llamacpp/tests")

