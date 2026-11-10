# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

if(MINIFI_PUGIXML_SOURCE STREQUAL "CONAN")
    message("Using Conan to install PugiXML")
    find_package(PugiXML REQUIRED)
elseif(MINIFI_PUGIXML_SOURCE STREQUAL "BUILD")
    message("Using CMake to build PugiXML from source")
    include(PugiXml)
endif()
