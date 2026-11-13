# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if (MINIFI_PACKAGING_TYPE STREQUAL "RPM")
    string(REPLACE ";" "\n" RPM_EXPECTED_EXTENSION_LIST "${RPM_EXPECTED_EXTENSION_LIST}")
    configure_file(rpm/expected-rpm-contents.in ${CMAKE_BINARY_DIR}/expected-rpm-contents.txt @ONLY)
endif()
