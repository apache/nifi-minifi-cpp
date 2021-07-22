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

#ifdef YAML_CONFIGURATION_USE_REGEX
#include <regex>
#endif  // YAML_CONFIGURATION_USE_REGEX

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

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

  yaml::checkRequiredField(&yamlNode, "name", logger_,
  CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY);
  std::string flowName = yamlNode["name"].as<std::string>();

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
    int64_t onScheduleRetryPeriodValue = -1;
    std::string onScheduleRetryPeriod = yamlNode["onschedule retry interval"].as<std::string>();
    logger_->log_debug("parseRootProcessGroup: onschedule retry period => [%s]", onScheduleRetryPeriod);

    core::TimeUnit unit;

    if (core::Property::StringToTime(onScheduleRetryPeriod, onScheduleRetryPeriodValue, unit)
        && core::Property::ConvertTimeUnitToMS(onScheduleRetryPeriodValue, unit, onScheduleRetryPeriodValue)
        && group) {
      logger_->log_debug("parseRootProcessGroup: onschedule retry => [%" PRId64 "] ms", onScheduleRetryPeriodValue);
      group->setOnScheduleRetryPeriod(onScheduleRetryPeriodValue);
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
      YAML::Node childProcessGroupNode = it->as<YAML::Node>();
      group->addProcessGroup(parseProcessGroupYaml(childProcessGroupNode, childProcessGroupNode));
    }
  }
  return group;
}

std::unique_ptr<core::ProcessGroup> YamlConfiguration::getYamlRoot(const YAML::Node& rootYamlNode) {
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
  int64_t schedulingPeriod = -1;
  int64_t penalizationPeriod = -1;
  int64_t yieldPeriod = -1;
  int64_t runDurationNanos = -1;
  utils::Identifier uuid;
  std::shared_ptr<core::Processor> processor = nullptr;

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
    YAML::Node procNode = iter->as<YAML::Node>();

    yaml::checkRequiredField(&procNode, "name", logger_,
    CONFIG_YAML_PROCESSORS_KEY);
    procCfg.name = procNode["name"].as<std::string>();
    procCfg.id = getOrGenerateId(procNode);

    uuid = procCfg.id.c_str();
    logger_->log_debug("parseProcessorNode: name => [%s] id => [%s]", procCfg.name, procCfg.id);
    yaml::checkRequiredField(&procNode, "class", logger_, CONFIG_YAML_PROCESSORS_KEY);
    procCfg.javaClass = procNode["class"].as<std::string>();
    logger_->log_debug("parseProcessorNode: class => [%s]", procCfg.javaClass);

    // Determine the processor name only from the Java class
    auto lastOfIdx = procCfg.javaClass.find_last_of(".");
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
          std::string autoTerminatedRel = relIter->as<std::string>();
          rawAutoTerminatedRelationshipValues.push_back(autoTerminatedRel);
        }
      }
      procCfg.autoTerminatedRelationships = rawAutoTerminatedRelationshipValues;
    }

    // handle processor properties
    if (procNode["Properties"]) {
      YAML::Node propertiesNode = procNode["Properties"];
      parsePropertiesNodeYaml(propertiesNode, processor, procCfg.name, CONFIG_YAML_PROCESSORS_KEY);
    }

    // Take care of scheduling

    core::TimeUnit unit;

    if (procCfg.schedulingStrategy == "TIMER_DRIVEN" || procCfg.schedulingStrategy == "EVENT_DRIVEN") {
      if (core::Property::StringToTime(procCfg.schedulingPeriod, schedulingPeriod, unit) && core::Property::ConvertTimeUnitToNS(schedulingPeriod, unit, schedulingPeriod)) {
        logger_->log_debug("convert: parseProcessorNode: schedulingPeriod => [%" PRId64 "] ns", schedulingPeriod);
        processor->setSchedulingPeriodNano(schedulingPeriod);
      }
    } else {
      processor->setCronPeriod(procCfg.schedulingPeriod);
    }

    if (core::Property::StringToTime(procCfg.penalizationPeriod, penalizationPeriod, unit) && core::Property::ConvertTimeUnitToMS(penalizationPeriod, unit, penalizationPeriod)) {
      logger_->log_debug("convert: parseProcessorNode: penalizationPeriod => [%" PRId64 "] ms", penalizationPeriod);
      processor->setPenalizationPeriod(std::chrono::milliseconds{penalizationPeriod});
    }

    if (core::Property::StringToTime(procCfg.yieldPeriod, yieldPeriod, unit) && core::Property::ConvertTimeUnitToMS(yieldPeriod, unit, yieldPeriod)) {
      logger_->log_debug("convert: parseProcessorNode: yieldPeriod => [%" PRId64 "] ms", yieldPeriod);
      processor->setYieldPeriodMsec(yieldPeriod);
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
      processor->setRunDurationNano((uint64_t) runDurationNanos);
    }

    std::set<core::Relationship> autoTerminatedRelationships;
    for (auto &&relString : procCfg.autoTerminatedRelationships) {
      core::Relationship relationship(relString, "");
      logger_->log_debug("parseProcessorNode: autoTerminatedRelationship  => [%s]", relString);
      autoTerminatedRelationships.insert(relationship);
    }

    processor->setAutoTerminatedRelationships(autoTerminatedRelationships);

    parentGroup->addProcessor(processor);
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
    YAML::Node currRpgNode = iter->as<YAML::Node>();

    yaml::checkRequiredField(&currRpgNode, "name", logger_,
    CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY);
    auto name = currRpgNode["name"].as<std::string>();
    id = getOrGenerateId(currRpgNode);

    logger_->log_debug("parseRemoteProcessGroupYaml: name => [%s], id => [%s]", name, id);

    auto urlNode = getOptionalField(currRpgNode, "url", YAML::Node(""),
    CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY);

    std::string url = urlNode.as<std::string>();
    logger_->log_debug("parseRemoteProcessGroupYaml: url => [%s]", url);

    core::TimeUnit unit;
    int64_t timeoutValue = -1;
    int64_t yieldPeriodValue = -1;
    uuid = id;
    auto group = this->createRemoteProcessGroup(name, uuid);
    group->setParent(parentGroup);

    if (currRpgNode["yield period"]) {
      std::string yieldPeriod = currRpgNode["yield period"].as<std::string>();
      logger_->log_debug("parseRemoteProcessGroupYaml: yield period => [%s]", yieldPeriod);

      if (core::Property::StringToTime(yieldPeriod, yieldPeriodValue, unit) && core::Property::ConvertTimeUnitToMS(yieldPeriodValue, unit, yieldPeriodValue) && group) {
        logger_->log_debug("parseRemoteProcessGroupYaml: yieldPeriod => [%" PRId64 "] ms", yieldPeriodValue);
        group->setYieldPeriodMsec(yieldPeriodValue);
      }
    }

    if (currRpgNode["timeout"]) {
      std::string timeout = currRpgNode["timeout"].as<std::string>();
      logger_->log_debug("parseRemoteProcessGroupYaml: timeout => [%s]", timeout);

      if (core::Property::StringToTime(timeout, timeoutValue, unit) && core::Property::ConvertTimeUnitToMS(timeoutValue, unit, timeoutValue) && group) {
        logger_->log_debug("parseRemoteProcessGroupYaml: timeoutValue => [%" PRId64 "] ms", timeoutValue);
        group->setTimeOut(timeoutValue);
      }
    }

    if (currRpgNode["local network interface"]) {
      std::string interface = currRpgNode["local network interface"].as<std::string>();
      logger_->log_debug("parseRemoteProcessGroupYaml: local network interface => [%s]", interface);
      group->setInterface(interface);
    }

    if (currRpgNode["transport protocol"]) {
      std::string transport_protocol = currRpgNode["transport protocol"].as<std::string>();
      logger_->log_debug("parseRemoteProcessGroupYaml: transport protocol => [%s]", transport_protocol);
      if (transport_protocol == "HTTP") {
        group->setTransportProtocol(transport_protocol);
        if (currRpgNode["proxy host"]) {
          std::string http_proxy_host = currRpgNode["proxy host"].as<std::string>();
          logger_->log_debug("parseRemoteProcessGroupYaml: proxy host => [%s]", http_proxy_host);
          group->setHttpProxyHost(http_proxy_host);
          if (currRpgNode["proxy user"]) {
            std::string http_proxy_username = currRpgNode["proxy user"].as<std::string>();
            logger_->log_debug("parseRemoteProcessGroupYaml: proxy user => [%s]", http_proxy_username);
            group->setHttpProxyUserName(http_proxy_username);
          }
          if (currRpgNode["proxy password"]) {
            std::string http_proxy_password = currRpgNode["proxy password"].as<std::string>();
            logger_->log_debug("parseRemoteProcessGroupYaml: proxy password => [%s]", http_proxy_password);
            group->setHttpProxyPassWord(http_proxy_password);
          }
          if (currRpgNode["proxy port"]) {
            std::string http_proxy_port = currRpgNode["proxy port"].as<std::string>();
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

    yaml::checkRequiredField(&currRpgNode, "Input Ports", logger_,
    CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY);
    YAML::Node inputPorts = currRpgNode["Input Ports"].as<YAML::Node>();
    if (inputPorts && inputPorts.IsSequence()) {
      for (YAML::const_iterator portIter = inputPorts.begin(); portIter != inputPorts.end(); ++portIter) {
        YAML::Node currPort = portIter->as<YAML::Node>();

        this->parsePortYaml(currPort, group.get(), sitetosite::SEND);
      }  // for node
    }
    YAML::Node outputPorts = currRpgNode["Output Ports"].as<YAML::Node>();
    if (outputPorts && outputPorts.IsSequence()) {
      for (YAML::const_iterator portIter = outputPorts.begin(); portIter != outputPorts.end(); ++portIter) {
        logger_->log_debug("Got a current port, iterating...");

        YAML::Node currPort = portIter->as<YAML::Node>();

        this->parsePortYaml(currPort, group.get(), sitetosite::RECEIVE);
      }  // for node
    }
    parentGroup->addProcessGroup(std::move(group));
  }
}

void YamlConfiguration::parseProvenanceReportingYaml(const YAML::Node& reportNode, core::ProcessGroup* parentGroup) {
  utils::Identifier port_uuid;
  int64_t schedulingPeriod = -1;

  if (!parentGroup) {
    logger_->log_error("parseProvenanceReportingYaml: no parent group exists");
    return;
  }

  if (!reportNode || !reportNode.IsDefined() || reportNode.IsNull()) {
    logger_->log_debug("no provenance reporting task specified");
    return;
  }

  std::shared_ptr<core::Processor> processor = nullptr;
  processor = createProvenanceReportTask();
  std::shared_ptr<core::reporting::SiteToSiteProvenanceReportingTask> reportTask = std::static_pointer_cast<core::reporting::SiteToSiteProvenanceReportingTask>(processor);

  YAML::Node node = reportNode.as<YAML::Node>();

  yaml::checkRequiredField(&node, "scheduling strategy", logger_,
  CONFIG_YAML_PROVENANCE_REPORT_KEY);
  auto schedulingStrategyStr = node["scheduling strategy"].as<std::string>();
  yaml::checkRequiredField(&node, "scheduling period", logger_,
  CONFIG_YAML_PROVENANCE_REPORT_KEY);
  auto schedulingPeriodStr = node["scheduling period"].as<std::string>();

  core::TimeUnit unit;
  if (core::Property::StringToTime(schedulingPeriodStr, schedulingPeriod, unit) && core::Property::ConvertTimeUnitToNS(schedulingPeriod, unit, schedulingPeriod)) {
    logger_->log_debug("ProvenanceReportingTask schedulingPeriod %" PRId64 " ns", schedulingPeriod);
    processor->setSchedulingPeriodNano(schedulingPeriod);
  }

  if (schedulingStrategyStr == "TIMER_DRIVEN") {
    processor->setSchedulingStrategy(core::TIMER_DRIVEN);
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
  yaml::checkRequiredField(&node, "port uuid", logger_, CONFIG_YAML_PROVENANCE_REPORT_KEY);
  auto portUUIDStr = node["port uuid"].as<std::string>();
  yaml::checkRequiredField(&node, "batch size", logger_, CONFIG_YAML_PROVENANCE_REPORT_KEY);
  auto batchSizeStr = node["batch size"].as<std::string>();

  logger_->log_debug("ProvenanceReportingTask port uuid %s", portUUIDStr);
  port_uuid = portUUIDStr;
  reportTask->setPortUUID(port_uuid);

  if (core::Property::StringToInt(batchSizeStr, lvalue)) {
    reportTask->setBatchSize(gsl::narrow<int>(lvalue));
  }

  reportTask->initialize();

  // add processor to parent
  parentGroup->addProcessor(processor);
  processor->setScheduledState(core::RUNNING);
}

void YamlConfiguration::parseControllerServices(const YAML::Node& controllerServicesNode) {
  if (!controllerServicesNode || !controllerServicesNode.IsSequence()) {
    return;
  }
  for (const auto& iter : controllerServicesNode) {
    YAML::Node controllerServiceNode = iter.as<YAML::Node>();
    try {
      yaml::checkRequiredField(&controllerServiceNode, "name", logger_,
      CONFIG_YAML_CONTROLLER_SERVICES_KEY);
      yaml::checkRequiredField(&controllerServiceNode, "id", logger_,
      CONFIG_YAML_CONTROLLER_SERVICES_KEY);
      std::string type = "";

      try {
        yaml::checkRequiredField(&controllerServiceNode, "class", logger_, CONFIG_YAML_CONTROLLER_SERVICES_KEY);
        type = controllerServiceNode["class"].as<std::string>();
      } catch (const std::invalid_argument &) {
        yaml::checkRequiredField(&controllerServiceNode, "type", logger_, CONFIG_YAML_CONTROLLER_SERVICES_KEY);
        type = controllerServiceNode["type"].as<std::string>();
        logger_->log_debug("Using type %s for controller service node", type);
      }
      std::string fullType = type;
      auto lastOfIdx = type.find_last_of(".");
      if (lastOfIdx != std::string::npos) {
        lastOfIdx++;  // if a value is found, increment to move beyond the .
        type = type.substr(lastOfIdx);
      }

      auto name = controllerServiceNode["name"].as<std::string>();
      auto id = controllerServiceNode["id"].as<std::string>();

      utils::Identifier uuid;
      uuid = id;
      auto controller_service_node = createControllerService(type, fullType, name, uuid);
      if (nullptr != controller_service_node) {
        logger_->log_debug("Created Controller Service with UUID %s and name %s", id, name);
        controller_service_node->initialize();
        YAML::Node propertiesNode = controllerServiceNode["Properties"];
        // we should propagate properties to the node and to the implementation
        parsePropertiesNodeYaml(propertiesNode, std::static_pointer_cast<core::ConfigurableComponent>(controller_service_node), name,
        CONFIG_YAML_CONTROLLER_SERVICES_KEY);
        if (controller_service_node->getControllerServiceImplementation() != nullptr) {
          parsePropertiesNodeYaml(propertiesNode, std::static_pointer_cast<core::ConfigurableComponent>(controller_service_node->getControllerServiceImplementation()), name,
          CONFIG_YAML_CONTROLLER_SERVICES_KEY);
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
    YAML::Node connectionNode = iter->as<YAML::Node>();
    std::shared_ptr<minifi::Connection> connection = nullptr;

    // Configure basic connection
    std::string id = getOrGenerateId(connectionNode);

    // Default name to be same as ID
    // If name is specified in configuration, use the value
    std::string name = connectionNode["name"].as<std::string>(id);

    const auto uuid = utils::Identifier::parse(id) | utils::orElse([this] {
      logger_->log_debug("Incorrect connection UUID format.");
      throw Exception(ExceptionType::GENERAL_EXCEPTION, "Incorrect connection UUID format.");
    });

    connection = createConnection(name, uuid.value());
    logger_->log_debug("Created connection with UUID %s and name %s", id, name);
    const yaml::YamlConnectionParser connectionParser(connectionNode, name, gsl::not_null<core::ProcessGroup*>{ parent }, logger_);
    connectionParser.configureConnectionSourceRelationshipsFromYaml(connection);
    connection->setMaxQueueSize(connectionParser.getWorkQueueSizeFromYaml());
    connection->setMaxQueueDataSize(connectionParser.getWorkQueueDataSizeFromYaml());
    connection->setSourceUUID(connectionParser.getSourceUUIDFromYaml());
    connection->setDestinationUUID(connectionParser.getDestinationUUIDFromYaml());
    connection->setFlowExpirationDuration(connectionParser.getFlowFileExpirationFromYaml());
    connection->setDropEmptyFlowFiles(connectionParser.getDropEmptyFromYaml());

    parent->addConnection(connection);
  }
}

void YamlConfiguration::parsePortYaml(const YAML::Node& portNode, core::ProcessGroup* parent, sitetosite::TransferDirection direction) {
  utils::Identifier uuid;
  std::shared_ptr<core::Processor> processor = NULL;
  std::shared_ptr<minifi::RemoteProcessorGroupPort> port = NULL;

  if (!parent) {
    logger_->log_error("parseProcessNode: no parent group existed");
    return;
  }

  YAML::Node inputPortsObj = portNode.as<YAML::Node>();

  // Check for required fields
  yaml::checkRequiredField(&inputPortsObj, "name", logger_,
  CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY);
  auto nameStr = inputPortsObj["name"].as<std::string>();
  yaml::checkRequiredField(&inputPortsObj, "id", logger_,
  CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY,
                     "The field 'id' is required for "
                         "the port named '" + nameStr + "' in the YAML Config. If this port "
                         "is an input port for a NiFi Remote Process Group, the port "
                         "id should match the corresponding id specified in the NiFi configuration. "
                         "This is a UUID of the format XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX.");
  auto portId = inputPortsObj["id"].as<std::string>();
  uuid = portId;

  port = std::make_shared<minifi::RemoteProcessorGroupPort>(stream_factory_, nameStr, parent->getURL(), this->configuration_, uuid);

  processor = std::static_pointer_cast<core::Processor>(port);
  port->setDirection(direction);
  port->setTimeOut(parent->getTimeOut());
  port->setTransmitting(true);
  processor->setYieldPeriodMsec(parent->getYieldPeriodMsec());
  processor->initialize();
  if (!parent->getInterface().empty())
    port->setInterface(parent->getInterface());
  if (parent->getTransportProtocol() == "HTTP") {
    port->enableHTTP();
    if (!parent->getHttpProxyHost().empty())
      port->setHTTPProxy(parent->getHTTPProxy());
  }
  // else defaults to RAW

  // handle port properties
  YAML::Node nodeVal = portNode.as<YAML::Node>();
  YAML::Node propertiesNode = nodeVal["Properties"];
  parsePropertiesNodeYaml(propertiesNode, std::static_pointer_cast<core::ConfigurableComponent>(processor), nameStr,
  CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY);

  // add processor to parent
  parent->addProcessor(processor);
  processor->setScheduledState(core::RUNNING);

  if (inputPortsObj["max concurrent tasks"]) {
    auto rawMaxConcurrentTasks = inputPortsObj["max concurrent tasks"].as<std::string>();
    int32_t maxConcurrentTasks;
    if (core::Property::StringToInt(rawMaxConcurrentTasks, maxConcurrentTasks)) {
      processor->setMaxConcurrentTasks(maxConcurrentTasks);
    }
    logger_->log_debug("parseProcessorNode: maxConcurrentTasks => [%d]", maxConcurrentTasks);
    processor->setMaxConcurrentTasks(maxConcurrentTasks);
  }
}

void YamlConfiguration::parsePropertyValueSequence(const std::string& propertyName, const YAML::Node& propertyValueNode, std::shared_ptr<core::ConfigurableComponent> processor) {
  for (const auto& iter : propertyValueNode) {
    if (iter.IsDefined()) {
      YAML::Node nodeVal = iter.as<YAML::Node>();
      YAML::Node propertiesNode = nodeVal["value"];
      // must insert the sequence in differently.
      std::string rawValueString = propertiesNode.as<std::string>();
      logger_->log_debug("Found %s=%s", propertyName, rawValueString);
      if (!processor->updateProperty(propertyName, rawValueString)) {
        std::shared_ptr<core::Connectable> proc = std::dynamic_pointer_cast<core::Connectable>(processor);
        if (proc) {
          logger_->log_warn("Received property %s with value %s but is not one of the properties for %s. Attempting to add as dynamic property.", propertyName, rawValueString, proc->getName());
          if (!processor->setDynamicProperty(propertyName, rawValueString)) {
            logger_->log_warn("Unable to set the dynamic property %s with value %s", propertyName, rawValueString);
          } else {
            logger_->log_warn("Dynamic property %s with value %s set", propertyName, rawValueString);
          }
        }
      }
    }
  }
}

namespace {
  void handleExceptionOnValidatedProcessorPropertyRead(const core::Property& propertyFromProcessor, const YAML::Node& propertyValueNode,
      const std::shared_ptr<Configure>& config, const std::type_index& defaultType, std::shared_ptr<logging::Logger>& logger) {
    std::string eof;
    bool const exit_on_failure = (config->get(Configure::nifi_flow_configuration_file_exit_failure, eof) && utils::StringUtils::toBool(eof).value_or(false));
    logger->log_error("Invalid conversion for field %s. Value %s", propertyFromProcessor.getName(), propertyValueNode.as<std::string>());
    if (exit_on_failure) {
      // We do not exit here even if exit_on_failure is set. Maybe we should?
      logger->log_error("Invalid conversion for %s to %s.", propertyFromProcessor.getName(), defaultType.name());
    }
  }
}  // namespace

// coerce the types. upon failure we will either exit or use the default value.
// we do this here ( in addition to the PropertyValue class ) to get the earliest
// possible YAML failure.
PropertyValue YamlConfiguration::getValidatedProcessorPropertyForDefaultTypeInfo(const core::Property& propertyFromProcessor, const YAML::Node& propertyValueNode) {
  PropertyValue defaultValue;
  defaultValue = propertyFromProcessor.getDefaultValue();
  const std::type_index defaultType = defaultValue.getTypeInfo();
  try {
    PropertyValue coercedValue = defaultValue;
    if (defaultType == typeid(int64_t)) {
      coercedValue = propertyValueNode.as<int64_t>();
    } else if (defaultType == typeid(uint64_t)) {
      try {
        coercedValue = propertyValueNode.as<uint64_t>();
      } catch (...) {
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
    handleExceptionOnValidatedProcessorPropertyRead(propertyFromProcessor, propertyValueNode, configuration_, defaultType, logger_);
  }  catch (...) {
    handleExceptionOnValidatedProcessorPropertyRead(propertyFromProcessor, propertyValueNode, configuration_, defaultType, logger_);
  }
  return defaultValue;
}

void YamlConfiguration::parseSingleProperty(const std::string& propertyName, const YAML::Node& propertyValueNode, std::shared_ptr<core::ConfigurableComponent> processor) {
  core::Property myProp(propertyName, "", "");
  processor->getProperty(propertyName, myProp);
  const PropertyValue coercedValue = getValidatedProcessorPropertyForDefaultTypeInfo(myProp, propertyValueNode);
  bool property_set = false;
  try {
    property_set = processor->setProperty(myProp, coercedValue);
  } catch(const utils::internal::InvalidValueException&) {
    auto component = std::dynamic_pointer_cast<core::CoreComponent>(processor);
    logger_->log_error("Invalid value was set for property '%s' creating component '%s'", propertyName, component->getName());
    throw;
  }
  const std::string rawValueString = propertyValueNode.as<std::string>();
  if (!property_set) {
    std::shared_ptr<core::Connectable> proc = std::dynamic_pointer_cast<core::Connectable>(processor);
    if (proc) {
      logger_->log_warn("Received property %s with value %s but is not one of the properties for %s. Attempting to add as dynamic property.", propertyName, rawValueString, proc->getName());
      if (!processor->setDynamicProperty(propertyName, rawValueString)) {
        logger_->log_warn("Unable to set the dynamic property %s with value %s", propertyName, rawValueString);
      } else {
        logger_->log_warn("Dynamic property %s with value %s set", propertyName, rawValueString);
      }
    }
  } else {
    logger_->log_debug("Property %s with value %s set", propertyName, rawValueString);
  }
}

void YamlConfiguration::parsePropertyNodeElement(const std::string& propertyName, const YAML::Node& propertyValueNode, std::shared_ptr<core::ConfigurableComponent> processor) {
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

void YamlConfiguration::parsePropertiesNodeYaml(const YAML::Node& propertiesNode, std::shared_ptr<core::ConfigurableComponent> processor, const std::string& component_name,
    const std::string& yaml_section) {
  // Treat generically as a YAML node so we can perform inspection on entries to ensure they are populated
  logger_->log_trace("Entered %s", component_name);
  for (const auto& propertyElem : propertiesNode) {
    const std::string propertyName = propertyElem.first.as<std::string>();
    const YAML::Node propertyValueNode = propertyElem.second;
    parsePropertyNodeElement(propertyName, propertyValueNode, processor);
  }

  validateComponentProperties(processor, component_name, yaml_section);
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
    YAML::Node funnel_node = element.as<YAML::Node>();

    std::string id = getOrGenerateId(funnel_node);

    // Default name to be same as ID
    std::string name = funnel_node["name"].as<std::string>(id);

    const auto uuid = utils::Identifier::parse(id) | utils::orElse([this] {
      logger_->log_debug("Incorrect funnel UUID format.");
      throw Exception(ExceptionType::GENERAL_EXCEPTION, "Incorrect funnel UUID format.");
    });

    std::shared_ptr<core::Processor> funnel = std::make_shared<core::Funnel>(name, uuid.value());
    logger_->log_debug("Created funnel with UUID %s and name %s", id, name);
    funnel->setScheduledState(core::RUNNING);
    funnel->setSchedulingStrategy(core::EVENT_DRIVEN);
    parent->addProcessor(funnel);
  }
}

void YamlConfiguration::validateComponentProperties(const std::shared_ptr<ConfigurableComponent> &component, const std::string &component_name, const std::string &yaml_section) const {
  const auto &component_properties = component->getProperties();

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
      std::regex excl_expr(excl_pair.second);
      if (std::regex_match(component_properties.at(excl_pair.first).getValue().to_string(), excl_expr)) {
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
      std::regex prop_regex(prop_regex_str);
      if (!std::regex_match(prop_pair.second.getValue().to_string(), prop_regex)) {
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
  YAML::Node node = yamlNode.as<YAML::Node>();

  if (node[idField]) {
    if (YAML::NodeType::Scalar == node[idField].Type()) {
      id = node[idField].as<std::string>();
    } else {
      throw std::invalid_argument("getOrGenerateId: idField is expected to reference YAML::Node "
                                  "of YAML::NodeType::Scalar.");
    }
  } else {
    id = id_generator_->generate().to_string();
    logger_->log_debug("Generating random ID: id => [%s]", id);
  }
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

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
