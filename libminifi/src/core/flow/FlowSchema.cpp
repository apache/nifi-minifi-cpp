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
      .flow_header = {"Flow Controller"},
      .root_group = {"."},

      .processors = {"Processors"},
      .processor_properties = {"Properties"},
      .autoterminated_rels = {"auto-terminated relationships list"},
      .max_concurrent_tasks = {"max concurrent tasks"},
      .penalization_period = {"penalization period"},
      .proc_yield_period = {"yield period"},
      .runduration_nanos = {"run duration nanos"},

      .connections = {"Connections"},
      .max_queue_size = {"max work queue size"},
      .max_queue_data_size = {"max work queue data size"},
      .swap_threshold = {"swap threshold"},
      .source_id = {"source id"},
      .source_name = {"source name"},
      .destination_id = {"destination id"},
      .destination_name = {"destination name"},
      .flowfile_expiration = {"flowfile expiration"},
      .drop_empty = {"drop empty"},
      .source_relationship = {"source relationship name"},
      .source_relationship_list = {"source relationship names"},

      .process_groups = {"Process Groups"},
      .process_group_version = {"version"},
      .scheduling_strategy = {"scheduling strategy"},
      .scheduling_period = {"scheduling period"},
      .name = {"name"},
      .identifier = {"id"},
      .type = {"class", "type"},
      .controller_services = {"Controller Services"},
      .controller_service_properties = {"Properties"},
      .remote_process_group = {"Remote Processing Groups", "Remote Process Groups"},
      .provenance_reporting = {"Provenance Reporting"},
      .provenance_reporting_port_uuid = {"port uuid"},
      .provenance_reporting_batch_size = {"batch size"},
      .funnels = {"Funnels"},
      .input_ports = {"Input Ports"},
      .output_ports = {"Output Ports"},

      .rpg_url = {"url"},
      .rpg_yield_period = {"yield period"},
      .rpg_timeout = {"timeout"},
      .rpg_local_network_interface = {"local network interface"},
      .rpg_transport_protocol = {"transport protocol"},
      .rpg_proxy_host = {"proxy host"},
      .rpg_proxy_user = {"proxy user"},
      .rpg_proxy_password = {"proxy password"},
      .rpg_proxy_port = {"proxy port"},
      .rpg_input_ports = {"Input Ports"},
      .rpg_output_ports = {"Output Ports"},
      .rpg_port_properties = {"Properties"},
      .rpg_port_target_id = {},

      .parameter_contexts = {"Parameter Contexts"},
      .parameters = {"Parameters"},
      .description = {"description"},
      .value = {"value"},
      .parameter_context_name = {"Parameter Context Name"},
      .sensitive = {"sensitive"},
      .provided = {"provided"},
      .inherited_parameter_contexts = {"Inherited Parameter Contexts"},
      .parameter_providers = {"Parameter Providers"},
      .parameter_provider_properties = {"Properties"},
      .parameter_provider{"Parameter Provider"}
  };
}

FlowSchema FlowSchema::getNiFiFlowJson() {
  return FlowSchema{
      .flow_header = {"rootGroup"},
      .root_group = {"rootGroup"},
      .processors = {"processors"},
      .processor_properties = {"properties"},
      .autoterminated_rels = {"autoTerminatedRelationships"},
      .max_concurrent_tasks = {"concurrentlySchedulableTaskCount"},
      .penalization_period = {"penaltyDuration"},
      .proc_yield_period = {"yieldDuration"},
      // TODO(adebreceni): MINIFICPP-2033 since this is unused the mismatch between nano and milli is not an issue
      .runduration_nanos = {"runDurationMillis"},

      .connections = {"connections"},
      .max_queue_size = {"backPressureObjectThreshold"},
      .max_queue_data_size = {"backPressureDataSizeThreshold"},
      .swap_threshold = {},
      .source_id = {"source/id"},
      .source_name = {"source/name"},
      .destination_id = {"destination/id"},
      .destination_name = {"destination/name"},
      .flowfile_expiration = {"flowFileExpiration"},
      // contrary to nifi we support dropEmpty in flow json as well
      .drop_empty = {"dropEmpty"},
      .source_relationship = {},
      .source_relationship_list = {"selectedRelationships"},

      .process_groups = {"processGroups"},
      .process_group_version = {},
      .scheduling_strategy = {"schedulingStrategy"},
      .scheduling_period = {"schedulingPeriod"},
      .name = {"name"},
      .identifier = {"identifier"},
      .type = {"type"},
      .controller_services = {"controllerServices"},
      .controller_service_properties = {"properties"},
      .remote_process_group = {"remoteProcessGroups"},
      .provenance_reporting = {},
      .provenance_reporting_port_uuid = {},
      .provenance_reporting_batch_size = {},
      .funnels = {"funnels"},
      .input_ports = {"inputPorts"},
      .output_ports = {"outputPorts"},

      .rpg_url = {"targetUris", "targetUri"},
      .rpg_yield_period = {"yieldDuration"},
      .rpg_timeout = {"communicationsTimeout"},
      .rpg_local_network_interface = {"localNetworkInterface"},
      .rpg_transport_protocol = {"transportProtocol"},
      .rpg_proxy_host = {"proxyHost"},
      .rpg_proxy_user = {"proxyUser"},
      .rpg_proxy_password = {"proxyPassword"},
      .rpg_proxy_port = {"proxyPort"},
      .rpg_input_ports = {"inputPorts"},
      .rpg_output_ports = {"outputPorts"},
      .rpg_port_properties = {"properties"},
      .rpg_port_target_id = {"targetId"},

      .parameter_contexts = {"parameterContexts"},
      .parameters = {"parameters"},
      .description = {"description"},
      .value = {"value"},
      .parameter_context_name = {"parameterContextName"},
      .sensitive = {"sensitive"},
      .provided = {"provided"},
      .inherited_parameter_contexts = {"inheritedParameterContexts"},
      .parameter_providers = {"parameterProviders"},
      .parameter_provider_properties = {"properties"},
      .parameter_provider{"parameterProvider"}
  };
}

}  // namespace org::apache::nifi::minifi::core::flow
