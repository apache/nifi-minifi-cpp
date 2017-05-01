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

#include "core/yaml/YamlConfiguration.h"
#include <memory>
#include <string>
#include <vector>
#include <set>
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

core::ProcessGroup *YamlConfiguration::parseRootProcessGroupYaml(
    YAML::Node rootFlowNode) {
  uuid_t uuid;

  checkRequiredField(&rootFlowNode, "name", CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY);
  std::string flowName = rootFlowNode["name"].as<std::string>();
  std::string id = getOrGenerateId(&rootFlowNode);
  uuid_parse(id.c_str(), uuid);

  logger_->log_debug(
      "parseRootProcessGroup: id => [%s], name => [%s]", id, flowName);
  std::unique_ptr<core::ProcessGroup> group =
      FlowConfiguration::createRootProcessGroup(flowName, uuid);

  this->name_ = flowName;

  return group.release();
}

void YamlConfiguration::parseProcessorNodeYaml(
    YAML::Node processorsNode, core::ProcessGroup * parentGroup) {
  int64_t schedulingPeriod = -1;
  int64_t penalizationPeriod = -1;
  int64_t yieldPeriod = -1;
  int64_t runDurationNanos = -1;
  uuid_t uuid;
  std::shared_ptr<core::Processor> processor = nullptr;

  if (!parentGroup) {
    logger_->log_error("parseProcessNodeYaml: no parent group exists");
    return;
  }

  if (processorsNode) {
    if (processorsNode.IsSequence()) {
      // Evaluate sequence of processors
      for (YAML::const_iterator iter = processorsNode.begin();
          iter != processorsNode.end(); ++iter) {
        core::ProcessorConfig procCfg;
        YAML::Node procNode = iter->as<YAML::Node>();

        checkRequiredField(&procNode, "name", CONFIG_YAML_PROCESSORS_KEY);
        checkRequiredField(&procNode, "class", CONFIG_YAML_PROCESSORS_KEY);

        procCfg.name = procNode["name"].as<std::string>();
        procCfg.id = getOrGenerateId(&procNode);
        uuid_parse(procCfg.id.c_str(), uuid);
        logger_->log_debug("parseProcessorNode: name => [%s] id => [%s]",
                           procCfg.name, procCfg.id);
        procCfg.javaClass = procNode["class"].as<std::string>();
        logger_->log_debug("parseProcessorNode: class => [%s]",
                           procCfg.javaClass);

        // Determine the processor name only from the Java class
        int lastOfIdx = procCfg.javaClass.find_last_of(".");
        if (lastOfIdx != std::string::npos) {
          lastOfIdx++;  // if a value is found, increment to move beyond the .
          int nameLength = procCfg.javaClass.length() - lastOfIdx;
          std::string processorName = procCfg.javaClass.substr(lastOfIdx,
                                                               nameLength);
          processor = this->createProcessor(processorName, uuid);
        }

        if (!processor) {
          logger_->log_error("Could not create a processor %s with id %s",
                             procCfg.name, procCfg.id);
          throw std::invalid_argument(
              "Could not create processor " + procCfg.name);
        }
        processor->setName(procCfg.name);

        procCfg.maxConcurrentTasks = procNode["max concurrent tasks"]
            .as<std::string>();
        logger_->log_debug("parseProcessorNode: max concurrent tasks => [%s]",
                           procCfg.maxConcurrentTasks);

        procCfg.schedulingStrategy = procNode["scheduling strategy"].as<std::string>();
        logger_->log_debug("parseProcessorNode: scheduling strategy => [%s]",
                           procCfg.schedulingStrategy);

        procCfg.schedulingPeriod = procNode["scheduling period"].as<std::string>();
        logger_->log_debug("parseProcessorNode: scheduling period => [%s]",
                           procCfg.schedulingPeriod);

        procCfg.penalizationPeriod = procNode["penalization period"].as<std::string>();
        logger_->log_debug("parseProcessorNode: penalization period => [%s]",
                           procCfg.penalizationPeriod);

        procCfg.yieldPeriod = procNode["yield period"].as<std::string>();
        logger_->log_debug("parseProcessorNode: yield period => [%s]",
                           procCfg.yieldPeriod);

        procCfg.yieldPeriod = procNode["run duration nanos"].as<std::string>();
        logger_->log_debug("parseProcessorNode: run duration nanos => [%s]",
                           procCfg.runDurationNanos);

        // handle auto-terminated relationships
        YAML::Node autoTerminatedSequence =
            procNode["auto-terminated relationships list"];
        std::vector<std::string> rawAutoTerminatedRelationshipValues;
        if (autoTerminatedSequence.IsSequence()
            && !autoTerminatedSequence.IsNull()
            && autoTerminatedSequence.size() > 0) {
          for (YAML::const_iterator relIter = autoTerminatedSequence.begin();
              relIter != autoTerminatedSequence.end(); ++relIter) {
            std::string autoTerminatedRel = relIter->as<std::string>();
            rawAutoTerminatedRelationshipValues.push_back(autoTerminatedRel);
          }
        }
        procCfg.autoTerminatedRelationships =
            rawAutoTerminatedRelationshipValues;

        // handle processor properties
        YAML::Node propertiesNode = procNode["Properties"];
        parsePropertiesNodeYaml(&propertiesNode, processor);

        // Take care of scheduling
        core::TimeUnit unit;
        if (core::Property::StringToTime(procCfg.schedulingPeriod,
                                         schedulingPeriod, unit)
            && core::Property::ConvertTimeUnitToNS(schedulingPeriod, unit,
                                                   schedulingPeriod)) {
          logger_->log_debug(
              "convert: parseProcessorNode: schedulingPeriod => [%d] ns",
              schedulingPeriod);
          processor->setSchedulingPeriodNano(schedulingPeriod);
        }

        if (core::Property::StringToTime(procCfg.penalizationPeriod,
                                         penalizationPeriod, unit)
            && core::Property::ConvertTimeUnitToMS(penalizationPeriod, unit,
                                                   penalizationPeriod)) {
          logger_->log_debug(
              "convert: parseProcessorNode: penalizationPeriod => [%d] ms",
              penalizationPeriod);
          processor->setPenalizationPeriodMsec(penalizationPeriod);
        }

        if (core::Property::StringToTime(procCfg.yieldPeriod, yieldPeriod, unit)
            && core::Property::ConvertTimeUnitToMS(yieldPeriod, unit,
                                                   yieldPeriod)) {
          logger_->log_debug(
              "convert: parseProcessorNode: yieldPeriod => [%d] ms",
              yieldPeriod);
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

        int64_t maxConcurrentTasks;
        if (core::Property::StringToInt(procCfg.maxConcurrentTasks,
                                        maxConcurrentTasks)) {
          logger_->log_debug("parseProcessorNode: maxConcurrentTasks => [%d]",
                             maxConcurrentTasks);
          processor->setMaxConcurrentTasks((uint8_t) maxConcurrentTasks);
        }

        if (core::Property::StringToInt(procCfg.runDurationNanos,
                                        runDurationNanos)) {
          logger_->log_debug("parseProcessorNode: runDurationNanos => [%d]",
                             runDurationNanos);
          processor->setRunDurationNano((uint64_t) runDurationNanos);
        }

        std::set<core::Relationship> autoTerminatedRelationships;
        for (auto &&relString : procCfg.autoTerminatedRelationships) {
          core::Relationship relationship(relString, "");
          logger_->log_debug(
              "parseProcessorNode: autoTerminatedRelationship  => [%s]",
              relString);
          autoTerminatedRelationships.insert(relationship);
        }

        processor->setAutoTerminatedRelationships(autoTerminatedRelationships);

        parentGroup->addProcessor(processor);
      }
    }
  } else {
    throw new std::invalid_argument(
        "Cannot instantiate a MiNiFi instance without a defined "
            "Processors configuration node.");
  }
}

void YamlConfiguration::parseRemoteProcessGroupYaml(
    YAML::Node *rpgNode, core::ProcessGroup * parentGroup) {
  uuid_t uuid;
  std::string id;

  if (!parentGroup) {
    logger_->log_error("parseRemoteProcessGroupYaml: no parent group exists");
    return;
  }

  if (rpgNode) {
    if (rpgNode->IsSequence()) {
      for (YAML::const_iterator iter = rpgNode->begin(); iter != rpgNode->end();
          ++iter) {
        YAML::Node currRpgNode = iter->as<YAML::Node>();

        auto name = currRpgNode["name"].as<std::string>();
        id = getOrGenerateId(&currRpgNode);

        logger_->log_debug(
            "parseRemoteProcessGroupYaml: name => [%s], id => [%s]", name, id);

        std::string url = currRpgNode["url"].as<std::string>();
        logger_->log_debug("parseRemoteProcessGroupYaml: url => [%s]", url);

        std::string timeout = currRpgNode["timeout"].as<std::string>();
        logger_->log_debug("parseRemoteProcessGroupYaml: timeout => [%s]",
                           timeout);

        std::string yieldPeriod = currRpgNode["yield period"].as<std::string>();
        logger_->log_debug("parseRemoteProcessGroupYaml: yield period => [%s]",
                           yieldPeriod);

        YAML::Node inputPorts = currRpgNode["Input Ports"].as<YAML::Node>();
        YAML::Node outputPorts = currRpgNode["Output Ports"].as<YAML::Node>();
        core::ProcessGroup *group = NULL;

        uuid_parse(id.c_str(), uuid);

        int64_t timeoutValue = -1;
        int64_t yieldPeriodValue = -1;

        group = this->createRemoteProcessGroup(name.c_str(), uuid).release();
        group->setParent(parentGroup);
        parentGroup->addProcessGroup(group);

        core::TimeUnit unit;

        if (core::Property::StringToTime(yieldPeriod, yieldPeriodValue, unit)
            && core::Property::ConvertTimeUnitToMS(yieldPeriodValue, unit,
                                                   yieldPeriodValue) && group) {
          logger_->log_debug(
              "parseRemoteProcessGroupYaml: yieldPeriod => [%d] ms",
              yieldPeriodValue);
          group->setYieldPeriodMsec(yieldPeriodValue);
        }

        if (core::Property::StringToTime(timeout, timeoutValue, unit)
            && core::Property::ConvertTimeUnitToMS(timeoutValue, unit,
                                                   timeoutValue) && group) {
          logger_->log_debug(
              "parseRemoteProcessGroupYaml: timeoutValue => [%d] ms",
              timeoutValue);
          group->setTimeOut(timeoutValue);
        }

        group->setTransmitting(true);
        group->setURL(url);

        if (inputPorts && inputPorts.IsSequence()) {
          for (YAML::const_iterator portIter = inputPorts.begin();
              portIter != inputPorts.end(); ++portIter) {
            logger_->log_debug("Got a current port, iterating...");

            YAML::Node currPort = portIter->as<YAML::Node>();

            this->parsePortYaml(&currPort, group, SEND);
          }  // for node
        }
        if (outputPorts && outputPorts.IsSequence()) {
          for (YAML::const_iterator portIter = outputPorts.begin();
              portIter != outputPorts.end(); ++portIter) {
            logger_->log_debug("Got a current port, iterating...");

            YAML::Node currPort = portIter->as<YAML::Node>();

            this->parsePortYaml(&currPort, group, RECEIVE);
          }  // for node
        }
      }
    }
  }
}

void YamlConfiguration::parseProvenanceReportingYaml(
    YAML::Node *reportNode, core::ProcessGroup * parentGroup) {
  uuid_t port_uuid;
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
  std::shared_ptr<core::reporting::SiteToSiteProvenanceReportingTask> reportTask =
      std::static_pointer_cast < core::reporting::SiteToSiteProvenanceReportingTask
      > (processor);

  YAML::Node node = reportNode->as<YAML::Node>();

  checkRequiredField(&node, "scheduling strategy", CONFIG_YAML_PROVENANCE_REPORT_KEY);
  auto schedulingStrategyStr = node["scheduling strategy"].as<std::string>();
  checkRequiredField(&node, "scheduling period", CONFIG_YAML_PROVENANCE_REPORT_KEY);
  auto schedulingPeriodStr = node["scheduling period"].as<std::string>();
  checkRequiredField(&node, "host", CONFIG_YAML_PROVENANCE_REPORT_KEY);
  auto hostStr = node["host"].as<std::string>();
  checkRequiredField(&node, "port", CONFIG_YAML_PROVENANCE_REPORT_KEY);
  auto portStr = node["port"].as<std::string>();
  checkRequiredField(&node, "port uuid", CONFIG_YAML_PROVENANCE_REPORT_KEY);
  auto portUUIDStr = node["port uuid"].as<std::string>();
  checkRequiredField(&node, "batch size", CONFIG_YAML_PROVENANCE_REPORT_KEY);
  auto batchSizeStr = node["batch size"].as<std::string>();

  // add processor to parent
  parentGroup->addProcessor(processor);
  processor->setScheduledState(core::RUNNING);

  core::TimeUnit unit;
  if (core::Property::StringToTime(schedulingPeriodStr, schedulingPeriod, unit) &&
      core::Property::ConvertTimeUnitToNS(schedulingPeriod, unit, schedulingPeriod)) {
    logger_->log_debug(
        "ProvenanceReportingTask schedulingPeriod %d ns",
        schedulingPeriod);
    processor->setSchedulingPeriodNano(schedulingPeriod);
  }

  if (schedulingStrategyStr == "TIMER_DRIVEN") {
     processor->setSchedulingStrategy(core::TIMER_DRIVEN);
     logger_->log_debug(
         "ProvenanceReportingTask scheduling strategy %s", schedulingStrategyStr);
  } else {
    throw std::invalid_argument(
        "Invalid scheduling strategy " +  schedulingStrategyStr);
  }

  reportTask->setHost(hostStr);
  logger_->log_debug("ProvenanceReportingTask host %s", hostStr);
  int64_t lvalue;
  if (core::Property::StringToInt(portStr, lvalue)) {
    logger_->log_debug("ProvenanceReportingTask port %d", (uint16_t) lvalue);
    reportTask->setPort((uint16_t) lvalue);
  }
  logger_->log_debug("ProvenanceReportingTask port uuid %s", portUUIDStr);
  uuid_parse(portUUIDStr.c_str(), port_uuid);
  reportTask->setPortUUID(port_uuid);
  if (core::Property::StringToInt(batchSizeStr, lvalue)) {
    reportTask->setBatchSize(lvalue);
  }
}

void YamlConfiguration::parseConnectionYaml(
    YAML::Node *connectionsNode,
    core::ProcessGroup *parent) {
  if (!parent) {
    logger_->log_error("parseProcessNode: no parent group was provided");
    return;
  }

  if (connectionsNode) {
    if (connectionsNode->IsSequence()) {
      for (YAML::const_iterator iter = connectionsNode->begin();
          iter != connectionsNode->end(); ++iter) {
        YAML::Node connectionNode = iter->as<YAML::Node>();
        std::shared_ptr<minifi::Connection> connection = nullptr;

        // Configure basic connection
        uuid_t uuid;
        std::string name = connectionNode["name"].as<std::string>();
        std::string id = getOrGenerateId(&connectionNode);
        uuid_parse(id.c_str(), uuid);
        connection = this->createConnection(name, uuid);
        logger_->log_debug(
            "Created connection with UUID %s and name %s", id, name);


        // Configure connection source
        auto rawRelationship = connectionNode["source relationship name"]
            .as<std::string>();
        core::Relationship relationship(rawRelationship, "");
        logger_->log_debug(
            "parseConnection: relationship => [%s]", rawRelationship);
        if (connection) {
          connection->setRelationship(relationship);
        }

        uuid_t srcUUID;
        std::string connectionSrcProcName = connectionNode["source name"]
            .as<std::string>();
        if (connectionNode["source id"]) {
          std::string connectionSrcProcId = connectionNode["source id"]
              .as<std::string>();

          uuid_parse(connectionSrcProcId.c_str(), srcUUID);
        } else {
          // if we don't have a source id, try harder to resolve the source processor.
          // config schema v2 will make this unnecessary
          uuid_t tmpUUID;
          if (!uuid_parse(connectionSrcProcName.c_str(), tmpUUID) &&
              NULL != parent->findProcessor(tmpUUID)) {
            // the source name is a remote port id, so use that as the source id
            uuid_copy(srcUUID, tmpUUID);
          } else {
            // lastly, look the processor up by name
            auto srcProcessor = parent->findProcessor(connectionSrcProcName);
            if (NULL != srcProcessor) {
              srcProcessor->getUUID(srcUUID);
            } else {
              // we ran out of ways to discover the source processor
              logger_->log_error(
                  "Could not locate a source with name %s to create a connection",
                  connectionSrcProcName);
              throw std::invalid_argument(
                  "Could not locate a source with name " +
                      connectionSrcProcName + " to create a connection ");
            }
          }
        }
        connection->setSourceUUID(srcUUID);

        // Configure connection destination
        uuid_t destUUID;
        std::string connectionDestProcName = connectionNode["destination name"]
            .as<std::string>();
        if (connectionNode["destination id"]) {
          std::string connectionDestProcId = connectionNode["destination id"]
              .as<std::string>();
          uuid_parse(connectionDestProcId.c_str(), destUUID);
        } else {
          // we use the same logic as above for resolving the source processor
          // for looking up the destination processor in absence of a processor id
          uuid_t tmpUUID;
          if (!uuid_parse(connectionDestProcName.c_str(), tmpUUID) &&
              NULL != parent->findProcessor(tmpUUID)) {
            // the destination name is a remote port id, so use that as the dest id
            uuid_copy(destUUID, tmpUUID);
          } else {
            // look the processor up by name
            auto destProcessor = parent->findProcessor(connectionDestProcName);
            if (NULL != destProcessor) {
              destProcessor->getUUID(destUUID);
            } else {
              // we ran out of ways to discover the destination processor
              logger_->log_error(
                  "Could not locate a destination with name %s to create a connection",
                  connectionDestProcName);
              throw std::invalid_argument(
                  "Could not locate a destination with name " +
                      connectionDestProcName + " to create a connection");
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

void YamlConfiguration::parsePortYaml(YAML::Node *portNode,
                                      core::ProcessGroup *parent,
                                      TransferDirection direction) {
  uuid_t uuid;
  std::shared_ptr<core::Processor> processor = NULL;
  std::shared_ptr<minifi::RemoteProcessorGroupPort> port = NULL;

  if (!parent) {
    logger_->log_error("parseProcessNode: no parent group existed");
    return;
  }

  YAML::Node inputPortsObj = portNode->as<YAML::Node>();

  // Check for required fields
  checkRequiredField(&inputPortsObj, "name", CONFIG_YAML_REMOTE_PROCESS_GROUP_KEY);
  auto nameStr = inputPortsObj["name"].as<std::string>();
  checkRequiredField(&inputPortsObj, "id", "The field 'id' is required for "
      "the port named '" + nameStr + "' in the YAML Config. If this port "
      "is an input port for a NiFi Remote Process Group, the port "
      "id should match the corresponding id specified in the NiFi configuration. "
      "This is a UUID of the format XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX.");
  auto portId = inputPortsObj["id"].as<std::string>();
  uuid_parse(portId.c_str(), uuid);

  port = std::make_shared<minifi::RemoteProcessorGroupPort>(stream_factory_, nameStr, uuid);

  processor = std::static_pointer_cast<core::Processor>(port);
  port->setDirection(direction);
  port->setTimeOut(parent->getTimeOut());
  port->setTransmitting(true);
  processor->setYieldPeriodMsec(parent->getYieldPeriodMsec());
  processor->initialize();

  // handle port properties
  YAML::Node nodeVal = portNode->as<YAML::Node>();
  YAML::Node propertiesNode = nodeVal["Properties"];

  parsePropertiesNodeYaml(&propertiesNode, processor);

  // add processor to parent
  parent->addProcessor(processor);
  processor->setScheduledState(core::RUNNING);
  auto rawMaxConcurrentTasks = inputPortsObj["max concurrent tasks"]
      .as<std::string>();
  int64_t maxConcurrentTasks;
  if (core::Property::StringToInt(rawMaxConcurrentTasks, maxConcurrentTasks)) {
    processor->setMaxConcurrentTasks(maxConcurrentTasks);
  }
  logger_->log_debug("parseProcessorNode: maxConcurrentTasks => [%d]",
                     maxConcurrentTasks);
  processor->setMaxConcurrentTasks(maxConcurrentTasks);
}

void YamlConfiguration::parsePropertiesNodeYaml(
    YAML::Node *propertiesNode, std::shared_ptr<core::Processor> processor) {
  // Treat generically as a YAML node so we can perform inspection on entries to ensure they are populated
  for (YAML::const_iterator propsIter = propertiesNode->begin();
      propsIter != propertiesNode->end(); ++propsIter) {
    std::string propertyName = propsIter->first.as<std::string>();
    YAML::Node propertyValueNode = propsIter->second;
    if (!propertyValueNode.IsNull() && propertyValueNode.IsDefined()) {
      std::string rawValueString = propertyValueNode.as<std::string>();
      if (!processor->setProperty(propertyName, rawValueString)) {
        logger_->log_warn(
            "Received property %s with value %s but it is not one of the properties for %s",
            propertyName,
            rawValueString,
            processor->getName());
      }
    }
  }
}

std::string YamlConfiguration::getOrGenerateId(
    YAML::Node *yamlNode,
    const std::string &idField) {
  std::string id;
  YAML::Node node = yamlNode->as<YAML::Node>();

  if (node[idField]) {
    if (YAML::NodeType::Scalar == node[idField].Type()) {
      id = node[idField].as<std::string>();
    } else {
      throw std::invalid_argument(
          "getOrGenerateId: idField is expected to reference YAML::Node "
              "of YAML::NodeType::Scalar.");
    }
  } else {
    uuid_t uuid;
    uuid_generate(uuid);
    char uuid_str[37];
    uuid_unparse(uuid, uuid_str);
    id = uuid_str;
    logger_->log_debug("Generating random ID: id => [%s]", id);
  }
  return id;
}

void YamlConfiguration::checkRequiredField(
    YAML::Node *yamlNode,
    const std::string &fieldName,
    const std::string &yamlSection,
    const std::string &errorMessage) {

  std::string errMsg = errorMessage;
  if (!yamlNode->as<YAML::Node>()[fieldName]) {
    if (errMsg.empty()) {
      // Build a helpful error message for the user so they can fix the
      // invalid YAML config file, using the component name if present
      errMsg =
          yamlNode->as<YAML::Node>()["name"] ?
          "Unable to parse configuration file for component named '" +
              yamlNode->as<YAML::Node>()["name"].as<std::string>() +
              "' as required field '" + fieldName + "' is missing" :
          "Unable to parse configuration file as required field '" +
              fieldName + "' is missing";
      if (!yamlSection.empty()) {
        errMsg += " [in '" + yamlSection +
            "' section of configuration file]";
      }
    }
    logger_->log_error(errorMessage.c_str());
    throw std::invalid_argument(errorMessage);
  }
}

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
