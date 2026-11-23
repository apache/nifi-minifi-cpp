# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

FROM lordgamez/open62541:1.5.2
RUN apk add --no-cache build-base
COPY file_server.cpp /opt/file_server/
RUN g++ -std=c++23 -Wall -Wextra -O2 -isystem /usr/local/include/open62541 \
        -o /opt/file_server/file_server /opt/file_server/file_server.cpp -lopen62541 && \
    mkdir -p /data
CMD ["/opt/file_server/file_server", "/data"]
