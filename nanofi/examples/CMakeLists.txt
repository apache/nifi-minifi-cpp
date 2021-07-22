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

include(CppVersion)
set_cpp_version()

if (NOT WIN32)
    add_executable(generate_flow generate_flow.c)

    add_executable(terminate_handler terminate_handler.c)

    target_link_libraries(generate_flow minifi-http-curl)
    target_link_libraries(generate_flow nanofi Threads::Threads)

    target_link_libraries(terminate_handler nanofi Threads::Threads)

    #add_executable(hash_file hash_file.c)

    #arget_link_libraries(hash_file nanofi Threads::Threads )

    add_executable(transmit_flow transmit_flow.c)

    target_link_libraries(transmit_flow minifi-http-curl)
    target_link_libraries(transmit_flow nanofi Threads::Threads)

    add_executable(transmit_payload transmit_payload.c)

    target_link_libraries(transmit_payload minifi-http-curl)
    target_link_libraries(transmit_payload nanofi Threads::Threads)

    add_executable(monitor_directory monitor_directory.c)

    target_link_libraries(monitor_directory minifi-http-curl)
    target_link_libraries(monitor_directory nanofi Threads::Threads)
endif()
