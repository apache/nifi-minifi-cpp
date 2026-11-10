#
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#


class File:
    def __init__(self, path, content: str | bytes, mode="rw", permissions=None):
        self.path = path
        self.content = content
        self.mode = mode
        self.permissions = permissions
