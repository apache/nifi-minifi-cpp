# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

from nifiapi.flowfiletransform import FlowFileTransform, FlowFileTransformResult


class RotatingForwarder(FlowFileTransform):
    """
    Forwards flow files to a different relationship each time it is called in a round robin manner.
    """

    def __init__(self, **kwargs):
        self.relationship_index = 0
        self.relationships = ["first", "second", "third", "fourth"]

    def transform(self, context, flowFile):
        content = flowFile.getContentsAsBytes().decode()

        relationship = self.relationships[self.relationship_index % len(self.relationships)]
        self.relationship_index += 1
        self.relationship_index %= len(self.relationships)
        return FlowFileTransformResult(relationship, contents=content)
