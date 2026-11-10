# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

find_package(Python 3.10 REQUIRED COMPONENTS Development Interpreter)

if(WIN32)
  set(Python_LIBRARIES ${Python_LIBRARY_DIRS}/python3.lib)
else()
  find_library(generic_lib_python NAMES libpython3.so)
  if (NOT generic_lib_python STREQUAL generic_lib_python-NOTFOUND)
    message(VERBOSE "Using generic python library at " ${generic_lib_python})
    set(Python_LIBRARIES ${generic_lib_python})
  endif()
endif()
