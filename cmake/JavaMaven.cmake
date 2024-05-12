#
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
#

function(use_javamaven SOURCE_DIR BINARY_DIR)
    if(USE_CONAN_PACKAGER)
        message("Using Conan Packager to manage installing prebuilt Java Maven external lib")
        include(${CMAKE_BINARY_DIR}/mavenTargets.cmake)
        find_package(maven)

        message("MAVEN Exec Path: ${maven_BIN_DIRS_RELEASE}/mvn")

        set(MAVEN_ARG "--version")

        # Path to conan's maven executable, check can run maven version from py script calling mvn
        # because I couldnt run conan mvn directly from calling it in CMake execute_process
        # TODO (JG): Later we could integrate this run_maven.py script directly into maven conan package
            # and set an environment variable for RUN_MAVEN_EXECUTABLE in the conanfile.py
        find_package(Python3 COMPONENTS Interpreter)
        set(PY_MAVEN_EXECUTABLE ${Python3_EXECUTABLE} "${SOURCE_DIR}/cmake/maven/python/run_maven.py" "$ENV{MAVEN_EXECUTABLE}" PARENT_SCOPE)
        execute_process(COMMAND ${PY_MAVEN_EXECUTABLE} --maven_arg1=${MAVEN_ARG})

    elseif(USE_CMAKE_FETCH_CONTENT)
        message("Using CMAKE's to integrate system Java Maven external lib")

        find_package(Maven)
    endif()
endfunction(use_javamaven)
