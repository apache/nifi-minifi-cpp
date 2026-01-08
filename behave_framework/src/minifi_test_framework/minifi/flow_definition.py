#
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
#

from abc import ABC

from .connection import Connection
from .controller_service import ControllerService
from .funnel import Funnel
from .parameter_context import ParameterContext
from .processor import Processor


class FlowDefinition(ABC):
    def __init__(self, flow_name: str = "MiNiFi Flow"):
        self.flow_name = flow_name
        self.processors: list[Processor] = []
        self.controller_services: list[ControllerService] = []
        self.funnels: list[Funnel] = []
        self.connections: list[Connection] = []
        self.parameter_contexts: list[ParameterContext] = []

    def add_processor(self, processor: Processor):
        self.processors.append(processor)

    def get_processor(self, processor_name: str) -> Processor | None:
        return next((proc for proc in self.processors if proc.name == processor_name), None)

    def get_controller_service(self, controller_service_name: str) -> ControllerService | None:
        return next((controller for controller in self.controller_services if controller.name == controller_service_name), None)

    def get_parameter_context(self, parameter_context_name: str) -> ParameterContext | None:
        return next((parameter_context for parameter_context in self.parameter_contexts if
                     parameter_context.name == parameter_context_name), None)

    def add_funnel(self, funnel: Funnel):
        self.funnels.append(funnel)

    def add_connection(self, connection: Connection):
        self.connections.append(connection)

    def to_yaml(self) -> str:
        raise NotImplementedError("to_yaml method must be implemented in subclasses")

    def to_json(self) -> str:
        raise NotImplementedError("to_json method must be implemented in subclasses")

    def __repr__(self):
        return f"FlowDefinition(Processors: {self.processors}, Controller Services: {self.controller_services})"
