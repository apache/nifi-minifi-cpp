# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


file(GLOB MQTT_TESTS  "*.cpp")

FOREACH(testfile ${MQTT_TESTS})
	get_filename_component(testfilename "${testfile}" NAME_WE)
	add_minifi_executable("${testfilename}" "${testfile}")
	target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_CURRENT_SOURCE_DIR}/../../extensions/standard-processors")
	target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_CURRENT_SOURCE_DIR}/../../../libminifi/test/")
	target_compile_definitions(${testfilename} PRIVATE "MINIFI_EXTENSION_GROUP_NAME=org.apache.nifi.minifi.test")
	createTests("${testfilename}")
	target_link_libraries(${testfilename} Catch2WithMain)
	target_link_libraries(${testfilename} minifi-mqtt-extensions)
	target_link_libraries(${testfilename} minifi-standard-processors)
	add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
	set_tests_properties("${testfilename}" PROPERTIES LABELS "mqtt")
ENDFOREACH()

list(LENGTH MQTT_TESTS TEST_COUNT)
message("-- Finished building ${TEST_COUNT} MQTT related test file(s)...")
