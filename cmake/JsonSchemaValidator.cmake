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

include(FetchContent)

FetchContent_Declare(json-schema-validator
    URL https://github.com/pboettch/json-schema-validator/archive/refs/tags/2.4.0.tar.gz
    URL_HASH SHA256=24cbb114609cc9b43d4018b8d03e082ff5d2f26f5dce8bd36538097267b63af9
    SYSTEM)

FetchContent_MakeAvailable(json-schema-validator)

target_compile_options(nlohmann_json_schema_validator PRIVATE -fsigned-char)
