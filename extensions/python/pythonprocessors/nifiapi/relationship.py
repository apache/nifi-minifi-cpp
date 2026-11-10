# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


class Relationship:
    def __init__(self, name, description, auto_terminated=False):
        self.name = name
        self.description = description
        self.auto_terminated = auto_terminated
