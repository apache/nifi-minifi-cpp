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

FetchContent_Declare(gsl-lite
    URL      https://github.com/gsl-lite/gsl-lite/archive/refs/tags/v0.39.0.tar.gz
    URL_HASH SHA256=f80ec07d9f4946097a1e2554e19cee4b55b70b45d59e03a7d2b7f80d71e467e9
    SYSTEM
)
FetchContent_MakeAvailable(gsl-lite)
