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

import yaml

from .flow_definition import FlowDefinition


class MinifiFlowDefinition(FlowDefinition):
    def __init__(self, flow_name: str = "MiNiFi Flow"):
        super().__init__(flow_name)

    def to_yaml(self) -> str:
        """Serializes the entire flow definition into the MiNiFi YAML format."""

        # Create a quick lookup map of processor names to their objects
        # This is crucial for finding the source/destination IDs for connections
        processors_by_name = {p.name: p for p in self.processors}
        funnels_by_name = {f.name: f for f in self.funnels}
        remote_input_ports_by_name = {port.name: port for rpg in self.remote_process_groups for port in rpg.input_ports}
        remote_output_ports_by_name = {port.name: port for rpg in self.remote_process_groups for port in rpg.output_ports}

        connectables_by_name = {**processors_by_name, **funnels_by_name, **remote_input_ports_by_name, **remote_output_ports_by_name}

        if len(self.parameter_contexts) > 0:
            parameter_context_name = self.parameter_contexts[0].name
        else:
            parameter_context_name = ''
        # Build the final dictionary structure
        config = {'MiNiFi Config Version': 3, 'Flow Controller': {'name': self.flow_name},
                  'Parameter Contexts': [p.to_yaml_dict() for p in self.parameter_contexts],
                  'Processors': [p.to_yaml_dict() for p in self.processors],
                  'Funnels': [f.to_yaml_dict() for f in self.funnels], 'Connections': [],
                  'Controller Services': [c.to_yaml_dict() for c in self.controller_services],
                  'Remote Processing Groups': [rpg.to_yaml_dict() for rpg in self.remote_process_groups],
                  'Parameter Context Name': parameter_context_name}

        # Build the connections list by looking up processor IDs
        for conn in self.connections:
            source_proc = connectables_by_name.get(conn.source_name)
            dest_proc = connectables_by_name.get(conn.target_name)

            if not source_proc or not dest_proc:
                raise ValueError(
                    f"Could not find processors for connection from '{conn.source_name}' to '{conn.target_name}'")

            config['Connections'].append(
                {'name': f"{conn.source_name}/{conn.source_relationship}/{conn.target_name}", 'id': conn.id,
                 'source id': source_proc.id, 'source relationship name': conn.source_relationship,
                 'destination id': dest_proc.id, "drop empty": conn.drop_empty_flowfiles})

        return yaml.dump(config, sort_keys=False, indent=2, width=120)
