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

# Create a custom build target called "docker" that will invoke DockerBuild.sh and create the NiFi-MiNiFi-CPP Docker image
add_custom_target(
    docker
    COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerBuild.sh 1000 1000 ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH} minificppsource ${CMAKE_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docker/)

add_custom_target(
    docker-verify
    COMMAND ${CMAKE_SOURCE_DIR}/docker/DockerVerify.sh)
