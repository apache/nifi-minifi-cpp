# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
from typing import ClassVar

from nifiapi.flowfilesource import FlowFileSource


class CreateNothing(FlowFileSource):
    class Java:
        implements: ClassVar[list] = ["org.apache.nifi.python.processor.FlowFileSource"]

    class ProcessorDetails:
        version = "0.0.1-SNAPSHOT"
        description = """A Python processor for testing a use-case where the Source processor
                         does not create any output."""
        tags: ClassVar[list] = ["test", "python", "source"]

    def __init__(self, **kwargs):
        pass

    def create(self, context):
        context.yield_resources()
