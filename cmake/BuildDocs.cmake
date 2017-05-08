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

FIND_PACKAGE(Doxygen)

if(DOXYGEN_FOUND)
if(EXISTS ${DOXYGEN_EXECUTABLE})

MESSAGE("-- Creating API Documentation using ${DOXYGEN_EXECUTABLE}")
SET(DOXYGEN_INPUT "../docs/Doxyfile")
SET(DOXYGEN_OUTPUT "../docs")

ADD_CUSTOM_COMMAND(
  OUTPUT ${DOXYGEN_OUTPUT}
  COMMAND ${CMAKE_COMMAND} -E echo_append "Building LibMiNiFi API Documentation..."
  COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_INPUT}
  COMMAND ${CMAKE_COMMAND} -E echo "Done."
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS ${DOXYGEN_INPUT}
  )

ADD_CUSTOM_TARGET(docs ${DOXYGEN_EXECUTABLE} ${DOXYGEN_INPUT})
else()
ADD_CUSTOM_TARGET(docs echo "Doxygen binary does not exist. Cannot create documentation... Please install Doxygen or verify correct installation.")
endif()
else()
ADD_CUSTOM_TARGET(docs echo "Doxygen does not exist. Please install it. Cannot create documentation...")
endif(DOXYGEN_FOUND)