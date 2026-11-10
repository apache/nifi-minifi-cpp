#
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#

import uuid


class Connection:
    def __init__(self, source_name: str, source_relationship: str, target_name: str):
        self.id: str = str(uuid.uuid4())
        self.source_name: str = source_name
        self.source_relationship: str = source_relationship
        self.target_name: str = target_name
        self.drop_empty_flowfiles: bool = False

    def __repr__(self):
        return f"({self.source_name}:{self.source_relationship} --> {self.target_name})"
