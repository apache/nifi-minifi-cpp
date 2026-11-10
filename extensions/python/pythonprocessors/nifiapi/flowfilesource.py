# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

import traceback
from abc import abstractmethod

from minifi_native import FlowFile, ProcessContext, ProcessSession

from .processorbase import ProcessorBase, WriteCallback
from .properties import ProcessContext as ProcessContextProxy


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
            self.logger.error(f"Failed to create flow file due to error:\n{traceback.format_exc()}")
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
