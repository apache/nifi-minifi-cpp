#  Licensed to the Apache Software Foundation (ASF) under one or more
#  contributor license agreements.  See the NOTICE file distributed with
#  this work for additional information regarding copyright ownership.
#  The ASF licenses this file to You under the Apache License, Version 2.0
#  (the "License"); you may not use this file except in compliance with
#  the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

from nifiapi.componentstate import Scope
from nifiapi.flowfilesource import FlowFileSource, FlowFileSourceResult


class TestStateManager(FlowFileSource):
    class Java:
        implements = ['org.apache.nifi.python.processor.FlowFileSource']

    class ProcessorDetails:
        version = '0.0.1-SNAPSHOT'
        description = '''A Python source processor that uses StateManager.'''
        tags = ['text', 'test', 'python', 'source']

    def __init__(self, **kwargs):
        pass

    def getPropertyDescriptors(self):
        return []

    def create(self, context):
        state_manager = context.getStateManager()
        state = state_manager.getState(Scope.CLUSTER)
        old_value = state.get("state_key")
        if not old_value:
            new_state = {'state_key': '1'}
            state_manager.setState(new_state, Scope.CLUSTER)
        elif old_value == '1':
            new_state = {'state_key': '2'}
            state_manager.replace(state, new_state, Scope.CLUSTER)
        else:
            state_manager.clear(Scope.CLUSTER)

        return FlowFileSourceResult(relationship='success', attributes=state.toMap(), contents='Output FlowFile Contents')
