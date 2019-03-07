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

#include "core/yaml/YamlConfiguration.h"
#include "core/state/Value.h"
#ifdef YAML_CONFIGURATION_USE_REGEX
#include <regex>
#endif  // YAML_CONFIGURATION_USE_REGEX

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

std::shared_ptr<utils::IdGenerator> YamlConfiguration::id_generator_ = utils::IdGenerator::getIdGenerator();

core::ProcessGroup *YamlConfiguration::parseRootProcessGroupYaml(YAML::Node rootFlowNode) {
  utils::Identifier uuid;
  int version = 0;

  checkRequiredField(&rootFlowNode, "name",
  CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY);
  std::string flowName = rootFlowNode["name"].as<std::string>();

  auto class_loader_functions = rootFlowNode["Class Loader Functions"];
  if (class_loader_functions && class_loader_functions.IsSequence()) {
    for (auto function : class_loader_functions) {
      registerResource(function.as<std::string>());
    }
  }

  std::string id = getOrGenerateId(&rootFlowNode);
  uuid = id;

  if (rootFlowNode["version"]) {
    version = rootFlowNode["version"].as<int>();
  }

  logger_->log_debug("parseRootProcessGroup: id => [%s], name => [%s]", id, flowName);
  std::unique_ptr<core::ProcessGroup> group = FlowConfiguration::createRootProcessGroup(flowName, uuid, version);

  this->name_ = flowName;

  return group.release();
}

void YamlConfiguration::parseProcessorNodeYaml(YAML::Node processorsNode, core::ProcessGroup *parentGroup) {
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

  if (processorsNode) {
    if (processorsNode.IsSequence()) {
      // Evaluate sequence of processors
      for (YAML::const_iterator iter = processorsNode.begin(); iter != processorsNode.end(); ++iter) {
        core::ProcessorConfig procCfg;
        YAML::Node procNode = iter->as<YAML::Node>();

        checkRequiredField(&procNode, "name",
        CONFIG_YAML_PROCESSORS_KEY);
        procCfg.name = procNode["name"].as<std::string>();
        procCfg.id = getOrGenerateId(&procNode);

        auto lib_location = procNode["Library Location"];
        auto lib_function = procNode["Library Function"];
        if (lib_location && lib_function) {
          auto lib_location_str = lib_location.as<std::string>();
          auto lib_function_str = lib_function.as<std::string>();
          registerResource(lib_location_str, lib_function_str);
        }

        uuid = procCfg.id.c_str();
        logger_->log_debug("parseProcessorNode: name => [%s] id => [%s]", procCfg.name, procCfg.id);
        checkRequiredField(&procNode, "class", CONFIG_YAML_PROCESSORS_KEY);
        procCfg.javaClass = procNode["class"].as<std::string>();
        logger_->log_debug("parseProcessorNode: class => [%s]", procCfg.javaClass);

        // Determine the processor name only from the Java class
        auto lastOfIdx = procCfg.javaClass.find_last_of(".");
        if (lastOfIdx != std::string::npos) {
          lastOfIdx++;  // if a value is found, increment to move beyond the .
          int nameLength = procCfg.javaClass.length() - lastOfIdx;
          std::string processorName = procCfg.javaClass.substr(lastOfIdx, nameLength);
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

        auto strategyNode = getOptionalField(&procNode, "scheduling strategy", YAML::Node(DEFAULT_SCHEDULING_STRATEGY),
        CONFIG_YAML_PROCESSORS_KEY);
        procCfg.schedulingStrategy = strategyNode.as<std::string>();
        logger_->log_debug("parseProcessorNode: scheduling strategy => [%s]", procCfg.schedulingStrategy);

        auto periodNode = getOptionalField(&procNode, "scheduling period", YAML::Node(DEFAULT_SCHEDULING_PERIOD_STR),
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
          parsePropertiesNodeYaml(&propertiesNode, processor, procCfg.name, CONFIG_YAML_PROCESSORS_KEY);
        }

        // Take care of scheduling
        core::TimeUnit unit;
        if (core::Property::StringToTime(procCfg.schedulingPeriod, schedulingPeriod, unit) && core::Property::ConvertTimeUnitToNS(schedulingPeriod, unit, schedulingPeriod)) {
          logger_->log_debug("convert: parseProcessorNode: schedulingPeriod => [%ll] ns", schedulingPeriod);
          processor->setSchedulingPeriodNano(schedulingPeriod);
        }

        if (core::Property::StringToTime(procCfg.penalizationPeriod, penalizationPeriod, unit) && core::Property::ConvertTimeUnitToMS(penalizationPeriod, unit, penalizationPeriod)) {
          logger_->log_debug("convert: parseProcessorNode: penalizationPeriod => [%ll] ms", penalizationPeriod);
          processor->setPenalizationPeriodMsec(penalizationPeriod);
        }

        if (core::Property::StringToTime(procCfg.yieldPeriod, yieldPeriod, unit) && core::Property::ConvertTimeUnitToMS(yieldPeriod, unit, yieldPeriod)) {
          logger_->log_debug("convert: parseProcessorNode: yieldPeriod => [%ll] ms", yieldPeriod);
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
    } else {
      throw new std::invalid_argument("Cannot instantiate a MiNiFi instance without a defined Processors configuration node.");
    }
  } else {
    throw new std::invalid_argument("Cannot instantiate a MiNiFi instance without a defined "
                                    "Processors configuration node.");
  }
}

void YamlConfiguration::parseRemoteProcessGroupYaml(YAML::Node *rpgNode, core::ProcessGroup *parentGroup) {
  utils::Identifier uuid;
  std::string id;

  if (!parentGroup) {
    logger_->log_error("parseRemoteProcessGroupYaml: no parent group exists");
    return;
  }

  if (rpgNode) {
    if (rpgNode->IsSequence()) {
      for (YAML::const_iterator iter = rpgNode->begin(); iter != rpgNode->end(); ++iter) {
        YAML::Node currRpgNode = iter->as<YAML::Node>();

        checkRequiredField(&currRpgNode, "name",
        CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY);
        auto name = currRpgNode["name"].as<std::string>();
        id = getOrGenerateId(&currRpgNode);

        logger_->log_debug("parseRemoteProcessGroupYaml: name => [%s], id => [%s]", name, id);

        auto urlNode = getOptionalField(&currRpgNode, "url", YAML::Node(""),
        CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY);

        std::string url = urlNode.as<std::string>();
        logger_->log_debug("parseRemoteProcessGroupYaml: url => [%s]", url);

        core::ProcessGroup *group = NULL;
        core::TimeUnit unit;
        int64_t timeoutValue = -1;
        int64_t yieldPeriodValue = -1;
        uuid = id;
        group = this->createRemoteProcessGroup(name.c_str(), uuid).release();
        group->setParent(parentGroup);
        parentGroup->addProcessGroup(group);

        if (currRpgNode["yield period"]) {
          std::string yieldPeriod = currRpgNode["yield period"].as<std::string>();
          logger_->log_debug("parseRemoteProcessGroupYaml: yield period => [%s]", yieldPeriod);

          if (core::Property::StringToTime(yieldPeriod, yieldPeriodValue, unit) && core::Property::ConvertTimeUnitToMS(yieldPeriodValue, unit, yieldPeriodValue) && group) {
            logger_->log_debug("parseRemoteProcessGroupYaml: yieldPeriod => [%ll] ms", yieldPeriodValue);
            group->setYieldPeriodMsec(yieldPeriodValue);
          }
        }

        if (currRpgNode["timeout"]) {
          std::string timeout = currRpgNode["timeout"].as<std::string>();
          logger_->log_debug("parseRemoteProcessGroupYaml: timeout => [%s]", timeout);

          if (core::Property::StringToTime(timeout, timeoutValue, unit) && core::Property::ConvertTimeUnitToMS(timeoutValue, unit, timeoutValue) && group) {
            logger_->log_debug("parseRemoteProcessGroupYaml: timeoutValue => [%ll] ms", timeoutValue);
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

        checkRequiredField(&currRpgNode, "Input Ports",
        CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY);
        YAML::Node inputPorts = currRpgNode["Input Ports"].as<YAML::Node>();
        if (inputPorts && inputPorts.IsSequence()) {
          for (YAML::const_iterator portIter = inputPorts.begin(); portIter != inputPorts.end(); ++portIter) {
            YAML::Node currPort = portIter->as<YAML::Node>();

            this->parsePortYaml(&currPort, group, sitetosite::SEND);
          }  // for node
        }
        YAML::Node outputPorts = currRpgNode["Output Ports"].as<YAML::Node>();
        if (outputPorts && outputPorts.IsSequence()) {
          for (YAML::const_iterator portIter = outputPorts.begin(); portIter != outputPorts.end(); ++portIter) {
            logger_->log_debug("Got a current port, iterating...");

            YAML::Node currPort = portIter->as<YAML::Node>();

            this->parsePortYaml(&currPort, group, sitetosite::RECEIVE);
          }  // for node
        }
      }
    }
  }
}

void YamlConfiguration::parseProvenanceReportingYaml(YAML::Node *reportNode, core::ProcessGroup *parentGroup) {
  utils::Identifier port_uuid;
  int64_t schedulingPeriod = -1;

  if (!parentGroup) {
    logger_->log_error("parseProvenanceReportingYaml: no parent group exists");
    return;
  }

  if (!reportNode || !reportNode->IsDefined() || reportNode->IsNull()) {
    logger_->log_debug("no provenance reporting task specified");
    return;
  }

  std::shared_ptr<core::Processor> processor = nullptr;
  processor = createProvenanceReportTask();
  std::shared_ptr<core::reporting::SiteToSiteProvenanceReportingTask> reportTask = std::static_pointer_cast<core::reporting::SiteToSiteProvenanceReportingTask>(processor);

  YAML::Node node = reportNode->as<YAML::Node>();

  checkRequiredField(&node, "scheduling strategy",
  CONFIG_YAML_PROVENANCE_REPORT_KEY);
  auto schedulingStrategyStr = node["scheduling strategy"].as<std::string>();
  checkRequiredField(&node, "scheduling period",
  CONFIG_YAML_PROVENANCE_REPORT_KEY);
  auto schedulingPeriodStr = node["scheduling period"].as<std::string>();

  core::TimeUnit unit;
  if (core::Property::StringToTime(schedulingPeriodStr, schedulingPeriod, unit) && core::Property::ConvertTimeUnitToNS(schedulingPeriod, unit, schedulingPeriod)) {
    logger_->log_debug("ProvenanceReportingTask schedulingPeriod %ll ns", schedulingPeriod);
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
      logger_->log_debug("ProvenanceReportingTask port %ll", lvalue);
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
  checkRequiredField(&node, "port uuid", CONFIG_YAML_PROVENANCE_REPORT_KEY);
  auto portUUIDStr = node["port uuid"].as<std::string>();
  checkRequiredField(&node, "batch size", CONFIG_YAML_PROVENANCE_REPORT_KEY);
  auto batchSizeStr = node["batch size"].as<std::string>();

  logger_->log_debug("ProvenanceReportingTask port uuid %s", portUUIDStr);
  port_uuid = portUUIDStr;
  reportTask->setPortUUID(port_uuid);

  if (core::Property::StringToInt(batchSizeStr, lvalue)) {
    reportTask->setBatchSize(lvalue);
  }

  reportTask->initialize();

  // add processor to parent
  parentGroup->addProcessor(processor);
  processor->setScheduledState(core::RUNNING);
}

void YamlConfiguration::parseControllerServices(YAML::Node *controllerServicesNode) {
  if (!IsNullOrEmpty(controllerServicesNode)) {
    if (controllerServicesNode->IsSequence()) {
      for (auto iter : *controllerServicesNode) {
        YAML::Node controllerServiceNode = iter.as<YAML::Node>();
        try {
          checkRequiredField(&controllerServiceNode, "name",
          CONFIG_YAML_CONTROLLER_SERVICES_KEY);
          checkRequiredField(&controllerServiceNode, "id",
          CONFIG_YAML_CONTROLLER_SERVICES_KEY);
          std::string type = "";

          try {
            checkRequiredField(&controllerServiceNode, "class", CONFIG_YAML_CONTROLLER_SERVICES_KEY);
            type = controllerServiceNode["class"].as<std::string>();
          } catch (const std::invalid_argument &e) {
            checkRequiredField(&controllerServiceNode, "type", CONFIG_YAML_CONTROLLER_SERVICES_KEY);
            type = controllerServiceNode["type"].as<std::string>();
            logger_->log_debug("Using type %s for controller service node", type);
          }
          std::string fullType = type;
          auto lastOfIdx = type.find_last_of(".");
          if (lastOfIdx != std::string::npos) {
            lastOfIdx++;  // if a value is found, increment to move beyond the .
            int nameLength = type.length() - lastOfIdx;
            type = type.substr(lastOfIdx, nameLength);
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
            // we should propogate properties to the node and to the implementation
            parsePropertiesNodeYaml(&propertiesNode, std::static_pointer_cast<core::ConfigurableComponent>(controller_service_node), name,
            CONFIG_YAML_CONTROLLER_SERVICES_KEY);
            if (controller_service_node->getControllerServiceImplementation() != nullptr) {
              parsePropertiesNodeYaml(&propertiesNode, std::static_pointer_cast<core::ConfigurableComponent>(controller_service_node->getControllerServiceImplementation()), name,
              CONFIG_YAML_CONTROLLER_SERVICES_KEY);
            }
          } else {
            logger_->log_debug("Could not locate %s", type);
          }
          controller_services_->put(id, controller_service_node);
          controller_services_->put(name, controller_service_node);
        } catch (YAML::InvalidNode &in) {
          throw Exception(ExceptionType::GENERAL_EXCEPTION, "Name, id, and class must be specified for controller services");
        }
      }
    }
  }
}

void YamlConfiguration::parseConnectionYaml(YAML::Node *connectionsNode, core::ProcessGroup *parent) {
  if (!parent) {
    logger_->log_error("parseProcessNode: no parent group was provided");
    return;
  }

  if (connectionsNode) {
    if (connectionsNode->IsSequence()) {
      for (YAML::const_iterator iter = connectionsNode->begin(); iter != connectionsNode->end(); ++iter) {
        YAML::Node connectionNode = iter->as<YAML::Node>();
        std::shared_ptr<minifi::Connection> connection = nullptr;

        // Configure basic connection
        utils::Identifier uuid;
        std::string id = getOrGenerateId(&connectionNode);

        // Default name to be same as ID
        std::string name = id;

        // If name is specified in configuration, use the value
        if (connectionNode["name"]) {
          name = connectionNode["name"].as<std::string>();
        }

        uuid = id;
        connection = this->createConnection(name, uuid);
        logger_->log_debug("Created connection with UUID %s and name %s", id, name);

        // Configure connection source
        if (connectionNode.as<YAML::Node>()["source relationship name"]) {
          auto rawRelationship = connectionNode["source relationship name"].as<std::string>();
          core::Relationship relationship(rawRelationship, "");
          logger_->log_debug("parseConnection: relationship => [%s]", rawRelationship);
          if (connection) {
            connection->addRelationship(relationship);
          }
        } else if (connectionNode.as<YAML::Node>()["source relationship names"]) {
          auto relList = connectionNode["source relationship names"];
          if (connection) {
            if (relList.IsSequence()) {
              for (const auto &rel : relList) {
                auto rawRelationship = rel.as<std::string>();
                core::Relationship relationship(rawRelationship, "");
                logger_->log_debug("parseConnection: relationship => [%s]", rawRelationship);
                connection->addRelationship(relationship);
              }
            } else {
              auto rawRelationship = relList.as<std::string>();
              core::Relationship relationship(rawRelationship, "");
              logger_->log_debug("parseConnection: relationship => [%s]", rawRelationship);
              connection->addRelationship(relationship);
            }
          }
        }

        utils::Identifier srcUUID;

        if (connectionNode["max work queue size"]) {
          auto max_work_queue_str = connectionNode["max work queue size"].as<std::string>();
          uint64_t max_work_queue_size = 0;
          if (core::Property::StringToInt(max_work_queue_str, max_work_queue_size)) {
            connection->setMaxQueueSize(max_work_queue_size);
          }
          logging::LOG_DEBUG(logger_) << "Setting " << max_work_queue_size << " as the max queue size for " << name;
        }

        if (connectionNode["max work queue data size"]) {
          auto max_work_queue_str = connectionNode["max work queue data size"].as<std::string>();
          uint64_t max_work_queue_data_size = 0;
          if (core::Property::StringToInt(max_work_queue_str, max_work_queue_data_size)) {
            connection->setMaxQueueDataSize(max_work_queue_data_size);
          }
          logging::LOG_DEBUG(logger_) << "Setting " << max_work_queue_data_size << " as the max queue data size for " << name;
        }

        if (connectionNode["source id"]) {
          std::string connectionSrcProcId = connectionNode["source id"].as<std::string>();
          srcUUID = connectionSrcProcId;
          logger_->log_debug("Using 'source id' to match source with same id for "
                             "connection '%s': source id => [%s]",
                             name, connectionSrcProcId);
        } else {
          // if we don't have a source id, try to resolve using source name. config schema v2 will make this unnecessary
          checkRequiredField(&connectionNode, "source name",
          CONFIG_YAML_CONNECTIONS_KEY);
          std::string connectionSrcProcName = connectionNode["source name"].as<std::string>();
          utils::Identifier tmpUUID;
          tmpUUID = connectionSrcProcName;
          if (NULL != parent->findProcessor(tmpUUID)) {
            // the source name is a remote port id, so use that as the source id
            srcUUID = tmpUUID;
            logger_->log_debug("Using 'source name' containing a remote port id to match the source for "
                               "connection '%s': source name => [%s]",
                               name, connectionSrcProcName);
          } else {
            // lastly, look the processor up by name
            auto srcProcessor = parent->findProcessor(connectionSrcProcName);
            if (NULL != srcProcessor) {
              srcProcessor->getUUID(srcUUID);
              logger_->log_debug("Using 'source name' to match source with same name for "
                                 "connection '%s': source name => [%s]",
                                 name, connectionSrcProcName);
            } else {
              // we ran out of ways to discover the source processor
              logger_->log_error("Could not locate a source with name %s to create a connection", connectionSrcProcName);
              throw std::invalid_argument("Could not locate a source with name " + connectionSrcProcName + " to create a connection ");
            }
          }
        }
        connection->setSourceUUID(srcUUID);

        // Configure connection destination
        utils::Identifier destUUID;
        if (connectionNode["destination id"]) {
          std::string connectionDestProcId = connectionNode["destination id"].as<std::string>();
          destUUID = connectionDestProcId;
          logger_->log_debug("Using 'destination id' to match destination with same id for "
                             "connection '%s': destination id => [%s]",
                             name, connectionDestProcId);
        } else {
          // we use the same logic as above for resolving the source processor
          // for looking up the destination processor in absence of a processor id
          checkRequiredField(&connectionNode, "destination name",
          CONFIG_YAML_CONNECTIONS_KEY);
          std::string connectionDestProcName = connectionNode["destination name"].as<std::string>();
          utils::Identifier tmpUUID;
          tmpUUID = connectionDestProcName;
          if (parent->findProcessor(tmpUUID)) {
            // the destination name is a remote port id, so use that as the dest id
            destUUID = tmpUUID;
            logger_->log_debug("Using 'destination name' containing a remote port id to match the destination for "
                               "connection '%s': destination name => [%s]",
                               name, connectionDestProcName);
          } else {
            // look the processor up by name
            auto destProcessor = parent->findProcessor(connectionDestProcName);
            if (NULL != destProcessor) {
              destProcessor->getUUID(destUUID);
              logger_->log_debug("Using 'destination name' to match destination with same name for "
                                 "connection '%s': destination name => [%s]",
                                 name, connectionDestProcName);
            } else {
              // we ran out of ways to discover the destination processor
              logger_->log_error("Could not locate a destination with name %s to create a connection", connectionDestProcName);
              throw std::invalid_argument("Could not locate a destination with name " + connectionDestProcName + " to create a connection");
            }
          }
        }
        connection->setDestinationUUID(destUUID);

        if (connection) {
          parent->addConnection(connection);
        }
      }
    }
  }
}

void YamlConfiguration::parsePortYaml(YAML::Node *portNode, core::ProcessGroup *parent, sitetosite::TransferDirection direction) {
  utils::Identifier uuid;
  std::shared_ptr<core::Processor> processor = NULL;
  std::shared_ptr<minifi::RemoteProcessorGroupPort> port = NULL;

  if (!parent) {
    logger_->log_error("parseProcessNode: no parent group existed");
    return;
  }

  YAML::Node inputPortsObj = portNode->as<YAML::Node>();

  // Check for required fields
  checkRequiredField(&inputPortsObj, "name",
  CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY);
  auto nameStr = inputPortsObj["name"].as<std::string>();
  checkRequiredField(&inputPortsObj, "id",
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
  YAML::Node nodeVal = portNode->as<YAML::Node>();
  YAML::Node propertiesNode = nodeVal["Properties"];
  parsePropertiesNodeYaml(&propertiesNode, std::static_pointer_cast<core::ConfigurableComponent>(processor), nameStr,
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

void YamlConfiguration::parsePropertiesNodeYaml(YAML::Node *propertiesNode, std::shared_ptr<core::ConfigurableComponent> processor, const std::string &component_name,
                                                const std::string &yaml_section) {
  // Treat generically as a YAML node so we can perform inspection on entries to ensure they are populated
  logger_->log_trace("Entered %s", component_name);
  for (YAML::const_iterator propsIter = propertiesNode->begin(); propsIter != propertiesNode->end(); ++propsIter) {
    std::string propertyName = propsIter->first.as<std::string>();
    YAML::Node propertyValueNode = propsIter->second;
    logger_->log_trace("Encountered %s", propertyName);
    if (!propertyValueNode.IsNull() && propertyValueNode.IsDefined()) {
      if (propertyValueNode.IsSequence()) {
        for (auto iter : propertyValueNode) {
          if (iter.IsDefined()) {
            YAML::Node nodeVal = iter.as<YAML::Node>();
            YAML::Node propertiesNode = nodeVal["value"];
            // must insert the sequence in differently.
            std::string rawValueString = propertiesNode.as<std::string>();
            logger_->log_debug("Found %s=%s", propertyName, rawValueString);
            if (!processor->updateProperty(propertyName, rawValueString)) {
              std::shared_ptr<core::Connectable> proc = std::dynamic_pointer_cast<core::Connectable>(processor);
              if (proc != 0) {
                logger_->log_warn("Received property %s with value %s but is not one of the properties for %s. "
                                  "Attempting to add as dynamic property.",
                                  propertyName, rawValueString, proc->getName());
                if (!processor->setDynamicProperty(propertyName, rawValueString)) {
                  logger_->log_warn("Unable to set the dynamic property %s with value %s", propertyName.c_str(), rawValueString.c_str());
                } else {
                  logger_->log_warn("Dynamic property %s with value %s set", propertyName.c_str(), rawValueString.c_str());
                }
              }
            }
          }
        }
      } else {
        core::Property myProp(propertyName, "", "");
        processor->getProperty(propertyName, myProp);
        PropertyValue defaultValue;
        defaultValue = myProp.getDefaultValue();
        auto defaultType = defaultValue.getTypeInfo();
        PropertyValue coercedValue = defaultValue;

        // coerce the types. upon failure we will either exit or use the default value.
        // we do this here ( in addition to the PropertyValue class ) to get the earliest
        // possible YAML failure.
        try {
          if (defaultType == typeid(std::string)) {
            auto typedValue = propertyValueNode.as<std::string>();
            coercedValue = typedValue;
          } else if (defaultType == typeid(int64_t)) {
            auto typedValue = propertyValueNode.as<int64_t>();
            coercedValue = typedValue;
          } else if (defaultType == typeid(uint64_t)) {
            try {
              auto typedValue = propertyValueNode.as<uint64_t>();
              coercedValue = typedValue;
            } catch (...) {
              auto typedValue = propertyValueNode.as<std::string>();
              coercedValue = typedValue;
            }
          } else if (defaultType == typeid(int)) {
            auto typedValue = propertyValueNode.as<int>();
            coercedValue = typedValue;
          } else if (defaultType == typeid(bool)) {
            auto typedValue = propertyValueNode.as<bool>();
            coercedValue = typedValue;
          } else {
            auto typedValue = propertyValueNode.as<std::string>();
            coercedValue = typedValue;
          }
        } catch (...) {
          std::string eof;
          bool exit_on_failure = false;
          if (configuration_->get(Configure::nifi_flow_configuration_file_exit_failure, eof)) {
            utils::StringUtils::StringToBool(eof, exit_on_failure);
          }
          logger_->log_error("Invalid conversion for field %s. Value %s", myProp.getName(), propertyValueNode.as<std::string>());
          if (exit_on_failure) {
            std::cerr << "Invalid conversion for " << myProp.getName() << " to " << defaultType.name() << std::endl;
          } else {
            coercedValue = defaultValue;
          }
        }
        std::string rawValueString = propertyValueNode.as<std::string>();
        if (!processor->setProperty(myProp, coercedValue)) {
          std::shared_ptr<core::Connectable> proc = std::dynamic_pointer_cast<core::Connectable>(processor);
          if (proc != 0) {
            logger_->log_warn("Received property %s with value %s but is not one of the properties for %s. "
                              "Attempting to add as dynamic property.",
                              propertyName, rawValueString, proc->getName());
            if (!processor->setDynamicProperty(propertyName, rawValueString)) {
              logger_->log_warn("Unable to set the dynamic property %s with value %s", propertyName.c_str(), rawValueString.c_str());
            } else {
              logger_->log_warn("Dynamic property %s with value %s set", propertyName.c_str(), rawValueString.c_str());
            }
          }
        } else {
          logger_->log_debug("Property %s with value %s set", propertyName.c_str(), rawValueString.c_str());
        }
      }
    }
  }

  validateComponentProperties(processor, component_name, yaml_section);
}

void YamlConfiguration::validateComponentProperties(const std::shared_ptr<ConfigurableComponent> &component, const std::string &component_name, const std::string &yaml_section) const {
  const auto &component_properties = component->getProperties();

  // Validate required properties
  for (const auto &prop_pair : component_properties) {
    if (prop_pair.second.getRequired()) {
      if (prop_pair.second.getValue().to_string().empty()) {
        std::stringstream reason;
        reason << "required property '" << prop_pair.second.getName() << "' is not set";
        raiseComponentError(component_name, yaml_section, reason.str());
      } else if (!prop_pair.second.getValue().validate(prop_pair.first).valid()) {
        std::stringstream reason;
        reason << "Property '" << prop_pair.second.getName() << "' is not valid";
        raiseComponentError(component_name, yaml_section, reason.str());
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
        std::string reason("property '");
        reason.append(prop_pair.second.getName());
        reason.append("' depends on property '");
        reason.append(dep_prop_key);
        reason.append("' which is not set");
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
        std::string reason("property '");
        reason.append(prop_pair.second.getName());
        reason.append("' is exclusive of property '");
        reason.append(excl_pair.first);
        reason.append("' values matching '");
        reason.append(excl_pair.second);
        reason.append("'");
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
        std::stringstream reason;
        reason << "property '" << prop_pair.second.getName() << "' does not match validation pattern '" << prop_regex_str << "'";
        raiseComponentError(component_name, yaml_section, reason.str());
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

std::string YamlConfiguration::getOrGenerateId(YAML::Node *yamlNode, const std::string &idField) {
  std::string id;
  YAML::Node node = yamlNode->as<YAML::Node>();

  if (node[idField]) {
    if (YAML::NodeType::Scalar == node[idField].Type()) {
      id = node[idField].as<std::string>();
    } else {
      throw std::invalid_argument("getOrGenerateId: idField is expected to reference YAML::Node "
                                  "of YAML::NodeType::Scalar.");
    }
  } else {
    utils::Identifier uuid;
    id_generator_->generate(uuid);
    id = uuid.to_string();
    logger_->log_debug("Generating random ID: id => [%s]", id);
  }
  return id;
}

void YamlConfiguration::checkRequiredField(YAML::Node *yamlNode, const std::string &fieldName, const std::string &yamlSection, const std::string &errorMessage) {
  std::string errMsg = errorMessage;
  if (!yamlNode->as<YAML::Node>()[fieldName]) {
    if (errMsg.empty()) {
      // Build a helpful error message for the user so they can fix the
      // invalid YAML config file, using the component name if present
      errMsg =
          yamlNode->as<YAML::Node>()["name"] ?
              "Unable to parse configuration file for component named '" + yamlNode->as<YAML::Node>()["name"].as<std::string>() + "' as required field '" + fieldName + "' is missing" :
              "Unable to parse configuration file as required field '" + fieldName + "' is missing";
      if (!yamlSection.empty()) {
        errMsg += " [in '" + yamlSection + "' section of configuration file]";
      }
    }
    logging::LOG_ERROR(logger_) << errMsg;

    throw std::invalid_argument(errMsg);
  }
}

YAML::Node YamlConfiguration::getOptionalField(YAML::Node *yamlNode, const std::string &fieldName, const YAML::Node &defaultValue, const std::string &yamlSection,
                                               const std::string &providedInfoMessage) {
  std::string infoMessage = providedInfoMessage;
  auto result = yamlNode->as<YAML::Node>()[fieldName];
  if (!result) {
    if (infoMessage.empty()) {
      // Build a helpful info message for the user to inform them that a default is being used
      infoMessage =
          yamlNode->as<YAML::Node>()["name"] ?
              "Using default value for optional field '" + fieldName + "' in component named '" + yamlNode->as<YAML::Node>()["name"].as<std::string>() + "'" :
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
