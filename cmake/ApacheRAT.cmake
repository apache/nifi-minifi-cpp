# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

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
