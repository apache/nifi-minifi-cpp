#
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#


class HostFile:
    def __init__(self, path, host_path, mode="ro"):
        self.container_path = path
        self.host_path = host_path
        self.mode = mode
