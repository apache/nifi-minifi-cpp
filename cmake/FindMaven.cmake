# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

set(MAVEN_HINT_UNIX /usr/bin CACHE STRING "maven default directory")

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
