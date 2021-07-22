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

cmake_minimum_required(VERSION 3.16)


if (NOT WIN32)
add_executable(log_aggregator log_aggregator.c)
target_link_libraries(log_aggregator nanofi Threads::Threads)
target_link_libraries(log_aggregator minifi-http-curl)

add_executable(tailfile_chunk tailfile_chunk.c)
target_link_libraries(tailfile_chunk nanofi Threads::Threads)
target_link_libraries(tailfile_chunk minifi-http-curl)

add_executable(tailfile_delimited tailfile_delimited.c)
target_link_libraries(tailfile_delimited nanofi Threads::Threads)
target_link_libraries(tailfile_delimited minifi-http-curl)
endif()
