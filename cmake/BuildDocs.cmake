# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
