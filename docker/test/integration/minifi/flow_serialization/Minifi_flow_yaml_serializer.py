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
import yaml

from ..core.Processor import Processor
from ..core.InputPort import InputPort
from ..core.Funnel import Funnel


class Minifi_flow_yaml_serializer:
    def serialize(self, start_nodes, controllers):
        res = None
        visited = None

        for node in start_nodes:
            res, visited = self.serialize_node(node, res, visited)

        for controller in controllers:
            res = self.serialize_controller(controller, res)

        return yaml.dump(res, default_flow_style=False)

    def serialize_node(self, connectable, root=None, visited=None):
        if visited is None:
            visited = []

        if root is None:
            res = {
                'Flow Controller': {
                    'name': 'MiNiFi Flow'
                },
                'Processors': [],
                'Funnels': [],
                'Connections': [],
                'Remote Processing Groups': [],
                'Controller Services': []
            }
        else:
            res = root

        visited.append(connectable)

        if hasattr(connectable, 'name'):
            connectable_name = connectable.name
        else:
            connectable_name = str(connectable.uuid)

        if isinstance(connectable, InputPort):
            group = connectable.remote_process_group
            res_group = None

            for res_group_candidate in res['Remote Processing Groups']:
                assert isinstance(res_group_candidate, dict)
                if res_group_candidate['id'] == str(group.uuid):
                    res_group = res_group_candidate

            if res_group is None:
                res_group = {
                    'name': group.name,
                    'id': str(group.uuid),
                    'url': group.url,
                    'timeout': '30 sec',
                    'yield period': '3 sec',
                    'Input Ports': []
                }

                res['Remote Processing Groups'].append(res_group)

            res_group['Input Ports'].append({
                'id': str(connectable.uuid),
                'name': connectable.name,
                'max concurrent tasks': 1,
                'Properties': {}
            })

        if isinstance(connectable, Processor):
            res['Processors'].append({
                'name': connectable_name,
                'id': str(connectable.uuid),
                'class': connectable.class_prefix + connectable.clazz,
                'scheduling strategy': connectable.schedule['scheduling strategy'],
                'scheduling period': connectable.schedule['scheduling period'],
                'penalization period': connectable.schedule['penalization period'],
                'yield period': connectable.schedule['yield period'],
                'run duration nanos': connectable.schedule['run duration nanos'],
                'Properties': connectable.properties,
                'auto-terminated relationships list': connectable.auto_terminate,
                'max concurrent tasks': connectable.max_concurrent_tasks
            })

            for svc in connectable.controller_services:
                if svc in visited:
                    continue

                visited.append(svc)
                res['Controller Services'].append({
                    'name': svc.name,
                    'id': svc.id,
                    'class': svc.service_class,
                    'Properties': svc.properties
                })

        if isinstance(connectable, Funnel):
            res['Funnels'].append({
                'id': str(connectable.uuid)
            })

        for conn_name in connectable.connections:
            conn_procs = connectable.connections[conn_name]

            if isinstance(conn_procs, list):
                for proc in conn_procs:
                    res['Connections'].append({
                        'name': str(uuid.uuid4()),
                        'source id': str(connectable.uuid),
                        'destination id': str(proc.uuid),
                        'drop empty': ("true" if proc.drop_empty_flowfiles else "false")
                    })
                    if (all(str(connectable.uuid) != x['id'] for x in res['Funnels'])):
                        res['Connections'][-1]['source relationship name'] = conn_name
                    if proc not in visited:
                        self.serialize_node(proc, res, visited)
            else:
                res['Connections'].append({
                    'name': str(uuid.uuid4()),
                    'source id': str(connectable.uuid),
                    'destination id': str(conn_procs.uuid),
                    'drop empty': ("true" if proc.drop_empty_flowfiles else "false")
                })
                if (all(str(connectable.uuid) != x['id'] for x in res['Funnels'])):
                    res['Connections'][-1]['source relationship name'] = conn_name
                if conn_procs not in visited:
                    self.serialize_node(conn_procs, res, visited)

        return (res, visited)

    def serialize_controller(self, controller, root=None):
        if root is None:
            res = {
                'Flow Controller': {
                    'name': 'MiNiFi Flow'
                },
                'Processors': [],
                'Funnels': [],
                'Connections': [],
                'Remote Processing Groups': [],
                'Controller Services': []
            }
        else:
            res = root

        if hasattr(controller, 'name'):
            connectable_name = controller.name
        else:
            connectable_name = str(controller.uuid)

        res['Controller Services'].append({
            'name': connectable_name,
            'id': controller.id,
            'class': controller.service_class,
            'Properties': controller.properties
        })

        return res
