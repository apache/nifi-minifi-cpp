#
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#


class Directory:
    def __init__(self, path, files: dict[str, str | bytes] | None = None, mode="rw"):
        self.path = path
        self.files: dict[str, str] = {}
        if files is not None:
            self.files = files
        self.mode = mode

    def add_file(self, file_name: str, content: str | bytes):
        self.files[file_name] = content
