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

find_package(Python 3.6 REQUIRED COMPONENTS Development Interpreter)

if(WIN32)
  set(Python_LIBRARIES ${Python_LIBRARY_DIRS}/python3.lib)
else()
  find_library(generic_lib_python NAMES libpython3.so)
  if (NOT generic_lib_python STREQUAL generic_lib_python-NOTFOUND)
    message(VERBOSE "Using generic python library at " ${generic_lib_python})
    set(Python_LIBRARIES ${generic_lib_python})
  endif()
endif()
