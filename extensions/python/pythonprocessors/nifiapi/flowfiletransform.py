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
from abc import abstractmethod
from minifi_native import ProcessContext, ProcessSession
from .processorbase import ProcessorBase, WriteCallback
from .properties import FlowFile as FlowFileProxy
from .properties import ProcessContext as ProcessContextProxy


class FlowFileTransformResult:
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


class FlowFileTransform(ProcessorBase):
    def onTrigger(self, context: ProcessContext, session: ProcessSession):
        original_flow_file = session.get()
        if not original_flow_file:
            return

        flow_file = session.clone(original_flow_file)

        flow_file_proxy = FlowFileProxy(session, flow_file)
        context_proxy = ProcessContextProxy(context, self)
        try:
            result = self.transform(context_proxy, flow_file_proxy)
        except Exception:
            self.logger.error("Failed to transform flow file due to error:\n{}".format(traceback.format_exc()))
            session.remove(flow_file)
            session.transfer(original_flow_file, self.REL_FAILURE)
            return

        if result.getRelationship() == "original":
            session.remove(flow_file)
            self.logger.error("Result relationship cannot be 'original', it is reserved for the original flow file, and transferred automatically in non-failure cases.")
            session.transfer(original_flow_file, self.REL_FAILURE)
            return

        result_attributes = result.getAttributes()
        if result.getRelationship() == "failure":
            session.remove(flow_file)
            if result_attributes is not None:
                for name, value in result_attributes.items():
                    original_flow_file.setAttribute(name, value)
            if result.getContents() is not None:
                self.logger.error("'failure' relationship should not have content, the original flow file will be transferred automatically in this case.")
            session.transfer(original_flow_file, self.REL_FAILURE)
            return

        if result_attributes is not None:
            for name, value in result_attributes.items():
                flow_file.setAttribute(name, value)

        result_content = result.getContents()
        if result_content is not None:
            session.write(flow_file, WriteCallback(result_content))

        if result.getRelationship() == "success":
            session.transfer(flow_file, self.REL_SUCCESS)
        else:
            session.transferToCustomRelationship(flow_file, result.getRelationship())
        session.transfer(original_flow_file, self.REL_ORIGINAL)

    @abstractmethod
    def transform(self, context: ProcessContextProxy, flowFile: FlowFileProxy) -> FlowFileTransformResult:
        pass
