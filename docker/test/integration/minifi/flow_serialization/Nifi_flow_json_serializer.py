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


class Nifi_flow_json_serializer:
    def serialize(self, start_nodes, nifi_version=None):
        res = {
            "encodingVersion": {
                "majorVersion": 2,
                "minorVersion": 0
            },
            "maxTimerDrivenThreadCount": 10,
            "maxEventDrivenThreadCount": 1,
            "registries": [],
            "parameterContexts": [],
            "parameterProviders": [],
            "controllerServices": [],
            "reportingTasks": [],
            "templates": [],
            "rootGroup": {
                "identifier": "9802c873-3322-3b60-a71d-732d02bd60f8",
                "instanceIdentifier": str(uuid.uuid4()),
                "name": "NiFi Flow",
                "comments": "",
                "position": {
                    "x": 0,
                    "y": 0
                },
                "processGroups": [],
                "remoteProcessGroups": [],
                "processors": [],
                "inputPorts": [],
                "outputPorts": [],
                "connections": [],
                "labels": [],
                "funnels": [],
                "controllerServices": [],
                "defaultFlowFileExpiration": "0 sec",
                "defaultBackPressureObjectThreshold": 10000,
                "defaultBackPressureDataSizeThreshold": "1 GB",
                "scheduledState": "RUNNING",
                "executionEngine": "INHERITED",
                "maxConcurrentTasks": 1,
                "statelessFlowTimeout": "1 min",
                "flowFileConcurrency": "UNBOUNDED",
                "flowFileOutboundPolicy": "STREAM_WHEN_AVAILABLE",
                "componentType": "PROCESS_GROUP"
            }
        }
        visited = []

        for node in start_nodes:
            self.serialize_node(node, nifi_version, res['rootGroup'], visited)

        return json.dumps(res)

    def serialize_node(self, connectable, nifi_version, root, visited):
        if visited is None:
            visited = []

        visited.append(connectable)

        if hasattr(connectable, 'name'):
            connectable_name_text = connectable.name
        else:
            connectable_name_text = str(connectable.uuid)

        if isinstance(connectable, InputPort):
            root['inputPorts'].append({
                'identifier': str(connectable.uuid),
                'instanceIdentifier': str(connectable.instance_id),
                'name': connectable_name_text,
                "comments": "",
                'position': {
                    'x': 0,
                    'y': 0
                },
                'type': 'INPUT_PORT',
                'concurrentlySchedulableTaskCount': 1,
                'scheduledState': 'RUNNING',
                'allowRemoteAccess': True,
                'portFunction': 'STANDARD',
                'componentType': 'INPUT_PORT',
                "groupIdentifier": "9802c873-3322-3b60-a71d-732d02bd60f8"
            })

        if isinstance(connectable, OutputPort):
            root['outputPorts'].append({
                'identifier': str(connectable.uuid),
                'instanceIdentifier': str(connectable.instance_id),
                'name': connectable_name_text,
                "comments": "",
                'position': {
                    'x': 0,
                    'y': 0
                },
                'type': 'OUTPUT_PORT',
                'concurrentlySchedulableTaskCount': 1,
                'scheduledState': 'RUNNING',
                'allowRemoteAccess': True,
                'portFunction': 'STANDARD',
                'componentType': 'OUTPUT_PORT',
                "groupIdentifier": "9802c873-3322-3b60-a71d-732d02bd60f8"
            })

        if isinstance(connectable, Processor):
            root['processors'].append({
                "identifier": str(connectable.uuid),
                "instanceIdentifier": str(connectable.instance_id),
                "name": connectable_name_text,
                "comments": "",
                "position": {
                    "x": 0,
                    "y": 0
                },
                "type": 'org.apache.nifi.processors.standard.' + connectable.clazz,
                "bundle": {
                    "group": "org.apache.nifi",
                    "artifact": "nifi-standard-nar",
                    "version": nifi_version
                },
                "properties": {key: value for key, value in connectable.properties.items() if connectable.nifi_property_key(key)},
                "propertyDescriptors": {},
                "style": {},
                "schedulingPeriod": "0 sec" if connectable.schedule['scheduling strategy'] == "EVENT_DRIVEN" else connectable.schedule['scheduling period'],
                "schedulingStrategy": "TIMER_DRIVEN",
                "executionNode": "ALL",
                "penaltyDuration": connectable.schedule['penalization period'],
                "yieldDuration": connectable.schedule['yield period'],
                "bulletinLevel": "WARN",
                "runDurationMillis": str(int(connectable.schedule['run duration nanos'] / 1000000)),
                "concurrentlySchedulableTaskCount": connectable.max_concurrent_tasks,
                "autoTerminatedRelationships": connectable.auto_terminate,
                "scheduledState": "RUNNING",
                "retryCount": 10,
                "retriedRelationships": [],
                "backoffMechanism": "PENALIZE_FLOWFILE",
                "maxBackoffPeriod": "10 mins",
                "componentType": "PROCESSOR",
                "groupIdentifier": "9802c873-3322-3b60-a71d-732d02bd60f8"
            })

            for svc in connectable.controller_services:
                if svc in visited:
                    continue

                root['processors'].append({
                    "identifier": str(svc.id),
                    "instanceIdentifier": str(svc.instance_id),
                    "name": svc.name,
                    "type": svc.service_class,
                    "bundle": {
                        "group": svc.group,
                        "artifact": svc.artifact,
                        "version": nifi_version
                    },
                    "properties": svc.properties,
                    "propertyDescriptors": {},
                    "controllerServiceApis": [],
                    "scheduledState": "ENABLED",
                    "bulletinLevel": "WARN",
                    "componentType": "CONTROLLER_SERVICE"
                })

                visited.append(svc)

        for conn_name in connectable.connections:
            conn_procs = connectable.connections[conn_name]

            if not isinstance(conn_procs, list):
                conn_procs = [conn_procs]

            source_type = ""
            if isinstance(connectable, Processor):
                source_type = 'PROCESSOR'
            elif isinstance(connectable, InputPort):
                source_type = 'INPUT_PORT'
            elif isinstance(connectable, OutputPort):
                source_type = 'OUTPUT_PORT'
            else:
                raise Exception('Unexpected source type: %s' % type(connectable))

            for proc in conn_procs:
                dest_type = ""
                if isinstance(proc, Processor):
                    dest_type = 'PROCESSOR'
                elif isinstance(proc, InputPort):
                    dest_type = 'INPUT_PORT'
                elif isinstance(proc, OutputPort):
                    dest_type = 'OUTPUT_PORT'
                else:
                    raise Exception('Unexpected destination type: %s' % type(proc))

                root['connections'].append({
                    "identifier": str(uuid.uuid4()),
                    "instanceIdentifier": str(uuid.uuid4()),
                    "name": "",
                    "source": {
                        "id": str(connectable.uuid),
                        "type": source_type,
                        "groupId": "9802c873-3322-3b60-a71d-732d02bd60f8",
                        "name": conn_name,
                        "comments": "",
                        "instanceIdentifier": str(connectable.instance_id)
                    },
                    "destination": {
                        "id": str(proc.uuid),
                        "type": dest_type,
                        "groupId": "9802c873-3322-3b60-a71d-732d02bd60f8",
                        "name": proc.name,
                        "comments": "",
                        "instanceIdentifier": str(proc.instance_id)
                    },
                    "labelIndex": 1,
                    "zIndex": 0,
                    "selectedRelationships": [conn_name] if not isinstance(connectable, InputPort) and not isinstance(connectable, OutputPort) else [""],
                    "backPressureObjectThreshold": 10,
                    "backPressureDataSizeThreshold": "50 B",
                    "flowFileExpiration": "0 sec",
                    "prioritizers": [],
                    "bends": [],
                    "loadBalanceStrategy": "DO_NOT_LOAD_BALANCE",
                    "partitioningAttribute": "",
                    "loadBalanceCompression": "DO_NOT_COMPRESS",
                    "componentType": "CONNECTION",
                    "groupIdentifier": "9802c873-3322-3b60-a71d-732d02bd60f8"
                })

                if proc not in visited:
                    self.serialize_node(proc, nifi_version, root, visited)
