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

  Keys flow_header;
  Keys root_group;

  Keys processors;
  Keys processor_properties;
  Keys autoterminated_rels;
  Keys max_concurrent_tasks;
  Keys penalization_period;
  Keys proc_yield_period;
  Keys bulletin_level;
  Keys runduration_nanos;

  Keys connections;
  Keys max_queue_size;
  Keys max_queue_data_size;
  Keys swap_threshold;
  Keys source_id;
  Keys source_name;
  Keys destination_id;
  Keys destination_name;
  Keys flowfile_expiration;
  Keys drop_empty;
  Keys source_relationship;
  Keys source_relationship_list;

  Keys process_groups;
  Keys process_group_version;
  Keys scheduling_strategy;
  Keys scheduling_period;
  Keys name;
  Keys identifier;
  Keys type;
  Keys controller_services;
  Keys controller_service_properties;
  Keys remote_process_group;
  Keys provenance_reporting;
  Keys provenance_reporting_port_uuid;
  Keys provenance_reporting_batch_size;
  Keys funnels;
  Keys input_ports;
  Keys output_ports;
  Keys rpg_url;
  Keys rpg_yield_period;
  Keys rpg_timeout;
  Keys rpg_local_network_interface;
  Keys rpg_transport_protocol;
  Keys rpg_proxy_host;
  Keys rpg_proxy_user;
  Keys rpg_proxy_password;
  Keys rpg_proxy_port;
  Keys rpg_input_ports;
  Keys rpg_output_ports;
  Keys rpg_port_properties;
  Keys rpg_port_target_id;

  Keys parameter_contexts;
  Keys parameters;
  Keys description;
  Keys value;
  Keys parameter_context_name;
  Keys sensitive;

  static FlowSchema getDefault();
  static FlowSchema getNiFiFlowJson();
};

}  // namespace org::apache::nifi::minifi::core::flow
