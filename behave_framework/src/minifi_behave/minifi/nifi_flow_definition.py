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

import json
import uuid
from .flow_definition import FlowDefinition


class NifiFlowDefinition(FlowDefinition):
    NIFI_VERSION: str = '2.2.0'

    def __init__(self, flow_name: str = "NiFi Flow"):
        super().__init__(flow_name)

    def to_json(self) -> str:
        config = {
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

        processors_by_name = {p.name: p for p in self.processors}
        processors_node = config["rootGroup"]["processors"]

        for proc in self.processors:
            processors_node.append({
                "identifier": str(proc.id),
                "instanceIdentifier": str(proc.id),
                "name": proc.name,
                "comments": "",
                "position": {
                    "x": 0,
                    "y": 0
                },
                "type": 'org.apache.nifi.processors.standard.' + proc.class_name,
                "bundle": {
                    "group": "org.apache.nifi",
                    "artifact": "nifi-standard-nar",
                    "version": self.NIFI_VERSION
                },
                "properties": {key: value for key, value in proc.properties.items() if key},
                "propertyDescriptors": {},
                "style": {},
                "schedulingPeriod": "0 sec" if proc.scheduling_strategy == "EVENT_DRIVEN" else proc.scheduling_period,
                "schedulingStrategy": "TIMER_DRIVEN",
                "executionNode": "ALL",
                "penaltyDuration": "5 sec",
                "yieldDuration": "1 sec",
                "bulletinLevel": "WARN",
                "runDurationMillis": "0",
                "concurrentlySchedulableTaskCount": proc.max_concurrent_tasks if proc.max_concurrent_tasks is not None else 1,
                "autoTerminatedRelationships": proc.auto_terminated_relationships,
                "scheduledState": "RUNNING",
                "retryCount": 10,
                "retriedRelationships": [],
                "backoffMechanism": "PENALIZE_FLOWFILE",
                "maxBackoffPeriod": "10 mins",
                "componentType": "PROCESSOR",
                "groupIdentifier": "9802c873-3322-3b60-a71d-732d02bd60f8"
            })

        connections_node = config["rootGroup"]["connections"]

        for conn in self.connections:
            source_proc = processors_by_name.get(conn.source_name)
            dest_proc = processors_by_name.get(conn.target_name)
            if not source_proc or not dest_proc:
                raise ValueError(
                    f"Could not find processors for connection from '{conn.source_name}' to '{conn.target_name}'")

            connections_node.append({
                "identifier": conn.id,
                "instanceIdentifier": conn.id,
                "name": f"{conn.source_name}/{conn.source_relationship}/{conn.target_name}",
                "source": {
                    "id": source_proc.id,
                    "type": "PROCESSOR",
                    "groupId": "9802c873-3322-3b60-a71d-732d02bd60f8",
                    "name": conn.source_name,
                    "comments": "",
                    "instanceIdentifier": source_proc.id
                },
                "destination": {
                    "id": dest_proc.id,
                    "type": "PROCESSOR",
                    "groupId": "9802c873-3322-3b60-a71d-732d02bd60f8",
                    "name": dest_proc.name,
                    "comments": "",
                    "instanceIdentifier": dest_proc.id
                },
                "labelIndex": 1,
                "zIndex": 0,
                "selectedRelationships": [conn.source_relationship],
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

        return json.dumps(config)
