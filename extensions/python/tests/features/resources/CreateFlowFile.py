# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

from typing import ClassVar

from nifiapi.flowfilesource import FlowFileSource, FlowFileSourceResult
from nifiapi.properties import PropertyDescriptor
from nifiapi.relationship import Relationship


class CreateFlowFile(FlowFileSource):
    class Java:
        implements: ClassVar[list] = ["org.apache.nifi.python.processor.FlowFileSource"]

    class ProcessorDetails:
        version = "2.0.0-snapshot"
        description = """Test Python source processor."""
        tags: ClassVar[list] = ["text", "test", "python", "source"]

    FF_CONTENTS = PropertyDescriptor(
        name="FlowFile Contents",
        description="""The contents of the FlowFile.""",
        required=True,
        default_value="Hello World!",
    )

    property_descriptors: ClassVar[list] = [FF_CONTENTS]

    REL_MULTILINE = Relationship(name="space", description="FlowFiles that contain space characters.")

    def __init__(self, **kwargs):
        pass

    def getPropertyDescriptors(self):
        return self.property_descriptors

    def getRelationships(self):
        return [self.REL_MULTILINE]

    def create(self, context):
        contents = context.getProperty(self.FF_CONTENTS).getValue()

        if contents is not None and isinstance(contents, str):
            contents_str = str.encode(contents)
            if b" " in contents_str:
                return FlowFileSourceResult(
                    relationship="space",
                    attributes={"type": "space"},
                    contents=contents_str,
                )

        return FlowFileSourceResult(relationship="success", attributes={"type": "non-space"}, contents=contents)
