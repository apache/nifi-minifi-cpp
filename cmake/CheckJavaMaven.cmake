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
function(check_javamaven_version)
    if(TARGET openjdk::openjdk AND TARGET maven::maven)
        execute_process(COMMAND ${PY_MAVEN_EXECUTABLE} 
            --maven_executable=$ENV{MAVEN_EXECUTABLE}
            --maven_arg1="--version"
            RESULT_VARIABLE mvn_result
            OUTPUT_VARIABLE mvn_output
            ERROR_VARIABLE mvn_error
        )
    else()
        if (WIN32)
            execute_process(COMMAND cmd /c "${MAVEN_EXECUTABLE}" "--version"
                RESULT_VARIABLE mvn_result
                OUTPUT_VARIABLE mvn_output
                ERROR_VARIABLE mvn_error)
        else()
            execute_process(COMMAND "${MAVEN_EXECUTABLE}" "--version"
                RESULT_VARIABLE mvn_result
                OUTPUT_VARIABLE mvn_output
                ERROR_VARIABLE mvn_error)
        endif()
    endif()

    if("${mvn_result}" STREQUAL "0")
        message("....Successfully Ran Maven Version....")
        message("${mvn_output}")

        if(DEFINED ENV{MAVEN_EXECUTABLE})
            message("MAVEN_EXECUTABLE = $ENV{MAVEN_EXECUTABLE}")
            set(MAVEN_FOUND TRUE CACHE STRING "" FORCE)
        else()
            message("MAVEN_EXECUTABLE not set")
            set(MAVEN_FOUND FALSE CACHE STRING "" FORCE)
        endif()

        if(DEFINED ENV{JAVA_HOME})
            message("JAVA_HOME = $ENV{JAVA_HOME}")
            set(JAVA_FOUND TRUE CACHE STRING "" FORCE)
        else()
            message("JAVA_HOME not set")
            set(JAVA_FOUND FALSE CACHE STRING "" FORCE)
        endif()
    else()
        message("Maven failed ${mvn_result} ${mvn_output} ${mvn_error}")
    endif()

    message("JAVA_FOUND = ${JAVA_FOUND}")
    message("MAVEN_FOUND = ${MAVEN_FOUND}")

endfunction(check_javamaven_version)
