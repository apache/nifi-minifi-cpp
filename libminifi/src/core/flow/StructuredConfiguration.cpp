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

#include <memory>
#include <vector>
#include <set>
#include <cinttypes>

#include "core/flow/StructuredConfiguration.h"
#include "core/flow/CheckRequiredField.h"
#include "core/flow/StructuredConnectionParser.h"
#include "core/state/Value.h"
#include "Defaults.h"
#include "utils/TimeUtil.h"
#include "utils/RegexUtils.h"
#include "Funnel.h"

namespace org::apache::nifi::minifi::core::flow {

std::shared_ptr<utils::IdGenerator> StructuredConfiguration::id_generator_ = utils::IdGenerator::getIdGenerator();

std::unique_ptr<core::ProcessGroup> StructuredConfiguration::getRoot() {
  if (!config_path_) {
    logger_->log_error("Cannot instantiate flow, no config file is set.");
    throw Exception(ExceptionType::FLOW_EXCEPTION, "No config file specified");
  }
  const auto configuration = filesystem_->read(config_path_.value());
  if (!configuration) {
    // non-existence of flow config file is not a dealbreaker, the caller might fetch it from network
    return nullptr;
  }
  return getRootFromPayload(configuration.value());
}

StructuredConfiguration::StructuredConfiguration(ConfigurationContext ctx, std::shared_ptr<logging::Logger> logger)
    : FlowConfiguration(std::move(ctx)),
      logger_(std::move(logger)) {}

std::unique_ptr<core::ProcessGroup> StructuredConfiguration::parseRootProcessGroup(const Node& root_flow_node) {
  checkRequiredField(root_flow_node, schema_.flow_header);
  auto root_group = parseProcessGroup(root_flow_node[schema_.flow_header], root_flow_node[schema_.root_group], true);
  this->name_ = root_group->getName();
  return root_group;
}

std::unique_ptr<core::ProcessGroup> StructuredConfiguration::createProcessGroup(const Node& node, bool is_root) {
  int version = 0;

  checkRequiredField(node, schema_.name);
  auto flowName = node[schema_.name].getString().value();

  utils::Identifier uuid;
  // assignment throws on invalid uuid
  uuid = getOrGenerateId(node);

  if (node[schema_.process_group_version]) {
    version = gsl::narrow<int>(node[schema_.process_group_version].getInt64().value());
  }

  logger_->log_debug("parseRootProcessGroup: id => [%s], name => [%s]", uuid.to_string(), flowName);
  std::unique_ptr<core::ProcessGroup> group;
  if (is_root) {
    group = FlowConfiguration::createRootProcessGroup(flowName, uuid, version);
  } else {
    group = FlowConfiguration::createSimpleProcessGroup(flowName, uuid, version);
  }

  if (node[schema_.onschedule_retry_interval]) {
    auto onScheduleRetryPeriod = node[schema_.onschedule_retry_interval].getString().value();
    logger_->log_debug("parseRootProcessGroup: onschedule retry period => [%s]", onScheduleRetryPeriod);

    auto on_schedule_retry_period_value = utils::timeutils::StringToDuration<std::chrono::milliseconds>(onScheduleRetryPeriod);
    if (on_schedule_retry_period_value.has_value() && group) {
      logger_->log_debug("parseRootProcessGroup: onschedule retry => [%" PRId64 "] ms", on_schedule_retry_period_value->count());
      group->setOnScheduleRetryPeriod(on_schedule_retry_period_value->count());
    }
  }

  return group;
}

std::unique_ptr<core::ProcessGroup> StructuredConfiguration::parseProcessGroup(const Node& header_node, const Node& node, bool is_root) {
  auto group = createProcessGroup(header_node, is_root);
  Node processorsNode = node[schema_.processors];
  Node connectionsNode = node[schema_.connections];
  Node funnelsNode = node[schema_.funnels];
  Node inputPortsNode = node[schema_.input_ports];
  Node outputPortsNode = node[schema_.output_ports];
  Node remoteProcessingGroupsNode = node[schema_.remote_process_group];
  Node childProcessGroupNodeSeq = node[schema_.process_groups];

  parseProcessorNode(processorsNode, group.get());
  parseRemoteProcessGroup(remoteProcessingGroupsNode, group.get());
  parseFunnels(funnelsNode, group.get());
  parsePorts(inputPortsNode, group.get(), PortType::INPUT);
  parsePorts(outputPortsNode, group.get(), PortType::OUTPUT);

  if (childProcessGroupNodeSeq && childProcessGroupNodeSeq.isSequence()) {
    for (const auto& childProcessGroupNode : childProcessGroupNodeSeq) {
      group->addProcessGroup(parseProcessGroup(childProcessGroupNode, childProcessGroupNode));
    }
  }
  // parse connections last to give feedback if the source and/or destination processors
  // is not in the same process group or input/output port connections are not allowed
  parseConnection(connectionsNode, group.get());
  return group;
}

std::unique_ptr<core::ProcessGroup> StructuredConfiguration::getRootFrom(const Node& root_node, FlowSchema schema) {
  try {
    schema_ = std::move(schema);
    uuids_.clear();
    Node controllerServiceNode = root_node[schema_.root_group][schema_.controller_services];
    Node provenanceReportNode = root_node[schema_.provenance_reporting];

    parseControllerServices(controllerServiceNode);
    // Create the root process group
    std::unique_ptr<core::ProcessGroup> root = parseRootProcessGroup(root_node);
    parseProvenanceReporting(provenanceReportNode, root.get());

    // set the controller services into the root group.
    for (const auto& controller_service : controller_services_->getAllControllerServices()) {
      root->addControllerService(controller_service->getName(), controller_service);
      root->addControllerService(controller_service->getUUIDStr(), controller_service);
    }

    root->verify();

    return root;
  } catch (const std::exception& ex) {
    logger_->log_error("Error while processing configuration file: %s", ex.what());
    throw;
  }
}

void StructuredConfiguration::parseProcessorNode(const Node& processors_node, core::ProcessGroup* parentGroup) {
  int64_t runDurationNanos = -1;
  utils::Identifier uuid;
  std::unique_ptr<core::Processor> processor;

  if (!parentGroup) {
    logger_->log_error("parseProcessNode: no parent group exists");
    return;
  }

  if (!processors_node) {
    throw std::invalid_argument("Cannot instantiate a MiNiFi instance without a defined Processors configuration node.");
  }
  if (!processors_node.isSequence()) {
    throw std::invalid_argument(
        "Cannot instantiate a MiNiFi instance without a defined Processors configuration node.");
  }
  // Evaluate sequence of processors
  for (const auto& procNode : processors_node) {
    core::ProcessorConfig procCfg;

    checkRequiredField(procNode, schema_.name);
    procCfg.name = procNode[schema_.name].getString().value();
    procCfg.id = getOrGenerateId(procNode);

    uuid = procCfg.id;
    logger_->log_debug("parseProcessorNode: name => [%s] id => [%s]", procCfg.name, procCfg.id);
    checkRequiredField(procNode, schema_.type);
    procCfg.javaClass = procNode[schema_.type].getString().value();
    logger_->log_debug("parseProcessorNode: class => [%s]", procCfg.javaClass);

    // Determine the processor name only from the Java class
    auto lastOfIdx = procCfg.javaClass.find_last_of('.');
    if (lastOfIdx != std::string::npos) {
      lastOfIdx++;  // if a value is found, increment to move beyond the .
      std::string processorName = procCfg.javaClass.substr(lastOfIdx);
      processor = this->createProcessor(processorName, procCfg.javaClass, uuid);
    } else {
      // Allow unqualified class names for core processors
      processor = this->createProcessor(procCfg.javaClass, uuid);
    }

    if (!processor) {
      logger_->log_error("Could not create a processor %s with id %s", procCfg.name, procCfg.id);
      throw std::invalid_argument("Could not create processor " + procCfg.name);
    }

    processor->setName(procCfg.name);

    processor->setFlowIdentifier(flow_version_->getFlowIdentifier());

    procCfg.schedulingStrategy = getOptionalField(procNode, schema_.scheduling_strategy, DEFAULT_SCHEDULING_STRATEGY);
    logger_->log_debug("parseProcessorNode: scheduling strategy => [%s]", procCfg.schedulingStrategy);

    procCfg.schedulingPeriod = getOptionalField(procNode, schema_.scheduling_period, DEFAULT_SCHEDULING_PERIOD_STR);

    logger_->log_debug("parseProcessorNode: scheduling period => [%s]", procCfg.schedulingPeriod);

    if (auto tasksNode = procNode[schema_.max_concurrent_tasks]) {
      procCfg.maxConcurrentTasks = tasksNode.getIntegerAsString().value();
      logger_->log_debug("parseProcessorNode: max concurrent tasks => [%s]", procCfg.maxConcurrentTasks);
    }

    if (auto penalizationNode = procNode[schema_.penalization_period]) {
      procCfg.penalizationPeriod = penalizationNode.getString().value();
      logger_->log_debug("parseProcessorNode: penalization period => [%s]", procCfg.penalizationPeriod);
    }

    if (auto yieldNode = procNode[schema_.proc_yield_period]) {
      procCfg.yieldPeriod = yieldNode.getString().value();
      logger_->log_debug("parseProcessorNode: yield period => [%s]", procCfg.yieldPeriod);
    }

    if (auto runNode = procNode[schema_.runduration_nanos]) {
      procCfg.runDurationNanos = runNode.getIntegerAsString().value();
      logger_->log_debug("parseProcessorNode: run duration nanos => [%s]", procCfg.runDurationNanos);
    }

    // handle auto-terminated relationships
    if (Node autoTerminatedSequence = procNode[schema_.autoterminated_rels]) {
      std::vector<std::string> rawAutoTerminatedRelationshipValues;
      if (autoTerminatedSequence.isSequence() && autoTerminatedSequence.size() > 0) {
        for (const auto& autoTerminatedRel : autoTerminatedSequence) {
          rawAutoTerminatedRelationshipValues.push_back(autoTerminatedRel.getString().value());
        }
      }
      procCfg.autoTerminatedRelationships = rawAutoTerminatedRelationshipValues;
    }

    // handle processor properties
    if (Node propertiesNode = procNode[schema_.processor_properties]) {
      parsePropertiesNode(propertiesNode, *processor, procCfg.name);
    }

    // Take care of scheduling

    if (procCfg.schedulingStrategy == "TIMER_DRIVEN" || procCfg.schedulingStrategy == "EVENT_DRIVEN") {
      if (auto scheduling_period = utils::timeutils::StringToDuration<std::chrono::nanoseconds>(procCfg.schedulingPeriod)) {
        logger_->log_debug("convert: parseProcessorNode: schedulingPeriod => [%" PRId64 "] ns", scheduling_period->count());
        processor->setSchedulingPeriod(*scheduling_period);
      }
    } else {
      processor->setCronPeriod(procCfg.schedulingPeriod);
    }

    if (auto penalization_period = utils::timeutils::StringToDuration<std::chrono::milliseconds>(procCfg.penalizationPeriod)) {
      logger_->log_debug("convert: parseProcessorNode: penalizationPeriod => [%" PRId64 "] ms", penalization_period->count());
      processor->setPenalizationPeriod(penalization_period.value());
    }

    if (auto yield_period = utils::timeutils::StringToDuration<std::chrono::milliseconds>(procCfg.yieldPeriod)) {
      logger_->log_debug("convert: parseProcessorNode: yieldPeriod => [%" PRId64 "] ms", yield_period->count());
      processor->setYieldPeriodMsec(yield_period.value());
    }

    // Default to running
    processor->setScheduledState(core::RUNNING);

    if (procCfg.schedulingStrategy == "TIMER_DRIVEN") {
      processor->setSchedulingStrategy(core::TIMER_DRIVEN);
      logger_->log_debug("setting scheduling strategy as %s", procCfg.schedulingStrategy);
    } else if (procCfg.schedulingStrategy == "EVENT_DRIVEN") {
      processor->setSchedulingStrategy(core::EVENT_DRIVEN);
      logger_->log_debug("setting scheduling strategy as %s", procCfg.schedulingStrategy);
    } else {
      processor->setSchedulingStrategy(core::CRON_DRIVEN);
      logger_->log_debug("setting scheduling strategy as %s", procCfg.schedulingStrategy);
    }

    int32_t maxConcurrentTasks;
    if (core::Property::StringToInt(procCfg.maxConcurrentTasks, maxConcurrentTasks)) {
      logger_->log_debug("parseProcessorNode: maxConcurrentTasks => [%d]", maxConcurrentTasks);
      processor->setMaxConcurrentTasks((uint8_t) maxConcurrentTasks);
    }

    if (core::Property::StringToInt(procCfg.runDurationNanos, runDurationNanos)) {
      logger_->log_debug("parseProcessorNode: runDurationNanos => [%d]", runDurationNanos);
      processor->setRunDurationNano(std::chrono::nanoseconds(runDurationNanos));
    }

    std::vector<core::Relationship> autoTerminatedRelationships;
    for (auto &&relString : procCfg.autoTerminatedRelationships) {
      core::Relationship relationship(relString, "");
      logger_->log_debug("parseProcessorNode: autoTerminatedRelationship  => [%s]", relString);
      autoTerminatedRelationships.push_back(relationship);
    }

    processor->setAutoTerminatedRelationships(autoTerminatedRelationships);

    parentGroup->addProcessor(std::move(processor));
  }
}

void StructuredConfiguration::parseRemoteProcessGroup(const Node& rpg_node_seq, core::ProcessGroup* parentGroup) {
  utils::Identifier uuid;
  std::string id;

  if (!parentGroup) {
    logger_->log_error("parseRemoteProcessGroup: no parent group exists");
    return;
  }

  if (!rpg_node_seq || !rpg_node_seq.isSequence()) {
    return;
  }
  for (const auto& currRpgNode : rpg_node_seq) {
    checkRequiredField(currRpgNode, schema_.name);
    auto name = currRpgNode[schema_.name].getString().value();
    id = getOrGenerateId(currRpgNode);

    logger_->log_debug("parseRemoteProcessGroup: name => [%s], id => [%s]", name, id);

    auto url = getOptionalField(currRpgNode, schema_.rpg_url, "");

    logger_->log_debug("parseRemoteProcessGroup: url => [%s]", url);

    uuid = id;
    auto group = createRemoteProcessGroup(name, uuid);
    group->setParent(parentGroup);

    if (currRpgNode[schema_.rpg_yield_period]) {
      auto yieldPeriod = currRpgNode[schema_.rpg_yield_period].getString().value();
      logger_->log_debug("parseRemoteProcessGroup: yield period => [%s]", yieldPeriod);

      auto yield_period_value = utils::timeutils::StringToDuration<std::chrono::milliseconds>(yieldPeriod);
      if (yield_period_value.has_value() && group) {
        logger_->log_debug("parseRemoteProcessGroup: yieldPeriod => [%" PRId64 "] ms", yield_period_value->count());
        group->setYieldPeriodMsec(*yield_period_value);
      }
    }

    if (currRpgNode[schema_.rpg_timeout]) {
      auto timeout = currRpgNode[schema_.rpg_timeout].getString().value();
      logger_->log_debug("parseRemoteProcessGroup: timeout => [%s]", timeout);

      auto timeout_value = utils::timeutils::StringToDuration<std::chrono::milliseconds>(timeout);
      if (timeout_value.has_value() && group) {
        logger_->log_debug("parseRemoteProcessGroup: timeoutValue => [%" PRId64 "] ms", timeout_value->count());
        group->setTimeout(timeout_value->count());
      }
    }

    if (currRpgNode[schema_.rpg_local_network_interface]) {
      auto interface = currRpgNode[schema_.rpg_local_network_interface].getString().value();
      logger_->log_debug("parseRemoteProcessGroup: local network interface => [%s]", interface);
      group->setInterface(interface);
    }

    if (currRpgNode[schema_.rpg_transport_protocol]) {
      auto transport_protocol = currRpgNode[schema_.rpg_transport_protocol].getString().value();
      logger_->log_debug("parseRemoteProcessGroup: transport protocol => [%s]", transport_protocol);
      if (transport_protocol == "HTTP") {
        group->setTransportProtocol(transport_protocol);
        if (currRpgNode[schema_.rpg_proxy_host]) {
          auto http_proxy_host = currRpgNode[schema_.rpg_proxy_host].getString().value();
          logger_->log_debug("parseRemoteProcessGroup: proxy host => [%s]", http_proxy_host);
          group->setHttpProxyHost(http_proxy_host);
          if (currRpgNode[schema_.rpg_proxy_user]) {
            auto http_proxy_username = currRpgNode[schema_.rpg_proxy_user].getString().value();
            logger_->log_debug("parseRemoteProcessGroup: proxy user => [%s]", http_proxy_username);
            group->setHttpProxyUserName(http_proxy_username);
          }
          if (currRpgNode[schema_.rpg_proxy_password]) {
            auto http_proxy_password = currRpgNode[schema_.rpg_proxy_password].getString().value();
            logger_->log_debug("parseRemoteProcessGroup: proxy password => [%s]", http_proxy_password);
            group->setHttpProxyPassWord(http_proxy_password);
          }
          if (currRpgNode[schema_.rpg_proxy_port]) {
            auto http_proxy_port = currRpgNode[schema_.rpg_proxy_port].getIntegerAsString().value();
            int32_t port;
            if (core::Property::StringToInt(http_proxy_port, port)) {
              logger_->log_debug("parseRemoteProcessGroup: proxy port => [%d]", port);
              group->setHttpProxyPort(port);
            }
          }
        }
      } else if (transport_protocol == "RAW") {
        group->setTransportProtocol(transport_protocol);
      } else {
        std::stringstream stream;
        stream << "Invalid transport protocol " << transport_protocol;
        throw minifi::Exception(ExceptionType::SITE2SITE_EXCEPTION, stream.str().c_str());
      }
    }

    group->setTransmitting(true);
    group->setURL(url);

    checkRequiredField(currRpgNode, schema_.rpg_input_ports);
    auto inputPorts = currRpgNode[schema_.rpg_input_ports];
    if (inputPorts && inputPorts.isSequence()) {
      for (const auto& currPort : inputPorts) {
        parseRPGPort(currPort, group.get(), sitetosite::SEND);
      }  // for node
    }
    auto outputPorts = currRpgNode[schema_.rpg_output_ports];
    if (outputPorts && outputPorts.isSequence()) {
      for (const auto& currPort : outputPorts) {
        logger_->log_debug("Got a current port, iterating...");

        parseRPGPort(currPort, group.get(), sitetosite::RECEIVE);
      }  // for node
    }
    parentGroup->addProcessGroup(std::move(group));
  }
}

void StructuredConfiguration::parseProvenanceReporting(const Node& node, core::ProcessGroup* parent_group) {
  utils::Identifier port_uuid;

  if (!parent_group) {
    logger_->log_error("parseProvenanceReporting: no parent group exists");
    return;
  }

  if (!node || node.isNull()) {
    logger_->log_debug("no provenance reporting task specified");
    return;
  }

  auto reportTask = createProvenanceReportTask();

  checkRequiredField(node, schema_.scheduling_strategy);
  auto schedulingStrategyStr = node[schema_.scheduling_strategy].getString().value();
  checkRequiredField(node, schema_.scheduling_period);
  auto schedulingPeriodStr = node[schema_.scheduling_period].getString().value();

  if (auto scheduling_period = utils::timeutils::StringToDuration<std::chrono::nanoseconds>(schedulingPeriodStr)) {
    logger_->log_debug("ProvenanceReportingTask schedulingPeriod %" PRId64 " ns", scheduling_period->count());
    reportTask->setSchedulingPeriod(*scheduling_period);
  }

  if (schedulingStrategyStr == "TIMER_DRIVEN") {
    reportTask->setSchedulingStrategy(core::TIMER_DRIVEN);
    logger_->log_debug("ProvenanceReportingTask scheduling strategy %s", schedulingStrategyStr);
  } else {
    throw std::invalid_argument("Invalid scheduling strategy " + schedulingStrategyStr);
  }

  int64_t lvalue;
  if (node["host"] && node["port"]) {
    auto hostStr = node["host"].getString().value();

    std::string portStr = node["port"].getIntegerAsString().value();
    if (core::Property::StringToInt(portStr, lvalue) && !hostStr.empty()) {
      logger_->log_debug("ProvenanceReportingTask port %" PRId64, lvalue);
      std::string url = hostStr + ":" + portStr;
      reportTask->setURL(url);
    }
  }

  if (node["url"]) {
    auto urlStr = node["url"].getString().value();
    if (!urlStr.empty()) {
      reportTask->setURL(urlStr);
      logger_->log_debug("ProvenanceReportingTask URL %s", urlStr);
    }
  }
  checkRequiredField(node, schema_.provenance_reporting_port_uuid);
  auto portUUIDStr = node[schema_.provenance_reporting_port_uuid].getString().value();
  checkRequiredField(node, schema_.provenance_reporting_batch_size);
  auto batchSizeStr = node[schema_.provenance_reporting_batch_size].getString().value();

  logger_->log_debug("ProvenanceReportingTask port uuid %s", portUUIDStr);
  port_uuid = portUUIDStr;
  reportTask->setPortUUID(port_uuid);

  if (core::Property::StringToInt(batchSizeStr, lvalue)) {
    reportTask->setBatchSize(gsl::narrow<int>(lvalue));
  }

  reportTask->initialize();

  // add processor to parent
  reportTask->setScheduledState(core::RUNNING);
  parent_group->addProcessor(std::move(reportTask));
}

void StructuredConfiguration::parseControllerServices(const Node& controller_services_node) {
  if (!controller_services_node || !controller_services_node.isSequence()) {
    return;
  }
  for (const auto& service_node : controller_services_node) {
    checkRequiredField(service_node, schema_.name);

    auto type = getRequiredField(service_node, schema_.type);
    logger_->log_debug("Using type %s for controller service node", type);

    std::string fullType = type;
    auto lastOfIdx = type.find_last_of('.');
    if (lastOfIdx != std::string::npos) {
      lastOfIdx++;  // if a value is found, increment to move beyond the .
      type = type.substr(lastOfIdx);
    }

    auto name = service_node[schema_.name].getString().value();
    auto id = getRequiredIdField(service_node);

    utils::Identifier uuid;
    uuid = id;
    std::shared_ptr<core::controller::ControllerServiceNode> controller_service_node = createControllerService(type, fullType, name, uuid);
    if (nullptr != controller_service_node) {
      logger_->log_debug("Created Controller Service with UUID %s and name %s", id, name);
      controller_service_node->initialize();
      if (Node propertiesNode = service_node[schema_.controller_service_properties]) {
        // we should propagate properties to the node and to the implementation
        parsePropertiesNode(propertiesNode, *controller_service_node, name);
        if (auto controllerServiceImpl = controller_service_node->getControllerServiceImplementation(); controllerServiceImpl) {
          parsePropertiesNode(propertiesNode, *controllerServiceImpl, name);
        }
      }
    } else {
      logger_->log_debug("Could not locate %s", type);
    }
    controller_services_->put(id, controller_service_node);
    controller_services_->put(name, controller_service_node);
  }
}

void StructuredConfiguration::parseConnection(const Node& connection_node_seq, core::ProcessGroup* parent) {
  if (!parent) {
    logger_->log_error("parseProcessNode: no parent group was provided");
    return;
  }
  if (!connection_node_seq || !connection_node_seq.isSequence()) {
    return;
  }

  for (const auto& connection_node : connection_node_seq) {
    // for backwards compatibility we ignore invalid connection_nodes instead of throwing
    // previously the ConnectionParser created an unreachable connection in this case
    if (!connection_node || !connection_node.isMap()) {
      logger_->log_error("Invalid connection node, ignoring");
      continue;
    }
    // Configure basic connection
    const std::string id = getOrGenerateId(connection_node);

    // Default name to be same as ID
    // If name is specified in configuration, use the value
    const auto name = connection_node[schema_.name].getString().value_or(id);

    const auto uuid = utils::Identifier::parse(id) | utils::orElse([this] {
      logger_->log_debug("Incorrect connection UUID format.");
      throw Exception(ExceptionType::GENERAL_EXCEPTION, "Incorrect connection UUID format.");
    });

    auto connection = createConnection(name, uuid.value());
    logger_->log_debug("Created connection with UUID %s and name %s", id, name);
    const StructuredConnectionParser connectionParser(connection_node, name, gsl::not_null<core::ProcessGroup*>{ parent }, logger_, schema_);
    connectionParser.configureConnectionSourceRelationships(*connection);
    connection->setBackpressureThresholdCount(connectionParser.getWorkQueueSize());
    connection->setBackpressureThresholdDataSize(connectionParser.getWorkQueueDataSize());
    connection->setSwapThreshold(connectionParser.getSwapThreshold());
    connection->setSourceUUID(connectionParser.getSourceUUID());
    connection->setDestinationUUID(connectionParser.getDestinationUUID());
    connection->setFlowExpirationDuration(connectionParser.getFlowFileExpiration());
    connection->setDropEmptyFlowFiles(connectionParser.getDropEmpty());

    parent->addConnection(std::move(connection));
  }
}

void StructuredConfiguration::parseRPGPort(const Node& port_node, core::ProcessGroup* parent, sitetosite::TransferDirection direction) {
  utils::Identifier uuid;

  if (!parent) {
    logger_->log_error("parseProcessNode: no parent group existed");
    return;
  }

  // Check for required fields
  checkRequiredField(port_node, schema_.name);
  auto nameStr = port_node[schema_.name].getString().value();
  auto portId = getRequiredIdField(port_node,
    "The field 'id' is required for "
    "the port named '" + nameStr + "' in the Flow Config. If this port "
    "is an input port for a NiFi Remote Process Group, the port "
    "id should match the corresponding id specified in the NiFi configuration. "
    "This is a UUID of the format XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX.");
  uuid = portId;

  auto port = std::make_unique<minifi::RemoteProcessorGroupPort>(
          nameStr, parent->getURL(), this->configuration_, uuid);
  port->setDirection(direction);
  port->setTimeout(parent->getTimeout());
  port->setTransmitting(true);
  port->setYieldPeriodMsec(parent->getYieldPeriodMsec());
  port->initialize();
  if (!parent->getInterface().empty())
    port->setInterface(parent->getInterface());
  if (parent->getTransportProtocol() == "HTTP") {
    port->enableHTTP();
    if (!parent->getHttpProxyHost().empty())
      port->setHTTPProxy(parent->getHTTPProxy());
  }
  // else defaults to RAW

  // handle port properties
  if (Node propertiesNode = port_node[schema_.rpg_port_properties]) {
    parsePropertiesNode(propertiesNode, *port, nameStr);
  } else {
    parsePropertyNodeElement(std::string(minifi::RemoteProcessorGroupPort::portUUID.name), port_node[schema_.rpg_port_target_id], *port);
    validateComponentProperties(*port, nameStr, port_node.getPath());
  }

  // add processor to parent
  auto& processor = *port;
  parent->addProcessor(std::move(port));
  processor.setScheduledState(core::RUNNING);

  if (auto tasksNode = port_node[schema_.max_concurrent_tasks]) {
    std::string rawMaxConcurrentTasks = tasksNode.getIntegerAsString().value();
    int32_t maxConcurrentTasks;
    if (core::Property::StringToInt(rawMaxConcurrentTasks, maxConcurrentTasks)) {
      processor.setMaxConcurrentTasks(maxConcurrentTasks);
    }
    logger_->log_debug("parseProcessorNode: maxConcurrentTasks => [%d]", maxConcurrentTasks);
    processor.setMaxConcurrentTasks(maxConcurrentTasks);
  }
}

void StructuredConfiguration::parsePropertyValueSequence(const std::string& property_name, const Node& property_value_node, core::ConfigurableComponent& component) {
  for (const auto& nodeVal : property_value_node) {
    if (nodeVal) {
      Node propertiesNode = nodeVal["value"];
      // must insert the sequence in differently.
      const auto rawValueString = propertiesNode.getString().value();
      logger_->log_debug("Found %s=%s", property_name, rawValueString);
      if (!component.updateProperty(property_name, rawValueString)) {
        auto proc = dynamic_cast<core::Connectable*>(&component);
        if (proc) {
          logger_->log_warn("Received property %s with value %s but is not one of the properties for %s. Attempting to add as dynamic property.", property_name, rawValueString, proc->getName());
          if (!component.setDynamicProperty(property_name, rawValueString)) {
            logger_->log_warn("Unable to set the dynamic property %s with value %s", property_name, rawValueString);
          } else {
            logger_->log_warn("Dynamic property %s with value %s set", property_name, rawValueString);
          }
        }
      }
    }
  }
}

PropertyValue StructuredConfiguration::getValidatedProcessorPropertyForDefaultTypeInfo(const core::Property& property_from_processor, const Node& property_value_node) {
  using state::response::Value;
  PropertyValue defaultValue;
  defaultValue = property_from_processor.getDefaultValue();
  const std::type_index defaultType = defaultValue.getTypeInfo();
  try {
    PropertyValue coercedValue = defaultValue;
    auto int64_val = property_value_node.getInt64();
    if (defaultType == Value::INT64_TYPE && int64_val) {
      coercedValue = gsl::narrow<int64_t>(int64_val.value());
    } else if (defaultType == Value::UINT64_TYPE && int64_val) {
      coercedValue = gsl::narrow<uint64_t>(int64_val.value());
    } else if (defaultType == Value::UINT32_TYPE && int64_val) {
      coercedValue = gsl::narrow<uint32_t>(int64_val.value());
    } else if (defaultType == Value::INT_TYPE && int64_val) {
      coercedValue = gsl::narrow<int>(int64_val.value());
    } else if (defaultType == Value::BOOL_TYPE && property_value_node.getBool()) {
      coercedValue = property_value_node.getBool().value();
    } else {
      coercedValue = property_value_node.getScalarAsString().value();
    }
    return coercedValue;
  } catch (const std::exception& e) {
    logger_->log_error("Fetching property failed with an exception of %s", e.what());
    logger_->log_error("Invalid conversion for field %s. Value %s", property_from_processor.getName(), property_value_node.getDebugString());
  } catch (...) {
    logger_->log_error("Invalid conversion for field %s. Value %s", property_from_processor.getName(), property_value_node.getDebugString());
  }
  return defaultValue;
}

void StructuredConfiguration::parseSingleProperty(const std::string& property_name, const Node& property_value_node, core::ConfigurableComponent& processor) {
  core::Property myProp(property_name, "", "");
  processor.getProperty(property_name, myProp);
  const PropertyValue coercedValue = getValidatedProcessorPropertyForDefaultTypeInfo(myProp, property_value_node);
  bool property_set = false;
  try {
    property_set = processor.setProperty(myProp, coercedValue);
  } catch(const utils::internal::InvalidValueException&) {
    auto component = dynamic_cast<core::CoreComponent*>(&processor);
    if (component == nullptr) {
      logger_->log_error("processor was not a CoreComponent for property '%s'", property_name);
    } else {
      logger_->log_error("Invalid value was set for property '%s' creating component '%s'", property_name, component->getName());
    }
    throw;
  }
  if (!property_set) {
    const auto rawValueString = property_value_node.getScalarAsString().value();
    auto proc = dynamic_cast<core::Connectable*>(&processor);
    if (proc) {
      logger_->log_warn("Received property %s with value %s but is not one of the properties for %s. Attempting to add as dynamic property.", property_name, rawValueString, proc->getName());
      if (!processor.setDynamicProperty(property_name, rawValueString)) {
        logger_->log_warn("Unable to set the dynamic property %s with value %s", property_name, rawValueString);
      } else {
        logger_->log_warn("Dynamic property %s with value %s set", property_name, rawValueString);
      }
    }
  } else {
    logger_->log_debug("Property %s with value %s set", property_name, coercedValue.to_string());
  }
}

void StructuredConfiguration::parsePropertyNodeElement(const std::string& property_name, const Node& property_value_node, core::ConfigurableComponent& processor) {
  logger_->log_trace("Encountered %s", property_name);
  if (!property_value_node || property_value_node.isNull()) {
    return;
  }
  if (property_value_node.isSequence()) {
    parsePropertyValueSequence(property_name, property_value_node, processor);
  } else {
    parseSingleProperty(property_name, property_value_node, processor);
  }
}

void StructuredConfiguration::parsePropertiesNode(const Node& properties_node, core::ConfigurableComponent& component, const std::string& component_name) {
  // Treat generically as a node so we can perform inspection on entries to ensure they are populated
  logger_->log_trace("Entered %s", component_name);
  for (const auto& property_node : properties_node) {
    const auto propertyName = property_node.first.getString().value();
    const Node propertyValueNode = property_node.second;
    parsePropertyNodeElement(propertyName, propertyValueNode, component);
  }

  validateComponentProperties(component, component_name, properties_node.getPath());
}

void StructuredConfiguration::parseFunnels(const Node& node, core::ProcessGroup* parent) {
  if (!parent) {
    logger_->log_error("parseFunnels: no parent group was provided");
    return;
  }
  if (!node || !node.isSequence()) {
    return;
  }

  for (const auto& funnel_node : node) {
    std::string id = getOrGenerateId(funnel_node);

    // Default name to be same as ID
    const auto name = funnel_node[schema_.name].getString().value_or(id);

    const auto uuid = utils::Identifier::parse(id) | utils::orElse([this] {
      logger_->log_debug("Incorrect funnel UUID format.");
      throw Exception(ExceptionType::GENERAL_EXCEPTION, "Incorrect funnel UUID format.");
    });

    auto funnel = std::make_unique<minifi::Funnel>(name, uuid.value());
    logger_->log_debug("Created funnel with UUID %s and name %s", id, name);
    funnel->setScheduledState(core::RUNNING);
    funnel->setSchedulingStrategy(core::EVENT_DRIVEN);
    parent->addProcessor(std::move(funnel));
  }
}

void StructuredConfiguration::parsePorts(const flow::Node& node, core::ProcessGroup* parent, PortType port_type) {
  if (!parent) {
    logger_->log_error("parsePorts: no parent group was provided");
    return;
  }
  if (!node || !node.isSequence()) {
    return;
  }

  for (const auto& port_node : node) {
    std::string id = getOrGenerateId(port_node);

    // Default name to be same as ID
    const auto name = port_node[schema_.name].getString().value_or(id);

    const auto uuid = utils::Identifier::parse(id) | utils::orElse([this] {
      logger_->log_debug("Incorrect port UUID format.");
      throw Exception(ExceptionType::GENERAL_EXCEPTION, "Incorrect port UUID format.");
    });

    auto port = std::make_unique<Port>(name, uuid.value(), port_type);
    logger_->log_debug("Created port UUID %s and name %s", id, name);
    port->setScheduledState(core::RUNNING);
    port->setSchedulingStrategy(core::EVENT_DRIVEN);
    parent->addPort(std::move(port));
  }
}


void StructuredConfiguration::validateComponentProperties(ConfigurableComponent& component, const std::string &component_name, const std::string &section) const {
  const auto &component_properties = component.getProperties();

  // Validate required properties
  for (const auto &prop_pair : component_properties) {
    if (prop_pair.second.getRequired()) {
      if (prop_pair.second.getValue().to_string().empty()) {
        std::string reason = utils::StringUtils::join_pack("required property '", prop_pair.second.getName(), "' is not set");
        raiseComponentError(component_name, section, reason);
      } else if (!prop_pair.second.getValue().validate(prop_pair.first).valid) {
        std::string reason = utils::StringUtils::join_pack("the value '", prop_pair.first, "' is not valid for property '", prop_pair.second.getName(), "'");
        raiseComponentError(component_name, section, reason);
      }
    }
  }

  // Validate dependent properties
  for (const auto &prop_pair : component_properties) {
    const auto &dep_props = prop_pair.second.getDependentProperties();

    if (prop_pair.second.getValue().to_string().empty()) {
      continue;
    }

    for (const auto &dep_prop_key : dep_props) {
      if (component_properties.at(dep_prop_key).getValue().to_string().empty()) {
        std::string reason = utils::StringUtils::join_pack("property '", prop_pair.second.getName(),
            "' depends on property '", dep_prop_key, "' which is not set");
        raiseComponentError(component_name, section, reason);
      }
    }
  }

  // Validate mutually-exclusive properties
  for (const auto &prop_pair : component_properties) {
    const auto &excl_props = prop_pair.second.getExclusiveOfProperties();

    if (prop_pair.second.getValue().empty()) {
      continue;
    }

    for (const auto &excl_pair : excl_props) {
      utils::Regex excl_expr(excl_pair.second);
      if (utils::regexMatch(component_properties.at(excl_pair.first).getValue().to_string(), excl_expr)) {
        std::string reason = utils::StringUtils::join_pack("property '", prop_pair.second.getName(),
            "' must not be set when the value of property '", excl_pair.first, "' matches '", excl_pair.second, "'");
        raiseComponentError(component_name, section, reason);
      }
    }
  }
}

void StructuredConfiguration::raiseComponentError(const std::string &component_name, const std::string &section, const std::string &reason) const {
  std::string err_msg = "Unable to parse configuration file for component named '";
  err_msg.append(component_name);
  err_msg.append("' because " + reason);
  if (!section.empty()) {
    err_msg.append(" [in '" + section + "' section of configuration file]");
  }

  logging::LOG_ERROR(logger_) << err_msg;

  throw std::invalid_argument(err_msg);
}

std::string StructuredConfiguration::getOrGenerateId(const Node& node) {
  if (node[schema_.identifier]) {
    if (auto opt_id_str = node[schema_.identifier].getString()) {
      auto id = opt_id_str.value();
      addNewId(id);
      return id;
    }
    throw std::invalid_argument("getOrGenerateId: idField '" + utils::StringUtils::join(",", schema_.identifier) + "' is expected to contain string.");
  }

  auto id = id_generator_->generate().to_string();
  logger_->log_debug("Generating random ID: id => [%s]", id);
  return id;
}

std::string StructuredConfiguration::getRequiredIdField(const Node& node, std::string_view error_message) {
  checkRequiredField(node, schema_.identifier, error_message);
  auto id = node[schema_.identifier].getString().value();
  addNewId(id);
  return id;
}

std::string StructuredConfiguration::getOptionalField(const Node& node, const std::vector<std::string>& field_name, const std::string& default_value, const std::string& info_message) {
  std::string infoMessage = info_message;
  auto result = node[field_name];
  if (!result) {
    if (infoMessage.empty()) {
      // Build a helpful info message for the user to inform them that a default is being used
      infoMessage = "Using default value for optional field '" + utils::StringUtils::join(",", field_name) + "'";
      if (auto name = node["name"]) {
        infoMessage += "' in component named '" + name.getString().value() + "'";
      }
      infoMessage += " [in '" + node.getPath() + "' section of configuration file]: ";

      infoMessage += default_value;
    }
    logging::LOG_INFO(logger_) << infoMessage;
    return default_value;
  }

  return result.getString().value();
}

void StructuredConfiguration::addNewId(const std::string& uuid) {
  const auto [_, success] = uuids_.insert(uuid);
  if (!success) {
    throw Exception(ExceptionType::GENERAL_EXCEPTION, "UUID " + uuid + " is duplicated in the flow configuration");
  }
}

}  // namespace org::apache::nifi::minifi::core::flow
