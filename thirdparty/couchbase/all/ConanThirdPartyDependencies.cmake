# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

find_package(spdlog CONFIG QUIET)
find_package(fmt CONFIG QUIET)
find_package(Microsoft.GSL CONFIG QUIET)
find_package(Snappy CONFIG QUIET)
find_package(asio CONFIG QUIET)
find_package(hdr_histogram CONFIG QUIET)
find_package(llhttp CONFIG QUIET)
find_package(taocpp-json CONFIG QUIET)

include(cmake/OpenSSL.cmake)

add_library(jsonsl OBJECT ${PROJECT_SOURCE_DIR}/third_party/jsonsl/jsonsl.c)
set_target_properties(jsonsl PROPERTIES C_VISIBILITY_PRESET hidden POSITION_INDEPENDENT_CODE TRUE)
target_include_directories(jsonsl SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/third_party/jsonsl)
