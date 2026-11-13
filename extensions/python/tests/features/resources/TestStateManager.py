# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

from typing import ClassVar

from nifiapi.componentstate import Scope
from nifiapi.flowfilesource import FlowFileSource, FlowFileSourceResult


class TestStateManager(FlowFileSource):
    class Java:
        implements: ClassVar[list] = ["org.apache.nifi.python.processor.FlowFileSource"]

    class ProcessorDetails:
        version = "0.0.1-SNAPSHOT"
        description = """A Python source processor that uses StateManager."""
        tags: ClassVar[list] = ["text", "test", "python", "source"]

    def __init__(self, **kwargs):
        pass

    def getPropertyDescriptors(self):
        return []

    def create(self, context):
        state_manager = context.getStateManager()
        state = state_manager.getState(Scope.CLUSTER)
        old_value = state.get("state_key")
        if not old_value:
            new_state = {"state_key": "1"}
            state_manager.setState(new_state, Scope.CLUSTER)
        elif old_value == "1":
            new_state = {"state_key": "2"}
            state_manager.replace(state, new_state, Scope.CLUSTER)
        else:
            state_manager.clear(Scope.CLUSTER)

        return FlowFileSourceResult(
            relationship="success",
            attributes=state.toMap(),
            contents="Output FlowFile Contents",
        )
