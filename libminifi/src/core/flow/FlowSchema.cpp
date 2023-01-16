/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "core/flow/FlowSchema.h"

namespace org::apache::nifi::minifi::core::flow {

FlowSchema FlowSchema::getDefault() {
  return FlowSchema{
      .FLOW_HEADER = {"Flow Controller"},
      .ROOT_GROUP = {"."},

      .PROCESSORS = {"Processors"},
      .PROCESSOR_PROPERTIES = {"Properties"},
      .AUTOTERMINATED_RELS = {"auto-terminated relationships list"},
      .MAX_CONCURRENT_TASKS = {"max concurrent tasks"},
      .PENALIZATION_PERIOD = {"penalization period"},
      .PROC_YIELD_PERIOD = {"yield period"},
      .RUNDURATION_NANOS = {"run duration nanos"},
      .ONSCHEDULE_RETRY_INTERVAL = {"onschedule retry interval"},

      .CONNECTIONS = {"Connections"},
      .MAX_QUEUE_SIZE = {"max work queue size"},
      .MAX_QUEUE_DATA_SIZE = {"max work queue data size"},
      .SWAP_THRESHOLD = {"swap threshold"},
      .SOURCE_ID = {"source id"},
      .SOURCE_NAME = {"source name"},
      .DESTINATION_ID = {"destination id"},
      .DESTINATION_NAME = {"destination name"},
      .FLOWFILE_EXPIRATION = {"flowfile expiration"},
      .DROP_EMPTY = {"drop empty"},
      .SOURCE_RELATIONSHIP = {"source relationship name"},
      .SOURCE_RELATIONSHIP_LIST = {"source relationship names"},

      .PROCESS_GROUPS = {"Process Groups"},
      .PROCESS_GROUP_VERSION = {"version"},
      .SCHEDULING_STRATEGY = {"scheduling strategy"},
      .SCHEDULING_PERIOD = {"scheduling period"},
      .NAME = {"name"},
      .IDENTIFIER = {"id"},
      .TYPE = {"class", "type"},
      .CONTROLLER_SERVICES = {"Controller Services"},
      .CONTROLLER_SERVICE_PROPERTIES = {"Properties"},
      .REMOTE_PROCESS_GROUP = {"Remote Processing Groups", "Remote Process Groups"},
      .PROVENANCE_REPORTING = {"Provenance Reporting"},
      .PROVENANCE_REPORTING_PORT_UUID = {"port uuid"},
      .PROVENANCE_REPORTING_BATCH_SIZE = {"batch size"},
      .FUNNELS = {"Funnels"},
      .INPUT_PORTS = {"Input Ports"},
      .OUTPUT_PORTS = {"Output Ports"},

      .RPG_URL = {"url"},
      .RPG_YIELD_PERIOD = {"yield period"},
      .RPG_TIMEOUT = {"timeout"},
      .RPG_LOCAL_NETWORK_INTERFACE = {"local network interface"},
      .RPG_TRANSPORT_PROTOCOL = {"transport protocol"},
      .RPG_PROXY_HOST = {"proxy host"},
      .RPG_PROXY_USER = {"proxy user"},
      .RPG_PROXY_PASSWORD = {"proxy password"},
      .RPG_PROXY_PORT = {"proxy port"},
      .RPG_INPUT_PORTS = {"Input Ports"},
      .RPG_OUTPUT_PORTS = {"Output Ports"},
      .RPG_PORT_PROPERTIES = {"Properties"},
      .RPG_PORT_TARGET_ID = {}
  };
}

FlowSchema FlowSchema::getNiFiFlowJson() {
  return FlowSchema{
      .FLOW_HEADER = {"rootGroup"},
      .ROOT_GROUP = {"rootGroup"},
      .PROCESSORS = {"processors"},
      .PROCESSOR_PROPERTIES = {"properties"},
      .AUTOTERMINATED_RELS = {"autoTerminatedRelationships"},
      .MAX_CONCURRENT_TASKS = {"concurrentlySchedulableTaskCount"},
      .PENALIZATION_PERIOD = {"penaltyDuration"},
      .PROC_YIELD_PERIOD = {"yieldDuration"},
      // TODO(adebreceni): MINIFICPP-2033 since this is unused the mismatch between nano and milli is not an issue
      .RUNDURATION_NANOS = {"runDurationMillis"},
      .ONSCHEDULE_RETRY_INTERVAL = {},

      .CONNECTIONS = {"connections"},
      .MAX_QUEUE_SIZE = {"backPressureObjectThreshold"},
      .MAX_QUEUE_DATA_SIZE = {"backPressureDataSizeThreshold"},
      .SWAP_THRESHOLD = {},
      .SOURCE_ID = {"source/id"},
      .SOURCE_NAME = {"source/name"},
      .DESTINATION_ID = {"destination/id"},
      .DESTINATION_NAME = {"destination/name"},
      .FLOWFILE_EXPIRATION = {"flowFileExpiration"},
      .DROP_EMPTY = {},
      .SOURCE_RELATIONSHIP = {},
      .SOURCE_RELATIONSHIP_LIST = {"selectedRelationships"},

      .PROCESS_GROUPS = {"processGroups"},
      .PROCESS_GROUP_VERSION = {},
      .SCHEDULING_STRATEGY = {"schedulingStrategy"},
      .SCHEDULING_PERIOD = {"schedulingPeriod"},
      .NAME = {"name"},
      .IDENTIFIER = {"identifier"},
      .TYPE = {"type"},
      .CONTROLLER_SERVICES = {"controllerServices"},
      .CONTROLLER_SERVICE_PROPERTIES = {"properties"},
      .REMOTE_PROCESS_GROUP = {"remoteProcessGroups"},
      .PROVENANCE_REPORTING = {},
      .PROVENANCE_REPORTING_PORT_UUID = {},
      .PROVENANCE_REPORTING_BATCH_SIZE = {},
      .FUNNELS = {"funnels"},
      .INPUT_PORTS = {"inputPorts"},
      .OUTPUT_PORTS = {"outputPorts"},

      .RPG_URL = {"targetUri"},
      .RPG_YIELD_PERIOD = {"yieldDuration"},
      .RPG_TIMEOUT = {"communicationsTimeout"},
      .RPG_LOCAL_NETWORK_INTERFACE = {"localNetworkInterface"},
      .RPG_TRANSPORT_PROTOCOL = {"transportProtocol"},
      .RPG_PROXY_HOST = {"proxyHost"},
      .RPG_PROXY_USER = {"proxyUser"},
      .RPG_PROXY_PASSWORD = {"proxyPassword"},
      .RPG_PROXY_PORT = {"proxyPort"},
      .RPG_INPUT_PORTS = {"inputPorts"},
      .RPG_OUTPUT_PORTS = {"outputPorts"},
      .RPG_PORT_PROPERTIES = {},
      .RPG_PORT_TARGET_ID = {"targetId"}
  };
}

}  // namespace org::apache::nifi::minifi::core::flow
