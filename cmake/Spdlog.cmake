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
include(FetchContent)
include(fmt)
set(SPDLOG_FMT_EXTERNAL ON CACHE STRING "" FORCE)
FetchContent_Declare(Spdlog
        URL  https://github.com/gabime/spdlog/archive/refs/tags/v1.12.0.tar.gz
        URL_HASH SHA256=4dccf2d10f410c1e2feaff89966bfc49a1abb29ef6f08246335b110e001e09a9
        OVERRIDE_FIND_PACKAGE
        )
FetchContent_MakeAvailable(Spdlog)
