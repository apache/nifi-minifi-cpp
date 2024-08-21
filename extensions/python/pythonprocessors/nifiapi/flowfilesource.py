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

import traceback
from .properties import ProcessContext as ProcessContextProxy
from abc import abstractmethod
from minifi_native import ProcessContext, ProcessSession, FlowFile
from .processorbase import ProcessorBase, WriteCallback


class FlowFileSourceResult:
    def __init__(self, relationship: str, attributes=None, contents=None):
        self.relationship = relationship
        self.attributes = attributes
        if contents is not None and isinstance(contents, str):
            self.contents = str.encode(contents)
        else:
            self.contents = contents

    def getRelationship(self):
        return self.relationship

    def getContents(self):
        return self.contents

    def getAttributes(self):
        return self.attributes


class FlowFileSource(ProcessorBase):
    def onTrigger(self, context: ProcessContext, session: ProcessSession):
        context_proxy = ProcessContextProxy(context, self)
        try:
            result = self.create(context_proxy)
            if not result:
                return
        except Exception:
            self.logger.error("Failed to create flow file due to error:\n{}".format(traceback.format_exc()))
            return

        flow_file = self.createFlowFile(session, result.getAttributes(), result.getContents())

        if result.getRelationship() == "success":
            session.transfer(flow_file, self.REL_SUCCESS)
        else:
            session.transferToCustomRelationship(flow_file, result.getRelationship())

    def createFlowFile(self, session: ProcessSession, attributes, contents) -> FlowFile:
        flow_file = session.create()
        if attributes:
            for key in attributes:
                flow_file.addAttribute(key, attributes[key])

        if contents:
            session.write(flow_file, WriteCallback(contents))

        return flow_file

    @abstractmethod
    def create(self, context):
        pass
