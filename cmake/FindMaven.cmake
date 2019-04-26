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

set (MAVEN_HINT_UNIX /usr/bin CACHE STRING "maven default directory")

if (WIN32)
    find_program(MAVEN_EXECUTABLE NAMES mvn.bat mvn)
else()
    find_program(MAVEN_EXECUTABLE NAMES mvn
        HINTS ENV${MAVEN_HINT_UNIX}/mvn ${MAVEN_HINT_UNIX}/mvn)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args (Maven
  FOUND_VAR MAVEN_FOUND
  REQUIRED_VARS MAVEN_EXECUTABLE
)

mark_as_advanced(MAVEN_FOUND MAVEN_EXECUTABLE)
