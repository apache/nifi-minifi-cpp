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


import uuid
import json

from ..core.Processor import Processor
from ..core.InputPort import InputPort
from ..core.OutputPort import OutputPort
from ..core.Funnel import Funnel


class Minifi_flow_json_serializer:
    def serialize(self, start_nodes, controllers, parameter_context_name: str, parameter_contexts):
        res = {
            'parameterContexts': [],
            'rootGroup': {
                'name': 'MiNiFi Flow',
                'processors': [],
                'funnels': [],
                'connections': [],
                'remoteProcessGroups': [],
                'controllerServices': [],
                'inputPorts': [],
                'outputPorts': []
            }
        }
        visited = []

        if parameter_context_name:
            res['rootGroup']['parameterContextName'] = parameter_context_name

        if parameter_contexts:
            for context_name in parameter_contexts:
                res['parameterContexts'].append({
                    'identifier': str(uuid.uuid4()),
                    'name': context_name,
                    'parameters': []
                })
                for parameter in parameter_contexts[context_name]:
                    res['parameterContexts'][-1]['parameters'].append({
                        'name': parameter.name,
                        'description': '',
                        'sensitive': False,
                        'value': parameter.value
                    })

        for node in start_nodes:
            self.serialize_node(node, res['rootGroup'], visited)

        for controller in controllers:
            self.serialize_controller(controller, res['rootGroup'])

        return json.dumps(res)

    def serialize_node(self, connectable, root, visited):
        visited.append(connectable)

        if hasattr(connectable, 'name'):
            connectable_name = connectable.name
        else:
            connectable_name = str(connectable.uuid)

        if isinstance(connectable, InputPort):
            group = connectable.remote_process_group
            if group is None:
                root['inputPorts'].append({
                    'name': connectable_name,
                    'identifier': str(connectable.instance_id),
                    'properties': connectable.properties
                })
            else:
                res_group = None

                for res_group_candidate in root['remoteProcessGroups']:
                    assert isinstance(res_group_candidate, dict)
                    if res_group_candidate['identifier'] == str(group.uuid):
                        res_group = res_group_candidate

                if res_group is None:
                    res_group = {
                        'name': group.name,
                        'identifier': str(group.uuid),
                        'targetUri': group.url,
                        'communicationsTimeout': '30 sec',
                        'yieldDuration': '3 sec',
                        'transportProtocol': group.transport_protocol,
                        'inputPorts': []
                    }

                    root['remoteProcessGroups'].append(res_group)

                res_group['inputPorts'].append({
                    'identifier': str(connectable.instance_id),
                    'name': connectable.name,
                    'properties': connectable.properties
                })

        if isinstance(connectable, OutputPort):
            group = connectable.remote_process_group
            if group is None:
                root['outputPorts'].append({
                    'name': connectable_name,
                    'identifier': str(connectable.instance_id),
                    'properties': connectable.properties
                })
            else:
                res_group = None

                for res_group_candidate in root['remoteProcessGroups']:
                    assert isinstance(res_group_candidate, dict)
                    if res_group_candidate['identifier'] == str(group.uuid):
                        res_group = res_group_candidate

                if res_group is None:
                    res_group = {
                        'name': group.name,
                        'identifier': str(group.uuid),
                        'targetUri': group.url,
                        'communicationsTimeout': '30 sec',
                        'yieldDuration': '3 sec',
                        'outputPorts': []
                    }

                    root['remoteProcessGroups'].append(res_group)

                res_group['outputPorts'].append({
                    'identifier': str(connectable.instance_id),
                    'name': connectable.name,
                    'properties': connectable.properties
                })

        if isinstance(connectable, Processor):
            root['processors'].append({
                'name': connectable_name,
                'identifier': str(connectable.uuid),
                'type': connectable.class_prefix + connectable.clazz,
                'schedulingStrategy': connectable.schedule['scheduling strategy'],
                'schedulingPeriod': connectable.schedule['scheduling period'],
                'penaltyDuration': connectable.schedule['penalization period'],
                'yieldDuration': connectable.schedule['yield period'],
                'runDurationMillis': connectable.schedule['run duration nanos'],
                'properties': connectable.properties,
                'autoTerminatedRelationships': connectable.auto_terminate,
                'concurrentlySchedulableTaskCount': connectable.max_concurrent_tasks
            })

            for svc in connectable.controller_services:
                if svc in visited:
                    continue

                visited.append(svc)
                self.serialize_controller(svc, root)

        if isinstance(connectable, Funnel):
            root['funnels'].append({
                'identifier': str(connectable.uuid)
            })

        for conn_name in connectable.connections:
            conn_procs = connectable.connections[conn_name]

            if not isinstance(conn_procs, list):
                conn_procs = [conn_procs]

            for proc in conn_procs:
                root['connections'].append({
                    'name': str(uuid.uuid4()),
                    'source': {'id': str(connectable.uuid) if not isinstance(connectable, InputPort) and not isinstance(connectable, OutputPort) else str(connectable.instance_id)},
                    'destination': {'id': str(proc.uuid) if not isinstance(proc, InputPort) and not isinstance(proc, OutputPort) else str(proc.instance_id)}
                })
                if (all(str(connectable.uuid) != x['identifier'] for x in root['funnels'])):
                    root['connections'][-1]['selectedRelationships'] = [conn_name]
                if proc not in visited:
                    self.serialize_node(proc, root, visited)

    def serialize_controller(self, controller, root):
        if hasattr(controller, 'name'):
            connectable_name = controller.name
        else:
            connectable_name = str(controller.uuid)

        root['controllerServices'].append({
            'name': connectable_name,
            'identifier': controller.id,
            'type': controller.service_class,
            'properties': controller.properties
        })

        if controller.linked_services:
            if len(controller.linked_services) == 1:
                root['controllerServices'][-1]['properties']['Linked Services'] = controller.linked_services[0].name
            else:
                root['controllerServices'][-1]['properties']['Linked Services'] = [{"value": service.name} for service in controller.linked_services]
