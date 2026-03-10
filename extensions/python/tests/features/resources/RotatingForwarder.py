# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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
