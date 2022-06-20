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

#include "core/yaml/YamlConfiguration.h"
#include "core/yaml/CheckRequiredField.h"
#include "core/yaml/YamlConnectionParser.h"
#include "core/state/Value.h"
#include "Defaults.h"
#include "utils/TimeUtil.h"

#ifdef YAML_CONFIGURATION_USE_REGEX
#include "utils/RegexUtils.h"
#endif  // YAML_CONFIGURATION_USE_REGEX

namespace org::apache::nifi::minifi::core {

std::shared_ptr<utils::IdGenerator> YamlConfiguration::id_generator_ = utils::IdGenerator::getIdGenerator();

YamlConfiguration::YamlConfiguration(const std::shared_ptr<core::Repository>& repo, const std::shared_ptr<core::Repository>& flow_file_repo,
                                     const std::shared_ptr<core::ContentRepository>& content_repo, const std::shared_ptr<io::StreamFactory>& stream_factory,
                                     const std::shared_ptr<Configure>& configuration, const std::optional<std::string>& path,
                                     const std::shared_ptr<utils::file::FileSystem>& filesystem)
    : FlowConfiguration(repo, flow_file_repo, content_repo, stream_factory, configuration,
                        path.value_or(DEFAULT_NIFI_CONFIG_YML), filesystem),
      stream_factory_(stream_factory),
      logger_(logging::LoggerFactory<YamlConfiguration>::getLogger()) {}

std::unique_ptr<core::ProcessGroup> YamlConfiguration::parseRootProcessGroupYaml(const YAML::Node& rootFlowNode) {
  auto flowControllerNode = rootFlowNode[CONFIG_YAML_FLOW_CONTROLLER_KEY];
  auto rootGroup = parseProcessGroupYaml(flowControllerNode, rootFlowNode, true);
  this->name_ = rootGroup->getName();
  return rootGroup;
}

std::unique_ptr<core::ProcessGroup> YamlConfiguration::createProcessGroup(const YAML::Node& yamlNode, bool is_root) {
  int version = 0;

  yaml::checkRequiredField(yamlNode, "name", CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY);
  auto flowName = yamlNode["name"].as<std::string>();

  utils::Identifier uuid;
  // assignment throws on invalid uuid
  uuid = getOrGenerateId(yamlNode);

  if (yamlNode["version"]) {
    version = yamlNode["version"].as<int>();
  }

  logger_->log_debug("parseRootProcessGroup: id => [%s], name => [%s]", uuid.to_string(), flowName);
  std::unique_ptr<core::ProcessGroup> group;
  if (is_root) {
    group = FlowConfiguration::createRootProcessGroup(flowName, uuid, version);
  } else {
    group = FlowConfiguration::createSimpleProcessGroup(flowName, uuid, version);
  }

  if (yamlNode["onschedule retry interval"]) {
    auto onScheduleRetryPeriod = yamlNode["onschedule retry interval"].as<std::string>();
    logger_->log_debug("parseRootProcessGroup: onschedule retry period => [%s]", onScheduleRetryPeriod);

    auto on_schedule_retry_period_value = utils::timeutils::StringToDuration<std::chrono::milliseconds>(onScheduleRetryPeriod);
    if (on_schedule_retry_period_value.has_value() && group) {
      logger_->log_debug("parseRootProcessGroup: onschedule retry => [%" PRId64 "] ms", on_schedule_retry_period_value->count());
      group->setOnScheduleRetryPeriod(on_schedule_retry_period_value->count());
    }
  }

  return group;
}

std::unique_ptr<core::ProcessGroup> YamlConfiguration::parseProcessGroupYaml(const YAML::Node& headerNode, const YAML::Node& yamlNode, bool is_root) {
  auto group = createProcessGroup(headerNode, is_root);
  YAML::Node processorsNode = yamlNode[CONFIG_YAML_PROCESSORS_KEY];
  YAML::Node connectionsNode = yamlNode[yaml::YamlConnectionParser::CONFIG_YAML_CONNECTIONS_KEY];
  YAML::Node funnelsNode = yamlNode[CONFIG_YAML_FUNNELS_KEY];
  YAML::Node remoteProcessingGroupsNode = [&] {
    // assignment is not supported on invalid Yaml nodes
    YAML::Node candidate = yamlNode[CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY];
    if (candidate) {
      return candidate;
    }
    return yamlNode[CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY_V3];
  }();
  YAML::Node childProcessGroupNodeSeq = yamlNode["Process Groups"];

  parseProcessorNodeYaml(processorsNode, group.get());
  parseRemoteProcessGroupYaml(remoteProcessingGroupsNode, group.get());
  parseFunnelsYaml(funnelsNode, group.get());
  // parse connections last to give feedback if the source and/or destination
  // is not in the same process group
  parseConnectionYaml(connectionsNode, group.get());

  if (childProcessGroupNodeSeq && childProcessGroupNodeSeq.IsSequence()) {
    for (YAML::const_iterator it = childProcessGroupNodeSeq.begin(); it != childProcessGroupNodeSeq.end(); ++it) {
      auto childProcessGroupNode = it->as<YAML::Node>();
      group->addProcessGroup(parseProcessGroupYaml(childProcessGroupNode, childProcessGroupNode));
    }
  }
  return group;
}

std::unique_ptr<core::ProcessGroup> YamlConfiguration::getYamlRoot(const YAML::Node& rootYamlNode) {
  uuids_.clear();
  YAML::Node controllerServiceNode = rootYamlNode[CONFIG_YAML_CONTROLLER_SERVICES_KEY];
  YAML::Node provenanceReportNode = rootYamlNode[CONFIG_YAML_PROVENANCE_REPORT_KEY];

  parseControllerServices(controllerServiceNode);
  // Create the root process group
  std::unique_ptr<core::ProcessGroup> root = parseRootProcessGroupYaml(rootYamlNode);
  parseProvenanceReportingYaml(provenanceReportNode, root.get());

  // set the controller services into the root group.
  for (const auto& controller_service : controller_services_->getAllControllerServices()) {
    root->addControllerService(controller_service->getName(), controller_service);
    root->addControllerService(controller_service->getUUIDStr(), controller_service);
  }

  return root;
}

void YamlConfiguration::parseProcessorNodeYaml(const YAML::Node& processorsNode, core::ProcessGroup* parentGroup) {
  int64_t runDurationNanos = -1;
  utils::Identifier uuid;
  std::unique_ptr<core::Processor> processor;

  if (!parentGroup) {
    logger_->log_error("parseProcessNodeYaml: no parent group exists");
    return;
  }

  if (!processorsNode) {
    throw std::invalid_argument("Cannot instantiate a MiNiFi instance without a defined Processors configuration node.");
  }
  if (!processorsNode.IsSequence()) {
    throw std::invalid_argument(
        "Cannot instantiate a MiNiFi instance without a defined Processors configuration node.");
  }
  // Evaluate sequence of processors
  for (YAML::const_iterator iter = processorsNode.begin(); iter != processorsNode.end(); ++iter) {
    core::ProcessorConfig procCfg;
    const auto procNode = iter->as<YAML::Node>();

    yaml::checkRequiredField(procNode, "name", CONFIG_YAML_PROCESSORS_KEY);
    procCfg.name = procNode["name"].as<std::string>();
    procCfg.id = getOrGenerateId(procNode);

    uuid = procCfg.id;
    logger_->log_debug("parseProcessorNode: name => [%s] id => [%s]", procCfg.name, procCfg.id);
    yaml::checkRequiredField(procNode, "class", CONFIG_YAML_PROCESSORS_KEY);
    procCfg.javaClass = procNode["class"].as<std::string>();
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

    auto strategyNode = getOptionalField(procNode, "scheduling strategy", YAML::Node(DEFAULT_SCHEDULING_STRATEGY),
    CONFIG_YAML_PROCESSORS_KEY);
    procCfg.schedulingStrategy = strategyNode.as<std::string>();
    logger_->log_debug("parseProcessorNode: scheduling strategy => [%s]", procCfg.schedulingStrategy);

    auto periodNode = getOptionalField(procNode, "scheduling period", YAML::Node(DEFAULT_SCHEDULING_PERIOD_STR),
    CONFIG_YAML_PROCESSORS_KEY);

    procCfg.schedulingPeriod = periodNode.as<std::string>();
    logger_->log_debug("parseProcessorNode: scheduling period => [%s]", procCfg.schedulingPeriod);

    if (procNode["max concurrent tasks"]) {
      procCfg.maxConcurrentTasks = procNode["max concurrent tasks"].as<std::string>();
      logger_->log_debug("parseProcessorNode: max concurrent tasks => [%s]", procCfg.maxConcurrentTasks);
    }

    if (procNode["penalization period"]) {
      procCfg.penalizationPeriod = procNode["penalization period"].as<std::string>();
      logger_->log_debug("parseProcessorNode: penalization period => [%s]", procCfg.penalizationPeriod);
    }

    if (procNode["yield period"]) {
      procCfg.yieldPeriod = procNode["yield period"].as<std::string>();
      logger_->log_debug("parseProcessorNode: yield period => [%s]", procCfg.yieldPeriod);
    }

    if (procNode["run duration nanos"]) {
      procCfg.runDurationNanos = procNode["run duration nanos"].as<std::string>();
      logger_->log_debug("parseProcessorNode: run duration nanos => [%s]", procCfg.runDurationNanos);
    }

    // handle auto-terminated relationships
    if (procNode["auto-terminated relationships list"]) {
      YAML::Node autoTerminatedSequence = procNode["auto-terminated relationships list"];
      std::vector<std::string> rawAutoTerminatedRelationshipValues;
      if (autoTerminatedSequence.IsSequence() && !autoTerminatedSequence.IsNull() && autoTerminatedSequence.size() > 0) {
        for (YAML::const_iterator relIter = autoTerminatedSequence.begin(); relIter != autoTerminatedSequence.end(); ++relIter) {
          auto autoTerminatedRel = relIter->as<std::string>();
          rawAutoTerminatedRelationshipValues.push_back(autoTerminatedRel);
        }
      }
      procCfg.autoTerminatedRelationships = rawAutoTerminatedRelationshipValues;
    }

    // handle processor properties
    if (procNode["Properties"]) {
      YAML::Node propertiesNode = procNode["Properties"];
      parsePropertiesNodeYaml(propertiesNode, *processor, procCfg.name, CONFIG_YAML_PROCESSORS_KEY);
    }

    // Take care of scheduling

    if (procCfg.schedulingStrategy == "TIMER_DRIVEN" || procCfg.schedulingStrategy == "EVENT_DRIVEN") {
      if (auto scheduling_period = utils::timeutils::StringToDuration<std::chrono::nanoseconds>(procCfg.schedulingPeriod)) {
        logger_->log_debug("convert: parseProcessorNode: schedulingPeriod => [%" PRId64 "] ns", scheduling_period->count());
        processor->setSchedulingPeriodNano(*scheduling_period);
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

void YamlConfiguration::parseRemoteProcessGroupYaml(const YAML::Node& rpgNode, core::ProcessGroup* parentGroup) {
  utils::Identifier uuid;
  std::string id;

  if (!parentGroup) {
    logger_->log_error("parseRemoteProcessGroupYaml: no parent group exists");
    return;
  }

  if (!rpgNode || !rpgNode.IsSequence()) {
    return;
  }
  for (YAML::const_iterator iter = rpgNode.begin(); iter != rpgNode.end(); ++iter) {
    auto currRpgNode = iter->as<YAML::Node>();

    yaml::checkRequiredField(currRpgNode, "name", CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY);
    auto name = currRpgNode["name"].as<std::string>();
    id = getOrGenerateId(currRpgNode);

    logger_->log_debug("parseRemoteProcessGroupYaml: name => [%s], id => [%s]", name, id);

    auto urlNode = getOptionalField(currRpgNode, "url", YAML::Node(""),
    CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY);

    auto url = urlNode.as<std::string>();
    logger_->log_debug("parseRemoteProcessGroupYaml: url => [%s]", url);

    uuid = id;
    auto group = this->createRemoteProcessGroup(name, uuid);
    group->setParent(parentGroup);

    if (currRpgNode["yield period"]) {
      auto yieldPeriod = currRpgNode["yield period"].as<std::string>();
      logger_->log_debug("parseRemoteProcessGroupYaml: yield period => [%s]", yieldPeriod);

      auto yield_period_value = utils::timeutils::StringToDuration<std::chrono::milliseconds>(yieldPeriod);
      if (yield_period_value.has_value() && group) {
        logger_->log_debug("parseRemoteProcessGroupYaml: yieldPeriod => [%" PRId64 "] ms", yield_period_value->count());
        group->setYieldPeriodMsec(*yield_period_value);
      }
    }

    if (currRpgNode["timeout"]) {
      auto timeout = currRpgNode["timeout"].as<std::string>();
      logger_->log_debug("parseRemoteProcessGroupYaml: timeout => [%s]", timeout);

      auto timeout_value = utils::timeutils::StringToDuration<std::chrono::milliseconds>(timeout);
      if (timeout_value.has_value() && group) {
        logger_->log_debug("parseRemoteProcessGroupYaml: timeoutValue => [%" PRId64 "] ms", timeout_value->count());
        group->setTimeout(timeout_value->count());
      }
    }

    if (currRpgNode["local network interface"]) {
      auto interface = currRpgNode["local network interface"].as<std::string>();
      logger_->log_debug("parseRemoteProcessGroupYaml: local network interface => [%s]", interface);
      group->setInterface(interface);
    }

    if (currRpgNode["transport protocol"]) {
      auto transport_protocol = currRpgNode["transport protocol"].as<std::string>();
      logger_->log_debug("parseRemoteProcessGroupYaml: transport protocol => [%s]", transport_protocol);
      if (transport_protocol == "HTTP") {
        group->setTransportProtocol(transport_protocol);
        if (currRpgNode["proxy host"]) {
          auto http_proxy_host = currRpgNode["proxy host"].as<std::string>();
          logger_->log_debug("parseRemoteProcessGroupYaml: proxy host => [%s]", http_proxy_host);
          group->setHttpProxyHost(http_proxy_host);
          if (currRpgNode["proxy user"]) {
            auto http_proxy_username = currRpgNode["proxy user"].as<std::string>();
            logger_->log_debug("parseRemoteProcessGroupYaml: proxy user => [%s]", http_proxy_username);
            group->setHttpProxyUserName(http_proxy_username);
          }
          if (currRpgNode["proxy password"]) {
            auto http_proxy_password = currRpgNode["proxy password"].as<std::string>();
            logger_->log_debug("parseRemoteProcessGroupYaml: proxy password => [%s]", http_proxy_password);
            group->setHttpProxyPassWord(http_proxy_password);
          }
          if (currRpgNode["proxy port"]) {
            auto http_proxy_port = currRpgNode["proxy port"].as<std::string>();
            int32_t port;
            if (core::Property::StringToInt(http_proxy_port, port)) {
              logger_->log_debug("parseRemoteProcessGroupYaml: proxy port => [%d]", port);
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

    yaml::checkRequiredField(currRpgNode, "Input Ports", CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY);
    auto inputPorts = currRpgNode["Input Ports"].as<YAML::Node>();
    if (inputPorts && inputPorts.IsSequence()) {
      for (YAML::const_iterator portIter = inputPorts.begin(); portIter != inputPorts.end(); ++portIter) {
        auto currPort = portIter->as<YAML::Node>();

        this->parsePortYaml(currPort, group.get(), sitetosite::SEND);
      }  // for node
    }
    auto outputPorts = currRpgNode["Output Ports"].as<YAML::Node>();
    if (outputPorts && outputPorts.IsSequence()) {
      for (YAML::const_iterator portIter = outputPorts.begin(); portIter != outputPorts.end(); ++portIter) {
        logger_->log_debug("Got a current port, iterating...");

        auto currPort = portIter->as<YAML::Node>();

        this->parsePortYaml(currPort, group.get(), sitetosite::RECEIVE);
      }  // for node
    }
    parentGroup->addProcessGroup(std::move(group));
  }
}

void YamlConfiguration::parseProvenanceReportingYaml(const YAML::Node& reportNode, core::ProcessGroup* parentGroup) {
  utils::Identifier port_uuid;

  if (!parentGroup) {
    logger_->log_error("parseProvenanceReportingYaml: no parent group exists");
    return;
  }

  if (!reportNode || !reportNode.IsDefined() || reportNode.IsNull()) {
    logger_->log_debug("no provenance reporting task specified");
    return;
  }

  auto reportTask = createProvenanceReportTask();

  const auto node = reportNode.as<YAML::Node>();

  yaml::checkRequiredField(node, "scheduling strategy", CONFIG_YAML_PROVENANCE_REPORT_KEY);
  auto schedulingStrategyStr = node["scheduling strategy"].as<std::string>();
  yaml::checkRequiredField(node, "scheduling period", CONFIG_YAML_PROVENANCE_REPORT_KEY);
  auto schedulingPeriodStr = node["scheduling period"].as<std::string>();

  if (auto scheduling_period = utils::timeutils::StringToDuration<std::chrono::nanoseconds>(schedulingPeriodStr)) {
    logger_->log_debug("ProvenanceReportingTask schedulingPeriod %" PRId64 " ns", scheduling_period->count());
    reportTask->setSchedulingPeriodNano(*scheduling_period);
  }

  if (schedulingStrategyStr == "TIMER_DRIVEN") {
    reportTask->setSchedulingStrategy(core::TIMER_DRIVEN);
    logger_->log_debug("ProvenanceReportingTask scheduling strategy %s", schedulingStrategyStr);
  } else {
    throw std::invalid_argument("Invalid scheduling strategy " + schedulingStrategyStr);
  }

  int64_t lvalue;
  if (node["host"] && node["port"]) {
    auto hostStr = node["host"].as<std::string>();

    auto portStr = node["port"].as<std::string>();
    if (core::Property::StringToInt(portStr, lvalue) && !hostStr.empty()) {
      logger_->log_debug("ProvenanceReportingTask port %" PRId64, lvalue);
      std::string url = hostStr + ":" + portStr;
      reportTask->setURL(url);
    }
  }

  if (node["url"]) {
    auto urlStr = node["url"].as<std::string>();
    if (!urlStr.empty()) {
      reportTask->setURL(urlStr);
      logger_->log_debug("ProvenanceReportingTask URL %s", urlStr);
    }
  }
  yaml::checkRequiredField(node, "port uuid", CONFIG_YAML_PROVENANCE_REPORT_KEY);
  auto portUUIDStr = node["port uuid"].as<std::string>();
  yaml::checkRequiredField(node, "batch size", CONFIG_YAML_PROVENANCE_REPORT_KEY);
  auto batchSizeStr = node["batch size"].as<std::string>();

  logger_->log_debug("ProvenanceReportingTask port uuid %s", portUUIDStr);
  port_uuid = portUUIDStr;
  reportTask->setPortUUID(port_uuid);

  if (core::Property::StringToInt(batchSizeStr, lvalue)) {
    reportTask->setBatchSize(gsl::narrow<int>(lvalue));
  }

  reportTask->initialize();

  // add processor to parent
  reportTask->setScheduledState(core::RUNNING);
  parentGroup->addProcessor(std::move(reportTask));
}

void YamlConfiguration::parseControllerServices(const YAML::Node& controllerServicesNode) {
  if (!controllerServicesNode || !controllerServicesNode.IsSequence()) {
    return;
  }
  for (const auto& iter : controllerServicesNode) {
    const auto controllerServiceNode = iter.as<YAML::Node>();
    try {
      yaml::checkRequiredField(controllerServiceNode, "name", CONFIG_YAML_CONTROLLER_SERVICES_KEY);

      auto type = yaml::getRequiredField(controllerServiceNode, std::vector<std::string>{"class", "type"}, CONFIG_YAML_CONTROLLER_SERVICES_KEY);
      logger_->log_debug("Using type %s for controller service node", type);

      std::string fullType = type;
      auto lastOfIdx = type.find_last_of('.');
      if (lastOfIdx != std::string::npos) {
        lastOfIdx++;  // if a value is found, increment to move beyond the .
        type = type.substr(lastOfIdx);
      }

      auto name = controllerServiceNode["name"].as<std::string>();
      auto id = getRequiredIdField(controllerServiceNode, CONFIG_YAML_CONTROLLER_SERVICES_KEY);

      utils::Identifier uuid;
      uuid = id;
      std::shared_ptr<core::controller::ControllerServiceNode> controller_service_node = createControllerService(type, fullType, name, uuid);
      if (nullptr != controller_service_node) {
        logger_->log_debug("Created Controller Service with UUID %s and name %s", id, name);
        controller_service_node->initialize();
        YAML::Node propertiesNode = controllerServiceNode["Properties"];
        // we should propagate properties to the node and to the implementation
        parsePropertiesNodeYaml(propertiesNode, *controller_service_node, name, CONFIG_YAML_CONTROLLER_SERVICES_KEY);
        if (auto controllerServiceImpl = controller_service_node->getControllerServiceImplementation(); controllerServiceImpl) {
          parsePropertiesNodeYaml(propertiesNode, *controllerServiceImpl, name, CONFIG_YAML_CONTROLLER_SERVICES_KEY);
        }
      } else {
        logger_->log_debug("Could not locate %s", type);
      }
      controller_services_->put(id, controller_service_node);
      controller_services_->put(name, controller_service_node);
    } catch (YAML::InvalidNode &) {
      throw Exception(ExceptionType::GENERAL_EXCEPTION, "Name, id, and class must be specified for controller services");
    }
  }
}

void YamlConfiguration::parseConnectionYaml(const YAML::Node& connectionsNode, core::ProcessGroup* parent) {
  if (!parent) {
    logger_->log_error("parseProcessNode: no parent group was provided");
    return;
  }
  if (!connectionsNode || !connectionsNode.IsSequence()) {
    return;
  }

  for (YAML::const_iterator iter = connectionsNode.begin(); iter != connectionsNode.end(); ++iter) {
    const auto connectionNode = iter->as<YAML::Node>();

    // Configure basic connection
    const std::string id = getOrGenerateId(connectionNode);

    // Default name to be same as ID
    // If name is specified in configuration, use the value
    const auto name = connectionNode["name"].as<std::string>(id);

    const auto uuid = utils::Identifier::parse(id) | utils::orElse([this] {
      logger_->log_debug("Incorrect connection UUID format.");
      throw Exception(ExceptionType::GENERAL_EXCEPTION, "Incorrect connection UUID format.");
    });

    auto connection = createConnection(name, uuid.value());
    logger_->log_debug("Created connection with UUID %s and name %s", id, name);
    const yaml::YamlConnectionParser connectionParser(connectionNode, name, gsl::not_null<core::ProcessGroup*>{ parent }, logger_);
    connectionParser.configureConnectionSourceRelationshipsFromYaml(*connection);
    connection->setMaxQueueSize(connectionParser.getWorkQueueSizeFromYaml());
    connection->setMaxQueueDataSize(connectionParser.getWorkQueueDataSizeFromYaml());
    connection->setSourceUUID(connectionParser.getSourceUUIDFromYaml());
    connection->setDestinationUUID(connectionParser.getDestinationUUIDFromYaml());
    connection->setFlowExpirationDuration(connectionParser.getFlowFileExpirationFromYaml());
    connection->setDropEmptyFlowFiles(connectionParser.getDropEmptyFromYaml());

    parent->addConnection(std::move(connection));
  }
}

void YamlConfiguration::parsePortYaml(const YAML::Node& portNode, core::ProcessGroup* parent, sitetosite::TransferDirection direction) {
  utils::Identifier uuid;

  if (!parent) {
    logger_->log_error("parseProcessNode: no parent group existed");
    return;
  }

  const auto inputPortsObj = portNode.as<YAML::Node>();

  // Check for required fields
  yaml::checkRequiredField(inputPortsObj, "name", CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY);
  auto nameStr = inputPortsObj["name"].as<std::string>();
  auto portId = getRequiredIdField(inputPortsObj, CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY,
    "The field 'id' is required for "
    "the port named '" + nameStr + "' in the YAML Config. If this port "
    "is an input port for a NiFi Remote Process Group, the port "
    "id should match the corresponding id specified in the NiFi configuration. "
    "This is a UUID of the format XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX.");
  uuid = portId;

  auto port = std::make_unique<minifi::RemoteProcessorGroupPort>(
          stream_factory_, nameStr, parent->getURL(), this->configuration_, uuid);
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
  const auto nodeVal = portNode.as<YAML::Node>();
  YAML::Node propertiesNode = nodeVal["Properties"];
  parsePropertiesNodeYaml(propertiesNode, *port, nameStr, CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY);

  // add processor to parent
  auto& processor = *port;
  parent->addProcessor(std::move(port));
  processor.setScheduledState(core::RUNNING);

  if (inputPortsObj["max concurrent tasks"]) {
    auto rawMaxConcurrentTasks = inputPortsObj["max concurrent tasks"].as<std::string>();
    int32_t maxConcurrentTasks;
    if (core::Property::StringToInt(rawMaxConcurrentTasks, maxConcurrentTasks)) {
      processor.setMaxConcurrentTasks(maxConcurrentTasks);
    }
    logger_->log_debug("parseProcessorNode: maxConcurrentTasks => [%d]", maxConcurrentTasks);
    processor.setMaxConcurrentTasks(maxConcurrentTasks);
  }
}

void YamlConfiguration::parsePropertyValueSequence(const std::string& propertyName, const YAML::Node& propertyValueNode, core::ConfigurableComponent& component) {
  for (const auto& iter : propertyValueNode) {
    if (iter.IsDefined()) {
      const auto nodeVal = iter.as<YAML::Node>();
      YAML::Node propertiesNode = nodeVal["value"];
      // must insert the sequence in differently.
      const auto rawValueString = propertiesNode.as<std::string>();
      logger_->log_debug("Found %s=%s", propertyName, rawValueString);
      if (!component.updateProperty(propertyName, rawValueString)) {
        auto proc = dynamic_cast<core::Connectable*>(&component);
        if (proc) {
          logger_->log_warn("Received property %s with value %s but is not one of the properties for %s. Attempting to add as dynamic property.", propertyName, rawValueString, proc->getName());
          if (!component.setDynamicProperty(propertyName, rawValueString)) {
            logger_->log_warn("Unable to set the dynamic property %s with value %s", propertyName, rawValueString);
          } else {
            logger_->log_warn("Dynamic property %s with value %s set", propertyName, rawValueString);
          }
        }
      }
    }
  }
}

PropertyValue YamlConfiguration::getValidatedProcessorPropertyForDefaultTypeInfo(const core::Property& propertyFromProcessor, const YAML::Node& propertyValueNode) {
  PropertyValue defaultValue;
  defaultValue = propertyFromProcessor.getDefaultValue();
  const std::type_index defaultType = defaultValue.getTypeInfo();
  try {
    PropertyValue coercedValue = defaultValue;
    if (defaultType == typeid(int64_t)) {
      coercedValue = propertyValueNode.as<int64_t>();
    } else if (defaultType == typeid(uint64_t)) {
      uint64_t integer_value;
      if (YAML::convert<uint64_t>::decode(propertyValueNode, integer_value)) {
        coercedValue = integer_value;
      } else {
        coercedValue = propertyValueNode.as<std::string>();
      }
    } else if (defaultType == typeid(int)) {
      coercedValue = propertyValueNode.as<int>();
    } else if (defaultType == typeid(bool)) {
      coercedValue = propertyValueNode.as<bool>();
    } else {
      coercedValue = propertyValueNode.as<std::string>();
    }
    return coercedValue;
  } catch (const std::exception& e) {
    logger_->log_error("Fetching property failed with an exception of %s", e.what());
    logger_->log_error("Invalid conversion for field %s. Value %s", propertyFromProcessor.getName(), propertyValueNode.as<std::string>());
  } catch (...) {
    logger_->log_error("Invalid conversion for field %s. Value %s", propertyFromProcessor.getName(), propertyValueNode.as<std::string>());
  }
  return defaultValue;
}

void YamlConfiguration::parseSingleProperty(const std::string& propertyName, const YAML::Node& propertyValueNode, core::ConfigurableComponent& processor) {
  core::Property myProp(propertyName, "", "");
  processor.getProperty(propertyName, myProp);
  const PropertyValue coercedValue = getValidatedProcessorPropertyForDefaultTypeInfo(myProp, propertyValueNode);
  bool property_set = false;
  try {
    property_set = processor.setProperty(myProp, coercedValue);
  } catch(const utils::internal::InvalidValueException&) {
    auto component = dynamic_cast<core::CoreComponent*>(&processor);
    if (component == nullptr) {
      logger_->log_error("processor was not a CoreComponent for property '%s'", propertyName);
    } else {
      logger_->log_error("Invalid value was set for property '%s' creating component '%s'", propertyName, component->getName());
    }
    throw;
  }
  const auto rawValueString = propertyValueNode.as<std::string>();
  if (!property_set) {
    auto proc = dynamic_cast<core::Connectable*>(&processor);
    if (proc) {
      logger_->log_warn("Received property %s with value %s but is not one of the properties for %s. Attempting to add as dynamic property.", propertyName, rawValueString, proc->getName());
      if (!processor.setDynamicProperty(propertyName, rawValueString)) {
        logger_->log_warn("Unable to set the dynamic property %s with value %s", propertyName, rawValueString);
      } else {
        logger_->log_warn("Dynamic property %s with value %s set", propertyName, rawValueString);
      }
    }
  } else {
    logger_->log_debug("Property %s with value %s set", propertyName, rawValueString);
  }
}

void YamlConfiguration::parsePropertyNodeElement(const std::string& propertyName, const YAML::Node& propertyValueNode, core::ConfigurableComponent& processor) {
  logger_->log_trace("Encountered %s", propertyName);
  if (propertyValueNode.IsNull() || !propertyValueNode.IsDefined()) {
    return;
  }
  if (propertyValueNode.IsSequence()) {
    parsePropertyValueSequence(propertyName, propertyValueNode, processor);
  } else {
    parseSingleProperty(propertyName, propertyValueNode, processor);
  }
}

void YamlConfiguration::parsePropertiesNodeYaml(const YAML::Node& propertiesNode, core::ConfigurableComponent& component, const std::string& component_name,
    const std::string& yaml_section) {
  // Treat generically as a YAML node so we can perform inspection on entries to ensure they are populated
  logger_->log_trace("Entered %s", component_name);
  for (const auto& propertyElem : propertiesNode) {
    const auto propertyName = propertyElem.first.as<std::string>();
    const YAML::Node propertyValueNode = propertyElem.second;
    parsePropertyNodeElement(propertyName, propertyValueNode, component);
  }

  validateComponentProperties(component, component_name, yaml_section);
}

void YamlConfiguration::parseFunnelsYaml(const YAML::Node& node, core::ProcessGroup* parent) {
  if (!parent) {
    logger_->log_error("parseFunnelsYaml: no parent group was provided");
    return;
  }
  if (!node || !node.IsSequence()) {
    return;
  }

  for (const auto& element : node) {
    const auto funnel_node = element.as<YAML::Node>();

    std::string id = getOrGenerateId(funnel_node);

    // Default name to be same as ID
    const auto name = funnel_node["name"].as<std::string>(id);

    const auto uuid = utils::Identifier::parse(id) | utils::orElse([this] {
      logger_->log_debug("Incorrect funnel UUID format.");
      throw Exception(ExceptionType::GENERAL_EXCEPTION, "Incorrect funnel UUID format.");
    });

    auto funnel = std::make_unique<core::Funnel>(name, uuid.value());
    logger_->log_debug("Created funnel with UUID %s and name %s", id, name);
    funnel->setScheduledState(core::RUNNING);
    funnel->setSchedulingStrategy(core::EVENT_DRIVEN);
    parent->addProcessor(std::move(funnel));
  }
}

void YamlConfiguration::validateComponentProperties(ConfigurableComponent& component, const std::string &component_name, const std::string &yaml_section) const {
  const auto &component_properties = component.getProperties();

  // Validate required properties
  for (const auto &prop_pair : component_properties) {
    if (prop_pair.second.getRequired()) {
      if (prop_pair.second.getValue().to_string().empty()) {
        std::string reason = utils::StringUtils::join_pack("required property '", prop_pair.second.getName(), "' is not set");
        raiseComponentError(component_name, yaml_section, reason);
      } else if (!prop_pair.second.getValue().validate(prop_pair.first).valid()) {
        std::string reason = utils::StringUtils::join_pack("the value '", prop_pair.first, "' is not valid for property '", prop_pair.second.getName(), "'");
        raiseComponentError(component_name, yaml_section, reason);
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
        raiseComponentError(component_name, yaml_section, reason);
      }
    }
  }

#ifdef YAML_CONFIGURATION_USE_REGEX
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
        raiseComponentError(component_name, yaml_section, reason);
      }
    }
  }

  // Validate regex properties
  for (const auto &prop_pair : component_properties) {
    const auto &prop_regex_str = prop_pair.second.getValidRegex();

    if (!prop_regex_str.empty()) {
      utils::Regex prop_regex(prop_regex_str);
      if (!utils::regexMatch(prop_pair.second.getValue().to_string(), prop_regex)) {
        std::string reason = utils::StringUtils::join_pack("property '", prop_pair.second.getName(), "' does not match validation pattern '", prop_regex_str, "'");
        raiseComponentError(component_name, yaml_section, reason);
      }
    }
  }
#else
  logging::LOG_INFO(logger_) << "Validation of mutally-exclusive properties is disabled in this build.";
  logging::LOG_INFO(logger_) << "Regex validation of properties is not available in this build.";
#endif  // YAML_CONFIGURATION_USE_REGEX
}

void YamlConfiguration::raiseComponentError(const std::string &component_name, const std::string &yaml_section, const std::string &reason) const {
  std::string err_msg = "Unable to parse configuration file for component named '";
  err_msg.append(component_name);
  err_msg.append("' because " + reason);
  if (!yaml_section.empty()) {
    err_msg.append(" [in '" + yaml_section + "' section of configuration file]");
  }

  logging::LOG_ERROR(logger_) << err_msg;

  throw std::invalid_argument(err_msg);
}

std::string YamlConfiguration::getOrGenerateId(const YAML::Node& yamlNode, const std::string& idField) {
  std::string id;
  auto node = yamlNode.as<YAML::Node>();

  if (node[idField]) {
    if (YAML::NodeType::Scalar == node[idField].Type()) {
      id = node[idField].as<std::string>();
      addNewId(id);
      return id;
    }
    throw std::invalid_argument("getOrGenerateId: idField is expected to reference YAML::Node of YAML::NodeType::Scalar.");
  }

  id = id_generator_->generate().to_string();
  logger_->log_debug("Generating random ID: id => [%s]", id);
  return id;
}

std::string YamlConfiguration::getRequiredIdField(const YAML::Node& yaml_node, std::string_view yaml_section, std::string_view error_message) {
  yaml::checkRequiredField(yaml_node, "id", yaml_section, error_message);
  auto id = yaml_node["id"].as<std::string>();
  addNewId(id);
  return id;
}

YAML::Node YamlConfiguration::getOptionalField(const YAML::Node& yamlNode, const std::string& fieldName, const YAML::Node& defaultValue, const std::string& yamlSection,
                                               const std::string& providedInfoMessage) {
  std::string infoMessage = providedInfoMessage;
  auto result = yamlNode.as<YAML::Node>()[fieldName];
  if (!result) {
    if (infoMessage.empty()) {
      // Build a helpful info message for the user to inform them that a default is being used
      infoMessage =
          yamlNode.as<YAML::Node>()["name"] ?
              "Using default value for optional field '" + fieldName + "' in component named '" + yamlNode.as<YAML::Node>()["name"].as<std::string>() + "'" :
              "Using default value for optional field '" + fieldName + "' ";
      if (!yamlSection.empty()) {
        infoMessage += " [in '" + yamlSection + "' section of configuration file]: ";
      }

      infoMessage += defaultValue.as<std::string>();
    }
    logging::LOG_INFO(logger_) << infoMessage;
    result = defaultValue;
  }

  return result;
}

void YamlConfiguration::addNewId(const std::string& uuid) {
  const auto [_, success] = uuids_.insert(uuid);
  if (!success) {
    throw Exception(ExceptionType::GENERAL_EXCEPTION, "UUID " + uuid + " is duplicated in the flow configuration");
  }
}

}  // namespace org::apache::nifi::minifi::core
