# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

FROM node:lts-alpine
WORKDIR /app
RUN npm i kinesis-local
RUN npm i @aws-sdk/client-kinesis @aws-sdk/node-http-handler
COPY server.json ./
COPY consumer ./consumer

ENTRYPOINT npx kinesis-local
