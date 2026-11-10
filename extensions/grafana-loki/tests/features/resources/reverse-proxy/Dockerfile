# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

FROM nginx:1.29.4

COPY nginx.conf.template /nginx.conf.template
COPY run.sh /run.sh

RUN apt update -y && apt install -y apache2-utils && rm /etc/nginx/conf.d/default.conf && chmod 0755 /run.sh

ENTRYPOINT [ "/run.sh" ]
