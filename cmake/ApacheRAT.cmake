# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

set(RAT_JAR "" CACHE FILEPATH "Path to the Apache RAT jar")

find_package(Java COMPONENTS Runtime)

if(NOT RAT_JAR)
  message(STATUS "RAT_JAR not set, 'rat' target not available")
  return()
endif()

if(NOT Java_FOUND)
  message(FATAL_ERROR "RAT_JAR is set but no Java runtime was found")
endif()

if(NOT EXISTS "${RAT_JAR}")
  message(FATAL_ERROR "RAT_JAR does not exist: ${RAT_JAR}")
endif()

add_custom_target(rat
  COMMAND ${Java_JAVA_EXECUTABLE} -jar "${RAT_JAR}"
          --input-exclude-parsed-scm GIT
          --input-exclude-file "${CMAKE_SOURCE_DIR}/.rat-excludes"
          --output-style unapproved-licenses
          -- "${CMAKE_SOURCE_DIR}"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Running Apache RAT license check"
  VERBATIM
)
