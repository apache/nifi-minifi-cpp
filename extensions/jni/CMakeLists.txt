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

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

message(STATUS "JAVA_HOME: '$ENV{JAVA_HOME}'")
find_package(JNI REQUIRED)
find_package(Java REQUIRED)
find_package(Maven REQUIRED)
message(STATUS "MAVEN: ${MAVEN_EXECUTABLE}")
include_directories(${JNI_INCLUDE_DIRS})

file(GLOB SOURCES  "jvm/*.cpp" "*.cpp")

add_library(minifi-jni SHARED ${SOURCES})

set(JNI_FRAMEWORK_JAR_SOURCE "${CMAKE_SOURCE_DIR}/extensions/jni/nifi-framework-jni")
set(JNI_FRAMEWORK_JAR_BIN "${CMAKE_CURRENT_BINARY_DIR}/" )

file(COPY ${JNI_FRAMEWORK_JAR_SOURCE} DESTINATION ${JNI_FRAMEWORK_JAR_BIN})

set(JNI_ASSEMBLY_JAR_SOURCE "${CMAKE_SOURCE_DIR}/extensions/jni/nifi-minifi-jni-assembly")
set(JNI_ASSEMBLY_JAR_BIN "${CMAKE_CURRENT_BINARY_DIR}/" )

file(COPY ${JNI_FRAMEWORK_JAR_SOURCE} DESTINATION ${JNI_FRAMEWORK_JAR_BIN})
file(COPY ${JNI_ASSEMBLY_JAR_SOURCE} DESTINATION ${JNI_ASSEMBLY_JAR_BIN})

message("Building NiFi assembly for packaging ....")
# duplicating the execute_process because a version of cmake tested exhibited failures
# with an empty cmd before ${MAVEN_EXECUTABLE}. Until this is resolved ( or proven to be a non cmake issue on all versions)
# we should stick with the the minor duplication
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
if("${mvn_result}" STREQUAL "0")
	message("${mvn_output}")

	install(DIRECTORY "${JNI_ASSEMBLY_JAR_BIN}/nifi-minifi-jni-assembly/target/nifi-minifi-jni-minifi-jni/nifi-minifi-jni-1.9.0/minifi-jni"
        DESTINATION .
        COMPONENT bin)

else()
	message("Maven failed ${mvn_result} ${mvn_output} ${mvn_error}")
endif()

message("Building NiFi JNI Jar for packaging ....")

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
if("${mvn_result}" STREQUAL "0")
	message("${mvn_output}")
	SET (JNI-FRAMEWORK-JAR "${JNI_FRAMEWORK_JAR_BIN}/nifi-framework-jni/target/nifi-framework-jni-1.9.0.jar")
	message("Produced ${JNI-FRAMEWORK-JAR}")
	message("${mvn_output}")
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



SET (JNI-EXTENSION minifi-jni PARENT_SCOPE)
register_extension(minifi-jni)
register_extension_linter(minifi-jni-linter)
