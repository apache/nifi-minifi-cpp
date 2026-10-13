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

FROM lordgamez/open62541:1.5.2
RUN apk add --no-cache build-base
COPY file_server.cpp /opt/file_server/
RUN g++ -std=c++23 -Wall -Wextra -O2 -isystem /usr/local/include/open62541 \
        -o /opt/file_server/file_server /opt/file_server/file_server.cpp -lopen62541 && \
    mkdir -p /data
CMD ["/opt/file_server/file_server", "/data"]
