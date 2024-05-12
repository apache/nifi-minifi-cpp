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

if(NOT (ENABLE_ALL OR ENABLE_JNI))
    return()
endif()

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

message(STATUS "JAVA_HOME: '$ENV{JAVA_HOME}'")
find_package(JNI REQUIRED)
# find_package(Java REQUIRED)
find_package(Maven REQUIRED)

include(JavaOpenJDK)


message(STATUS "MAVEN: ${MAVEN_EXECUTABLE}")
include_directories(${JNI_INCLUDE_DIRS})

file(GLOB SOURCES  "jvm/*.cpp" "*.cpp")

add_minifi_library(minifi-jni SHARED ${SOURCES})

set(JNI_FRAMEWORK_JAR_SOURCE "${CMAKE_SOURCE_DIR}/extensions/jni/nifi-framework-jni")
set(JNI_FRAMEWORK_JAR_BIN "${CMAKE_CURRENT_BINARY_DIR}/" )

file(COPY ${JNI_FRAMEWORK_JAR_SOURCE} DESTINATION ${JNI_FRAMEWORK_JAR_BIN})

set(JNI_ASSEMBLY_JAR_SOURCE "${CMAKE_SOURCE_DIR}/extensions/jni/nifi-minifi-jni-assembly")
set(JNI_ASSEMBLY_JAR_BIN "${CMAKE_CURRENT_BINARY_DIR}/" )

file(COPY ${JNI_FRAMEWORK_JAR_SOURCE} DESTINATION ${JNI_FRAMEWORK_JAR_BIN})
file(COPY ${JNI_ASSEMBLY_JAR_SOURCE} DESTINATION ${JNI_ASSEMBLY_JAR_BIN})

message(STATUS "Building NiFi assembly for packaging ....")
# duplicating the execute_process because a version of cmake tested exhibited failures
# with an empty cmd before ${MAVEN_EXECUTABLE}. Until this is resolved ( or proven to be a non cmake issue on all versions)
# we should stick with the the minor duplication
if(USE_CONAN_PACKAGER)
    set(MAVEN_ARG1 "-q")
    set(MAVEN_ARG2 "package")
    set(MAVEN_ARG3 "-Denforcer.skip=true")
    set(JNI_ASSEMBLY_WORKING_DIR "${JNI_ASSEMBLY_JAR_BIN}/nifi-minifi-jni-assembly")

    message("-----Prep Conan Py Maven Command: ${PY_MAVEN_EXECUTABLE}")
    message("-----Passed Maven Args to Py Script: ${MAVEN_ARG1} ${MAVEN_ARG2} ${MAVEN_ARG3}")
    message("-----Passed Working Dir to Py Script: ${WORKING_DIR}")
    message("-----Execute Py Maven Script to build Apache NiFi JNI Assembly")
    if (WIN32)
        execute_process(COMMAND cmd /c "${PY_MAVEN_EXECUTABLE}" 
            --working_directory=${JNI_ASSEMBLY_WORKING_DIR}
            --maven_arg1=${MAVEN_ARG1}
            --maven_arg2=${MAVEN_ARG2}
            --maven_arg3=${MAVEN_ARG3}
            RESULT_VARIABLE mvn_result
            OUTPUT_VARIABLE mvn_output
            ERROR_VARIABLE mvn_error
        )
    else()
        execute_process(COMMAND ${PY_MAVEN_EXECUTABLE} 
            --working_directory=${JNI_ASSEMBLY_WORKING_DIR}
            --maven_arg1=${MAVEN_ARG1}
            --maven_arg2=${MAVEN_ARG2}
            --maven_arg3=${MAVEN_ARG3}
            RESULT_VARIABLE mvn_result
            OUTPUT_VARIABLE mvn_output
            ERROR_VARIABLE mvn_error
        )
    endif()

else()

    if (WIN32)
        execute_process(COMMAND cmd /c "${MAVEN_EXECUTABLE}" "-q" "package" "-Denforcer.skip=true"
            WORKING_DIRECTORY "${JNI_ASSEMBLY_JAR_BIN}/nifi-minifi-jni-assembly"
            RESULT_VARIABLE mvn_result
            OUTPUT_VARIABLE mvn_output
            ERROR_VARIABLE mvn_error)
    else()
        execute_process(COMMAND "${MAVEN_EXECUTABLE}" "-q" "package" "-Denforcer.skip=true"
            WORKING_DIRECTORY "${JNI_ASSEMBLY_JAR_BIN}/nifi-minifi-jni-assembly"
            RESULT_VARIABLE mvn_result
            OUTPUT_VARIABLE mvn_output
            ERROR_VARIABLE mvn_error)
    endif()

endif()

if("${mvn_result}" STREQUAL "0")
    message("${mvn_output}")

    SET (JNI-ASSEMBLY-JAR-DIR "${JNI_ASSEMBLY_JAR_BIN}/nifi-minifi-jni-assembly/target/nifi-minifi-jni-minifi-jni/nifi-minifi-jni-1.9.0/minifi-jni")
    message("....Successfully Produced ${JNI-ASSEMBLY-JAR-DIR}....")

    install(DIRECTORY "${JNI-ASSEMBLY-JAR-DIR}"
            DESTINATION .
            COMPONENT bin)

else()
    message("Maven failed ${mvn_result} ${mvn_output} ${mvn_error}")
endif()

message(STATUS "Building NiFi JNI Jar for packaging ....")

if(USE_CONAN_PACKAGER)
    set(MAVEN_ARG1 "-q")
    set(MAVEN_ARG2 "package")
    set(JNI_FRAMEWORK_WORKING_DIR "${JNI_FRAMEWORK_JAR_BIN}/nifi-framework-jni")

    message("-----Prep Conan Py Maven Command: ${PY_MAVEN_EXECUTABLE}")
    message("-----Passed Maven Args to Py Script: ${MAVEN_ARG1} ${MAVEN_ARG2} ${MAVEN_ARG3}")
    message("-----Passed Working Dir to Py Script: ${WORKING_DIR}")
    message("-----Execute Py Maven Script to build Apache NiFi JNI Framework")
    if (WIN32)
        execute_process(COMMAND cmd /c "${PY_MAVEN_EXECUTABLE}" 
            --working_directory=${JNI_FRAMEWORK_WORKING_DIR}
            --maven_arg1=${MAVEN_ARG1}
            --maven_arg2=${MAVEN_ARG2}
            RESULT_VARIABLE mvn_result
            OUTPUT_VARIABLE mvn_output
            ERROR_VARIABLE mvn_error
        )
    else()
        execute_process(COMMAND ${PY_MAVEN_EXECUTABLE} 
            --working_directory=${JNI_FRAMEWORK_WORKING_DIR}
            --maven_arg1=${MAVEN_ARG1}
            --maven_arg2=${MAVEN_ARG2}
            RESULT_VARIABLE mvn_result
            OUTPUT_VARIABLE mvn_output
            ERROR_VARIABLE mvn_error
        )
    endif()

else()

    if (WIN32)
        execute_process(COMMAND cmd /c "${MAVEN_EXECUTABLE}" "-q" "package"
            WORKING_DIRECTORY "${JNI_FRAMEWORK_JAR_BIN}/nifi-framework-jni"
            RESULT_VARIABLE mvn_result
            OUTPUT_VARIABLE mvn_output
            ERROR_VARIABLE mvn_error)
    else()
        execute_process(COMMAND "${MAVEN_EXECUTABLE}" "-q" "package"
            WORKING_DIRECTORY "${JNI_FRAMEWORK_JAR_BIN}/nifi-framework-jni"
            RESULT_VARIABLE mvn_result
            OUTPUT_VARIABLE mvn_output
            ERROR_VARIABLE mvn_error)
    endif()

endif()
if("${mvn_result}" STREQUAL "0")
    message("${mvn_output}")
    SET (JNI-FRAMEWORK-JAR "${JNI_FRAMEWORK_JAR_BIN}/nifi-framework-jni/target/nifi-framework-jni-1.9.0.jar")
    message("....Successfully Produced ${JNI-FRAMEWORK-JAR}....")
    install(FILES ${JNI-FRAMEWORK-JAR}
        DESTINATION minifi-jni/lib
        COMPONENT bin)
else()
    message("Maven could not be invoked to build the framework jar")
endif()

target_link_libraries (minifi-jni ${LIBMINIFI})

if (APPLE)
    target_link_libraries (minifi-jni ${JAVA_JVM_LIBRARY})
elseif (WIN32)
    target_link_libraries (minifi-jni ${JAVA_JVM_LIBRARY})
    target_link_libraries (minifi-jni ${Java_LIBRARIES})
else()
    target_link_libraries (minifi-jni ${JAVA_JVM_LIBRARY})
endif()

register_extension(minifi-jni "JNI EXTENSIONS" JNI-EXTENSION "Enables JNI capabilities to support loading Java Classes.")
register_extension_linter(minifi-jni-linter)
