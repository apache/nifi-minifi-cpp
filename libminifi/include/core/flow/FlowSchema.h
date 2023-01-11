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

#pragma once

#include <vector>
#include <string>

namespace org::apache::nifi::minifi::core::flow {

struct FlowSchema {
  using Keys = std::vector<std::string>;

  Keys FLOW_HEADER;
  Keys ROOT_GROUP;

  Keys PROCESSORS;
  Keys PROCESSOR_PROPERTIES;
  Keys AUTOTERMINATED_RELS;
  Keys MAX_CONCURRENT_TASKS;
  Keys PENALIZATION_PERIOD;
  Keys PROC_YIELD_PERIOD;
  Keys RUNDURATION_NANOS;
  Keys ONSCHEDULE_RETRY_INTERVAL;

  Keys CONNECTIONS;
  Keys MAX_QUEUE_SIZE;
  Keys MAX_QUEUE_DATA_SIZE;
  Keys SWAP_THRESHOLD;
  Keys SOURCE_ID;
  Keys SOURCE_NAME;
  Keys DESTINATION_ID;
  Keys DESTINATION_NAME;
  Keys FLOWFILE_EXPIRATION;
  Keys DROP_EMPTY;
  Keys SOURCE_RELATIONSHIP;
  Keys SOURCE_RELATIONSHIP_LIST;

  Keys PROCESS_GROUPS;
  Keys PROCESS_GROUP_VERSION;
  Keys SCHEDULING_STRATEGY;
  Keys SCHEDULING_PERIOD;
  Keys NAME;
  Keys IDENTIFIER;
  Keys TYPE;
  Keys CONTROLLER_SERVICES;
  Keys CONTROLLER_SERVICE_PROPERTIES;
  Keys REMOTE_PROCESS_GROUP;
  Keys PROVENANCE_REPORTING;
  Keys PROVENANCE_REPORTING_PORT_UUID;
  Keys PROVENANCE_REPORTING_BATCH_SIZE;
  Keys FUNNELS;
  Keys INPUT_PORTS;
  Keys OUTPUT_PORTS;
  Keys RPG_URL;
  Keys RPG_YIELD_PERIOD;
  Keys RPG_TIMEOUT;
  Keys RPG_LOCAL_NETWORK_INTERFACE;
  Keys RPG_TRANSPORT_PROTOCOL;
  Keys RPG_PROXY_HOST;
  Keys RPG_PROXY_USER;
  Keys RPG_PROXY_PASSWORD;
  Keys RPG_PROXY_PORT;
  Keys RPG_INPUT_PORTS;
  Keys RPG_OUTPUT_PORTS;
  Keys RPG_PORT_PROPERTIES;
  Keys RPG_PORT_TARGET_ID;

  static FlowSchema getDefault();
  static FlowSchema getNiFiFlowJson();
};

}  // namespace org::apache::nifi::minifi::core::flow
