# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


include(GetJsonSchemaValidator)

file(GLOB SCHEMA_TESTS  "*.cpp")
SET(SCHEMA_TEST_COUNT 0)
FOREACH(testfile ${SCHEMA_TESTS})
  get_filename_component(testfilename "${testfile}" NAME_WE)
  add_minifi_executable("${testfilename}" "${testfile}")
  createTests("${testfilename}")
  target_link_libraries(${testfilename} Catch2WithMain)
  target_link_libraries(${testfilename} minifi-standard-processors)
  target_link_libraries(${testfilename} nlohmann_json_schema_validator)
  MATH(EXPR SCHEMA_TEST_COUNT "${SCHEMA_TEST_COUNT}+1")
  add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${TEST_DIR})
ENDFOREACH()
message("-- Finished building ${SCHEMA_TEST_COUNT} Json Schema related test file(s)...")
