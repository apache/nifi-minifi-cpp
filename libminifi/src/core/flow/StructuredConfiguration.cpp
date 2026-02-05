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

#include "core/flow/StructuredConfiguration.h"

#include <memory>
#include <set>
#include <vector>

#include "Funnel.h"
#include "core/ParameterContext.h"
#include "core/Processor.h"
#include "core/ParameterTokenParser.h"
#include "core/ReferenceParser.h"
#include "core/flow/CheckRequiredField.h"
#include "core/flow/StructuredConnectionParser.h"
#include "core/state/Value.h"
#include "utils/RegexUtils.h"
#include "utils/TimeUtil.h"
#include "utils/crypto/property_encryption/PropertyEncryptionUtils.h"
#include "utils/PropertyErrors.h"

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
  auto process_group = getRootFromPayload(configuration.value());
  gsl_Expects(process_group);
  persist(*process_group);
  return process_group;
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

  logger_->log_debug("parseRootProcessGroup: id => [{}], name => [{}]", uuid.to_string(), flowName);
  std::unique_ptr<core::ProcessGroup> group;
  if (is_root) {
    group = FlowConfiguration::createRootProcessGroup(flowName, uuid, version);
  } else {
    group = FlowConfiguration::createSimpleProcessGroup(flowName, uuid, version);
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
  Node parameterContextNameNode = node[schema_.parameter_context_name];
  Node controllerServiceNode = node[schema_.controller_services];

  parseParameterContext(parameterContextNameNode, *group);
  parseControllerServices(controllerServiceNode, group.get());
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
    Node parameterContextsNode = root_node[schema_.parameter_contexts];
    Node parameterProvidersNode = root_node[schema_.parameter_providers];
    Node provenanceReportNode = root_node[schema_.provenance_reporting];

    parseParameterContexts(parameterContextsNode, parameterProvidersNode);
    // Create the root process group
    std::unique_ptr<core::ProcessGroup> root = parseRootProcessGroup(root_node);
    parseProvenanceReporting(provenanceReportNode, root.get());

    root->verify();

    return root;
  } catch (const std::exception& ex) {
    logger_->log_error("Error while processing configuration file: {}", ex.what());
    throw;
  }
}

namespace {
bool hasInheritanceCycle(const ParameterContext& parameter_context, std::unordered_set<std::string>& visited_parameter_contexts, std::unordered_set<std::string>& current_stack) {
  if (current_stack.contains(parameter_context.getName())) {
    return true;
  }

  if (visited_parameter_contexts.contains(parameter_context.getName())) {
    return false;
  }

  current_stack.insert(parameter_context.getName());
  visited_parameter_contexts.insert(parameter_context.getName());

  for (const auto& inherited_parameter_context : parameter_context.getInheritedParameterContexts()) {
    if (hasInheritanceCycle(*inherited_parameter_context, visited_parameter_contexts, current_stack)) {
      return true;
    }
  }

  current_stack.erase(parameter_context.getName());

  return false;
}
}  // namespace

void StructuredConfiguration::verifyNoInheritanceCycles() const {
  std::unordered_set<std::string> visited_parameter_contexts;
  std::unordered_set<std::string> current_stack;
  for (const auto& [parameter_context_name, parameter_context] : parameter_contexts_) {
    if (hasInheritanceCycle(*parameter_context, visited_parameter_contexts, current_stack)) {
      throw std::invalid_argument("Circular references in Parameter Context inheritance are not allowed. Inheritance cycle was detected in parameter context '" + parameter_context_name + "'");
    }
  }
}

void StructuredConfiguration::parseParameterContextsNode(const Node& parameter_contexts_node) {
  if (!parameter_contexts_node || !parameter_contexts_node.isSequence()) {
    return;
  }
  for (const auto& parameter_context_node : parameter_contexts_node) {
    checkRequiredField(parameter_context_node, schema_.name);

    auto name = parameter_context_node[schema_.name].getString().value();
    if (parameter_contexts_.contains(name)) {
      throw std::invalid_argument("Parameter context name '" + name + "' already exists, parameter context names must be unique!");
    }
    auto id = getRequiredIdField(parameter_context_node);

    utils::Identifier uuid;
    uuid = id;
    auto parameter_context = std::make_unique<ParameterContext>(name, uuid);
    parameter_context->setDescription(getOptionalField(parameter_context_node, schema_.description, ""));
    parameter_context->setParameterProvider(getOptionalField(parameter_context_node, schema_.parameter_provider, ""));
    for (const auto& parameter_node : parameter_context_node[schema_.parameters]) {
      checkRequiredField(parameter_node, schema_.name);
      checkRequiredField(parameter_node, schema_.value);
      checkRequiredField(parameter_node, schema_.sensitive);
      auto parameter_name = parameter_node[schema_.name].getString().value();
      auto parameter_value = parameter_node[schema_.value].getString().value();
      auto sensitive = parameter_node[schema_.sensitive].getBool().value();
      auto provided = parameter_node[schema_.provided].getBool().value_or(false);
      auto parameter_description = getOptionalField(parameter_node, schema_.description, "");
      if (sensitive) {
        parameter_value = utils::crypto::property_encryption::decrypt(parameter_value, sensitive_values_encryptor_);
      }
      parameter_context->addParameter(Parameter{
        .name = parameter_name,
        .description = parameter_description,
        .sensitive = sensitive,
        .provided = provided,
        .value = parameter_value});
    }

    parameter_contexts_.emplace(name, gsl::make_not_null(std::move(parameter_context)));
  }
}

void StructuredConfiguration::parseParameterProvidersNode(const Node& parameter_providers_node) {
  if (!parameter_providers_node || !parameter_providers_node.isSequence()) {
    return;
  }
  for (const auto& parameter_provider_node : parameter_providers_node) {
    checkRequiredField(parameter_provider_node, schema_.name);

    auto type = getRequiredField(parameter_provider_node, schema_.type);
    logger_->log_debug("Using type {} for parameter provider node", type);

    std::string fullType = type;
    type = utils::string::partAfterLastOccurrenceOf(type, '.');

    auto name = parameter_provider_node[schema_.name].getString().value();
    auto id = getRequiredIdField(parameter_provider_node);

    utils::Identifier uuid;
    uuid = id;
    auto parameter_provider = createParameterProvider(type, fullType, uuid);
    if (nullptr != parameter_provider) {
      logger_->log_debug("Created Parameter Provider with UUID {} and name {}", id, name);
      if (Node propertiesNode = parameter_provider_node[schema_.parameter_provider_properties]) {
        parsePropertiesNode(propertiesNode, *parameter_provider, name, nullptr);
      }
    } else {
      logger_->log_debug("Could not locate {}", type);
    }
    parameter_provider->setName(name);
    auto parameter_contexts = parameter_provider->createParameterContexts();
    for (auto& parameter_context : parameter_contexts) {
      auto it = parameter_contexts_.find(parameter_context->getName());
      if (it == parameter_contexts_.end()) {
        parameter_contexts_.emplace(parameter_context->getName(), std::move(parameter_context));
      } else if (it->second->getParameterProvider() != parameter_provider->getUUIDStr()) {
        throw std::invalid_argument(fmt::format("Parameter provider '{}' cannot create parameter context '{}' because parameter context already exists "
          "with no parameter provider or generated by other parameter provider", parameter_provider->getName(), parameter_context->getName()));
      } else if (parameter_provider->reloadValuesOnRestart()) {
        it->second = std::move(parameter_context);
      }
    }
    parameter_providers_.push_back(gsl::make_not_null(std::move(parameter_provider)));
  }
}

void StructuredConfiguration::parseParameterContextInheritance(const Node& parameter_contexts_node) {
  if (!parameter_contexts_node || !parameter_contexts_node.isSequence()) {
    return;
  }
  for (const auto& parameter_context_node : parameter_contexts_node) {
    if (!isFieldPresent(parameter_context_node, schema_.inherited_parameter_contexts[0])) {
      continue;
    }
    auto inherited_parameters_node = parameter_context_node[schema_.inherited_parameter_contexts];
    for (const auto& inherited_parameter_context_name : inherited_parameters_node) {
      auto name = inherited_parameter_context_name.getString().value();
      if (!parameter_contexts_.contains(name)) {
        throw std::invalid_argument("Inherited parameter context '" + name + "' does not exist!");
      }

      auto parameter_context_name = parameter_context_node[schema_.name].getString().value();
      if (parameter_context_name == name) {
        throw std::invalid_argument("Inherited parameter context '" + name + "' cannot be the same as the parameter context!");
      }
      gsl::not_null<ParameterContext*> context = gsl::make_not_null(parameter_contexts_.at(name).get());
      parameter_contexts_.at(parameter_context_name)->addInheritedParameterContext(context);
    }
  }
  verifyNoInheritanceCycles();
}

void StructuredConfiguration::parseParameterContexts(const Node& parameter_contexts_node, const Node& parameter_providers_node) {
  parseParameterContextsNode(parameter_contexts_node);
  parseParameterProvidersNode(parameter_providers_node);
  parseParameterContextInheritance(parameter_contexts_node);
}

void StructuredConfiguration::parseProcessorNode(const Node& processors_node, core::ProcessGroup* parentGroup) {
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
    logger_->log_debug("parseProcessorNode: name => [{}] id => [{}]", procCfg.name, procCfg.id);
    checkRequiredField(procNode, schema_.type);
    procCfg.javaClass = procNode[schema_.type].getString().value();
    logger_->log_debug("parseProcessorNode: class => [{}]", procCfg.javaClass);

    // Determine the processor name only from the Java class
    processor = createProcessor(utils::string::partAfterLastOccurrenceOf(procCfg.javaClass, '.'), procCfg.javaClass, procCfg.name, uuid);
    if (!processor) {
      logger_->log_error("Could not create a processor {} with id {}", procCfg.name, procCfg.id);
      throw std::invalid_argument("Could not create processor " + procCfg.name);
    }

    processor->setName(procCfg.name);

    processor->setFlowIdentifier(flow_version_->getFlowIdentifier());

    procCfg.schedulingStrategy = getOptionalField(procNode, schema_.scheduling_strategy, DEFAULT_SCHEDULING_STRATEGY);
    logger_->log_debug("parseProcessorNode: scheduling strategy => [{}]", procCfg.schedulingStrategy);

    procCfg.schedulingPeriod = getOptionalField(procNode, schema_.scheduling_period, DEFAULT_SCHEDULING_PERIOD_STR);

    logger_->log_debug("parseProcessorNode: scheduling period => [{}]", procCfg.schedulingPeriod);

    if (auto tasksNode = procNode[schema_.max_concurrent_tasks]) {
      procCfg.maxConcurrentTasks = tasksNode.getIntegerAsString().value();
      logger_->log_debug("parseProcessorNode: max concurrent tasks => [{}]", procCfg.maxConcurrentTasks);
    }

    if (auto penalizationNode = procNode[schema_.penalization_period]) {
      procCfg.penalizationPeriod = penalizationNode.getString().value();
      logger_->log_debug("parseProcessorNode: penalization period => [{}]", procCfg.penalizationPeriod);
    }

    if (auto yieldNode = procNode[schema_.proc_yield_period]) {
      procCfg.yieldPeriod = yieldNode.getString().value();
      logger_->log_debug("parseProcessorNode: yield period => [{}]", procCfg.yieldPeriod);
    }

    if (auto bulletin_level_node = procNode[schema_.bulletin_level]) {
      procCfg.bulletinLevel = bulletin_level_node.getString().value();
      logger_->log_debug("parseProcessorNode: bulletin level => [{}]", procCfg.bulletinLevel);
    }

    if (auto runNode = procNode[schema_.runduration_nanos]) {
      procCfg.runDurationNanos = runNode.getIntegerAsString().value();
      logger_->log_debug("parseProcessorNode: run duration nanos => [{}]", procCfg.runDurationNanos);
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
      parsePropertiesNode(propertiesNode, *processor, procCfg.name, parentGroup->getParameterContext());
    }

    // Take care of scheduling

    if (procCfg.schedulingStrategy == "TIMER_DRIVEN" || procCfg.schedulingStrategy == "EVENT_DRIVEN") {
      if (auto scheduling_period = utils::timeutils::StringToDuration<std::chrono::nanoseconds>(procCfg.schedulingPeriod)) {
        logger_->log_debug("convert: parseProcessorNode: schedulingPeriod => [{}]", scheduling_period);
        processor->setSchedulingPeriod(*scheduling_period);
      }
    } else {
      processor->setCronPeriod(procCfg.schedulingPeriod);
    }

    if (auto penalization_period = utils::timeutils::StringToDuration<std::chrono::milliseconds>(procCfg.penalizationPeriod)) {
      logger_->log_debug("convert: parseProcessorNode: penalizationPeriod => [{}]", penalization_period);
      processor->setPenalizationPeriod(penalization_period.value());
    }

    if (auto yield_period = utils::timeutils::StringToDuration<std::chrono::milliseconds>(procCfg.yieldPeriod)) {
      logger_->log_debug("convert: parseProcessorNode: yieldPeriod => [{}]", yield_period);
      processor->setYieldPeriodMsec(yield_period.value());
    }

    if (!procCfg.bulletinLevel.empty()) {
      processor->setLogBulletinLevel(core::logging::mapStringToLogLevel(procCfg.bulletinLevel));
    }
    processor->setLoggerCallback([this, processor = processor.get()](core::logging::LOG_LEVEL level, const std::string& message) {
      if (level < processor->getLogBulletinLevel()) {
        return;
      }
      if (bulletin_store_) {
        bulletin_store_->addProcessorBulletin(*processor, level, message);
      }
    });

    // Default to running
    processor->setScheduledState(core::RUNNING);

    if (procCfg.schedulingStrategy == "TIMER_DRIVEN") {
      processor->setSchedulingStrategy(core::TIMER_DRIVEN);
      logger_->log_debug("setting scheduling strategy as {}", procCfg.schedulingStrategy);
    } else if (procCfg.schedulingStrategy == "EVENT_DRIVEN") {
      processor->setSchedulingStrategy(core::EVENT_DRIVEN);
      logger_->log_debug("setting scheduling strategy as {}", procCfg.schedulingStrategy);
    } else {
      processor->setSchedulingStrategy(core::CRON_DRIVEN);
      logger_->log_debug("setting scheduling strategy as {}", procCfg.schedulingStrategy);
    }

    if (auto max_concurrent_tasks = parsing::parseIntegral<uint8_t>(procCfg.maxConcurrentTasks)) {
      logger_->log_debug("parseProcessorNode: maxConcurrentTasks => [{}]", *max_concurrent_tasks);
      processor->setMaxConcurrentTasks(*max_concurrent_tasks);
    }

    if (auto run_duration_nanos = parsing::parseIntegral<uint64_t>(procCfg.runDurationNanos)) {
      logger_->log_debug("parseProcessorNode: runDurationNanos => [{}]", *run_duration_nanos);
      processor->setRunDurationNano(std::chrono::nanoseconds(*run_duration_nanos));
    }

    std::vector<core::Relationship> autoTerminatedRelationships;
    for (auto &&relString : procCfg.autoTerminatedRelationships) {
      core::Relationship relationship(relString, "");
      logger_->log_debug("parseProcessorNode: autoTerminatedRelationship  => [{}]", relString);
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

    logger_->log_debug("parseRemoteProcessGroup: name => [{}], id => [{}]", name, id);

    auto url = getOptionalField(currRpgNode, schema_.rpg_url, "");

    logger_->log_debug("parseRemoteProcessGroup: url => [{}]", url);

    uuid = id;
    auto group = createRemoteProcessGroup(name, uuid);
    group->setParent(parentGroup);

    if (currRpgNode[schema_.rpg_yield_period]) {
      auto yieldPeriod = currRpgNode[schema_.rpg_yield_period].getString().value();
      logger_->log_debug("parseRemoteProcessGroup: yield period => [{}]", yieldPeriod);

      auto yield_period_value = utils::timeutils::StringToDuration<std::chrono::milliseconds>(yieldPeriod);
      if (yield_period_value.has_value() && group) {
        logger_->log_debug("parseRemoteProcessGroup: yieldPeriod => [{}]", yield_period_value);
        group->setYieldPeriodMsec(*yield_period_value);
      }
    }

    if (currRpgNode[schema_.rpg_timeout]) {
      auto timeout = currRpgNode[schema_.rpg_timeout].getString().value();
      logger_->log_debug("parseRemoteProcessGroup: timeout => [{}]", timeout);

      auto timeout_value = utils::timeutils::StringToDuration<std::chrono::milliseconds>(timeout);
      if (timeout_value.has_value() && group) {
        logger_->log_debug("parseRemoteProcessGroup: timeoutValue => [{}]", timeout_value);
        group->setTimeout(timeout_value->count());
      }
    }

    if (currRpgNode[schema_.rpg_local_network_interface]) {
      auto interface = currRpgNode[schema_.rpg_local_network_interface].getString().value();
      logger_->log_debug("parseRemoteProcessGroup: local network interface => [{}]", interface);
      group->setInterface(interface);
    }

    if (currRpgNode[schema_.rpg_transport_protocol]) {
      auto transport_protocol = currRpgNode[schema_.rpg_transport_protocol].getString().value();
      logger_->log_debug("parseRemoteProcessGroup: transport protocol => [{}]", transport_protocol);
      if (transport_protocol == "HTTP") {
        group->setTransportProtocol(transport_protocol);
        if (currRpgNode[schema_.rpg_proxy_host]) {
          auto http_proxy_host = currRpgNode[schema_.rpg_proxy_host].getString().value();
          logger_->log_debug("parseRemoteProcessGroup: proxy host => [{}]", http_proxy_host);
          group->setHttpProxyHost(http_proxy_host);
          if (currRpgNode[schema_.rpg_proxy_user]) {
            auto http_proxy_username = currRpgNode[schema_.rpg_proxy_user].getString().value();
            logger_->log_debug("parseRemoteProcessGroup: proxy user => [{}]", http_proxy_username);
            group->setHttpProxyUserName(http_proxy_username);
          }
          if (currRpgNode[schema_.rpg_proxy_password]) {
            auto http_proxy_password = currRpgNode[schema_.rpg_proxy_password].getString().value();
            logger_->log_debug("parseRemoteProcessGroup: proxy password => [{}]", http_proxy_password);
            group->setHttpProxyPassWord(http_proxy_password);
          }
          if (currRpgNode[schema_.rpg_proxy_port]) {
            auto http_proxy_port = currRpgNode[schema_.rpg_proxy_port].getIntegerAsString().value();
            if (auto port = parsing::parseIntegral<int>(http_proxy_port)) {
              logger_->log_debug("parseRemoteProcessGroup: proxy port => [{}]", *port);
              group->setHttpProxyPort(*port);
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

    std::vector<std::string> port_nodes(std::begin(schema_.rpg_input_ports), std::end(schema_.rpg_input_ports));
    port_nodes.insert(std::end(port_nodes), std::begin(schema_.rpg_output_ports), std::end(schema_.rpg_output_ports));
    checkRequiredField(currRpgNode, port_nodes);
    auto inputPorts = currRpgNode[schema_.rpg_input_ports];
    if (inputPorts && inputPorts.isSequence()) {
      for (const auto& currPort : inputPorts) {
        parseRPGPort(currPort, group.get(), sitetosite::TransferDirection::SEND);
      }  // for node
    }
    auto outputPorts = currRpgNode[schema_.rpg_output_ports];
    if (outputPorts && outputPorts.isSequence()) {
      for (const auto& currPort : outputPorts) {
        logger_->log_debug("Got a current port, iterating...");

        parseRPGPort(currPort, group.get(), sitetosite::TransferDirection::RECEIVE);
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

  auto report_task = createProvenanceReportTask();

  checkRequiredField(node, schema_.scheduling_strategy);
  auto schedulingStrategyStr = node[schema_.scheduling_strategy].getString().value();
  checkRequiredField(node, schema_.scheduling_period);
  auto schedulingPeriodStr = node[schema_.scheduling_period].getString().value();

  if (auto scheduling_period = utils::timeutils::StringToDuration<std::chrono::nanoseconds>(schedulingPeriodStr)) {
    logger_->log_debug("ProvenanceReportingTask schedulingPeriod {}", scheduling_period);
    report_task->setSchedulingPeriod(*scheduling_period);
  }

  if (schedulingStrategyStr == "TIMER_DRIVEN") {
    report_task->setSchedulingStrategy(core::TIMER_DRIVEN);
    logger_->log_debug("ProvenanceReportingTask scheduling strategy {}", schedulingStrategyStr);
  } else {
    throw std::invalid_argument("Invalid scheduling strategy " + schedulingStrategyStr);
  }

  if (node["host"] && node["port"]) {
    auto hostStr = node["host"].getString().value();

    std::string portStr = node["port"].getIntegerAsString().value();
    if (auto port = parsing::parseIntegral<int64_t>(portStr); port && !hostStr.empty()) {
      logger_->log_debug("ProvenanceReportingTask port {}", *port);
      std::string url = hostStr + ":" + portStr;
      report_task->getImpl<core::reporting::SiteToSiteProvenanceReportingTask>().setURL(url);
    }
  }

  if (node["url"]) {
    auto urlStr = node["url"].getString().value();
    if (!urlStr.empty()) {
      report_task->getImpl<core::reporting::SiteToSiteProvenanceReportingTask>().setURL(urlStr);
      logger_->log_debug("ProvenanceReportingTask URL {}", urlStr);
    }
  }
  checkRequiredField(node, schema_.provenance_reporting_port_uuid);
  auto portUUIDStr = node[schema_.provenance_reporting_port_uuid].getString().value();
  checkRequiredField(node, schema_.provenance_reporting_batch_size);
  auto batchSizeStr = node[schema_.provenance_reporting_batch_size].getString().value();

  logger_->log_debug("ProvenanceReportingTask port uuid {}", portUUIDStr);
  port_uuid = portUUIDStr;
  report_task->getImpl<core::reporting::SiteToSiteProvenanceReportingTask>().setPortUUID(port_uuid);

  if (auto batch_size = parsing::parseIntegral<int>(batchSizeStr)) {
    report_task->getImpl<core::reporting::SiteToSiteProvenanceReportingTask>().setBatchSize(*batch_size);
  }

  // add processor to parent
  report_task->setScheduledState(core::RUNNING);
  parent_group->addProcessor(std::move(report_task));
}

void StructuredConfiguration::parseControllerServices(const Node& controller_services_node, core::ProcessGroup* parent_group) {
  if (!controller_services_node || !controller_services_node.isSequence()) {
    return;
  }
  for (const auto& service_node : controller_services_node) {
    checkRequiredField(service_node, schema_.name);

    auto type = getRequiredField(service_node, schema_.type);
    logger_->log_debug("Using type {} for controller service node", type);

    type = utils::string::partAfterLastOccurrenceOf(type, '.');

    auto name = service_node[schema_.name].getString().value();
    auto id = getRequiredIdField(service_node);

    utils::Identifier uuid;
    uuid = id;
    std::shared_ptr<core::controller::ControllerServiceNode> controller_service_node = createControllerService(type, name, uuid, parent_group);
    if (nullptr != controller_service_node) {
      logger_->log_debug("Created Controller Service with UUID {} and name {}", id, name);
      controller_service_node->initialize();
      if (Node propertiesNode = service_node[schema_.controller_service_properties]) {
        // we should propagate properties to the node and to the implementation
        parsePropertiesNode(propertiesNode, *controller_service_node, name, parent_group->getParameterContext());
        if (auto controllerServiceImpl = controller_service_node->getControllerServiceImplementation(); controllerServiceImpl) {
          parsePropertiesNode(propertiesNode, *controllerServiceImpl, name, parent_group->getParameterContext());
        }
      }

      parent_group->addControllerService(controller_service_node->getName(), controller_service_node);
      parent_group->addControllerService(controller_service_node->getUUIDStr(), controller_service_node);
    } else {
      logger_->log_debug("Could not locate {}", type);
    }
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
    logger_->log_debug("Created connection with UUID {} and name {}", id, name);
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

  auto port_impl = std::make_unique<minifi::RemoteProcessGroupPort>(
          nameStr, parent->getURL(), this->configuration_, uuid);
  auto* port = port_impl.get();
  auto port_wrapper = std::make_unique<core::Processor>(nameStr, uuid, std::move(port_impl));
  port->setDirection(direction);
  port->setTimeout(std::chrono::milliseconds(parent->getTimeout()));
  port->setTransmitting(true);
  port_wrapper->setYieldPeriodMsec(parent->getYieldPeriodMsec());

  if (isFieldPresent(port_node, schema_.rpg_port_use_compression[0])) {
    auto use_compression = port_node[schema_.rpg_port_use_compression].getBool().value_or(false);
    logger_->log_debug("parseRPGPort: use compression => [{}]", use_compression);
    port->setUseCompression(use_compression);
  }

  if (isFieldPresent(port_node, schema_.rpg_port_batch_size[0])) {
    if (isFieldPresent(port_node[schema_.rpg_port_batch_size[0]], schema_.rpg_port_batch_size_count[0])) {
      auto batch_count_str = port_node[schema_.rpg_port_batch_size[0]][schema_.rpg_port_batch_size_count[0]].getIntegerAsString().value();
      if (auto batch_count = parsing::parseIntegral<uint64_t>(batch_count_str); batch_count) {
        logger_->log_debug("parseRPGPort: batch count => [{}]", *batch_count);
        port->setBatchCount(*batch_count);
      }
    }

    if (isFieldPresent(port_node[schema_.rpg_port_batch_size[0]], schema_.rpg_port_batch_size_size[0])) {
      auto batch_size_str = port_node[schema_.rpg_port_batch_size[0]][schema_.rpg_port_batch_size_size[0]].getIntegerAsString().value();
      if (auto batch_size = parsing::parseDataSize(batch_size_str); batch_size) {
        logger_->log_debug("parseRPGPort: batch size => [{}]", *batch_size);
        port->setBatchSize(*batch_size);
      }
    }

    if (isFieldPresent(port_node[schema_.rpg_port_batch_size[0]], schema_.rpg_port_batch_size_duration[0])) {
      auto batch_duration_str = port_node[schema_.rpg_port_batch_size[0]][schema_.rpg_port_batch_size_duration[0]].getIntegerAsString().value();
      if (auto batch_duration = parsing::parseDuration(batch_duration_str); batch_duration) {
        logger_->log_debug("parseRPGPort: batch size => [{}]", *batch_duration);
        port->setBatchDuration(*batch_duration);
      }
    }
  }

  port_wrapper->initialize();
  if (!parent->getInterface().empty())
    port->setInterface(parent->getInterface());
  if (parent->getTransportProtocol() == "HTTP") {
    port->enableHTTP();
    if (!parent->getHttpProxyHost().empty())
      port->setHTTPProxy(parent->getHTTPProxy());
  }
  // else defaults to RAW

  // handle port properties
  if (const Node propertiesNode = port_node[schema_.rpg_port_properties]) {
    parsePropertiesNode(propertiesNode, *port_wrapper, nameStr, nullptr);
  } else {
    parsePropertyNodeElement(std::string(minifi::RemoteProcessGroupPort::portUUID.name), port_node[schema_.rpg_port_target_id], *port_wrapper, nullptr);
    validateComponentProperties(*port_wrapper, nameStr, port_node.getPath());
  }

  // add processor to parent
  auto& processor = *port_wrapper;
  parent->addProcessor(std::move(port_wrapper));
  processor.setScheduledState(core::RUNNING);

  if (auto tasksNode = port_node[schema_.max_concurrent_tasks]) {
    const std::string raw_max_concurrent_tasks = tasksNode.getIntegerAsString().value();
    if (auto max_concurrent_tasks = parsing::parseIntegral<uint8_t>(raw_max_concurrent_tasks); max_concurrent_tasks) {
      logger_->log_debug("parseProcessorNode: maxConcurrentTasks => [{}]", *max_concurrent_tasks);
      processor.setMaxConcurrentTasks(*max_concurrent_tasks);
    }
  }
}

static std::string resolveAssetReferences(std::string_view str, utils::file::AssetManager *asset_manager) {
  return resolveIdentifier(str, {getAssetResolver(asset_manager)});
}

void StructuredConfiguration::parsePropertyValueSequence(const std::string& property_name, const Node& property_value_node, core::ConfigurableComponent& component,
    ParameterContext* parameter_context) {
  const bool is_sensitive = component.getSupportedProperty(property_name)
      | utils::transform([](const auto& prop) -> bool { return prop.isSensitive(); })
      | utils::valueOrElse([]{ return false; });

  for (const auto& nodeVal : property_value_node) {
    if (nodeVal) {
      Node propertiesNode = nodeVal["value"];
      auto rawValueString = propertiesNode.getString().value();
      std::unique_ptr<core::ParameterTokenParser> token_parser;
      if (is_sensitive) {
        rawValueString = utils::crypto::property_encryption::decrypt(rawValueString, sensitive_values_encryptor_);
        token_parser = std::make_unique<core::SensitiveParameterTokenParser>(rawValueString, sensitive_values_encryptor_);
      } else {
        token_parser = std::make_unique<core::NonSensitiveParameterTokenParser>(rawValueString);
      }

      try {
        rawValueString = token_parser->replaceParameters(parameter_context);
      } catch (const ParameterException& e) {
        logger_->log_error("Error while substituting parameters in property '{}': {}", property_name, e.what());
        throw;
      }

      try {
        rawValueString = resolveAssetReferences(rawValueString, asset_manager_);
      } catch (const AssetException& e) {
        logger_->log_error("Error while resolving asset in property '{}': {}", property_name, e.what());
        throw;
      }

      logger_->log_debug("Found property {}", property_name);

      const auto append_prop_result = component.appendProperty(property_name, rawValueString);
      if (append_prop_result) {
        continue;
      }

      if (append_prop_result.error() == make_error_code(PropertyErrorCode::NotSupportedProperty)) {
        logger_->log_warn("Received property {} with value {} but is not one of the properties for {}. Attempting to add as dynamic property.", property_name, rawValueString, component.getName());
        const auto append_dynamic_prop_result = component.appendDynamicProperty(property_name, rawValueString);
        if (!append_dynamic_prop_result) {
          logger_->log_error("Unable to set the dynamic property {} due to {}", property_name, append_dynamic_prop_result.error());
        } else {
          logger_->log_warn("Dynamic property {} has been set", property_name);
        }
      } else {
        logger_->log_error("Failed to set {} property due to {}", property_name, append_prop_result.error());
        raiseComponentError(component.getName(), "", append_prop_result.error().message());
      }
    }
  }
}

std::optional<std::string> StructuredConfiguration::getReplacedParametersValueOrDefault(const std::string_view property_name,
    const bool is_sensitive,
    const std::optional<std::string_view> default_value,
    const Node& property_value_node,
    ParameterContext* parameter_context) {
  try {
    std::unique_ptr<core::ParameterTokenParser> token_parser;
    std::string property_value_string;
    if (is_sensitive) {
      property_value_string = utils::crypto::property_encryption::decrypt(property_value_node.getScalarAsString().value(), sensitive_values_encryptor_);
      token_parser = std::make_unique<core::SensitiveParameterTokenParser>(std::move(property_value_string), sensitive_values_encryptor_);
    } else {
      property_value_string = property_value_node.getScalarAsString().value();
      token_parser = std::make_unique<core::NonSensitiveParameterTokenParser>(std::move(property_value_string));
    }
    auto replaced_property_value_string = token_parser->replaceParameters(parameter_context);
    replaced_property_value_string = resolveAssetReferences(replaced_property_value_string, asset_manager_);
    return replaced_property_value_string;
  } catch (const utils::crypto::EncryptionError& e) {
    logger_->log_error("Fetching property failed with a decryption error: {}", e.what());
    throw;
  } catch (const ParameterException& e) {
    logger_->log_error("Error while substituting parameters in property '{}': {}", property_name, e.what());
    throw;
  } catch (const AssetException& e) {
    logger_->log_error("Error while resolving asset in property '{}': {}", property_name, e.what());
    throw;
  } catch (const std::exception& e) {
    logger_->log_error("Fetching property failed with an exception of {}", e.what());
    logger_->log_error("Invalid conversion for field {}. Value {}", property_name, property_value_node.getDebugString());
  } catch (...) {
    logger_->log_error("Invalid conversion for field {}. Value {}", property_name, property_value_node.getDebugString());
  }
  return default_value | utils::transform([](const std::string_view def_value) { return std::string{def_value}; });
}

void StructuredConfiguration::parseSingleProperty(const std::string& property_name, const Node& property_value_node, core::ConfigurableComponent& component,
    ParameterContext* parameter_context) {
  auto my_prop = component.getSupportedProperty(property_name);
  const bool is_sensitive = my_prop ? my_prop->isSensitive() : false;
  const std::optional<std::string> default_value = my_prop ? my_prop->getDefaultValue() : std::nullopt;

  const auto value_to_set = getReplacedParametersValueOrDefault(property_name, is_sensitive, default_value, property_value_node, parameter_context);
  if (!value_to_set) {
    return;
  }
  if (my_prop) {
    const auto prop_set = component.setProperty(property_name, *value_to_set);
    if (!prop_set) {
      logger_->log_error("Invalid value was set for property '{}' creating component '{}'", property_name, component.getName());
      raiseComponentError(component.getName(), "", prop_set.error().message());
    }
    return;
  }
  logger_->log_warn("Received property {} but is not one of the supported properties for {}. Attempting to add as dynamic property.", property_name, component.getName());

  if (!component.setDynamicProperty(property_name, *value_to_set)) {
    logger_->log_warn("Unable to set the dynamic property {}", property_name);
  } else {
    logger_->log_warn("Dynamic property {} has been set", property_name);
  }
}

void StructuredConfiguration::parsePropertyNodeElement(const std::string& property_name, const Node& property_value_node, core::ConfigurableComponent& component,
    ParameterContext* parameter_context) {
  logger_->log_trace("Encountered {}", property_name);
  if (!property_value_node || property_value_node.isNull()) {
    return;
  }
  if (property_value_node.isSequence()) {
    parsePropertyValueSequence(property_name, property_value_node, component, parameter_context);
  } else {
    parseSingleProperty(property_name, property_value_node, component, parameter_context);
  }
}

void StructuredConfiguration::parsePropertiesNode(const Node& properties_node, core::ConfigurableComponent& component, const std::string& component_name,
    ParameterContext* parameter_context) {
  // Treat generically as a node so we can perform inspection on entries to ensure they are populated
  logger_->log_trace("Entered {}", component_name);
  for (const auto& property_node : properties_node) {
    const auto propertyName = property_node.first.getString().value();
    const Node propertyValueNode = property_node.second;
    const bool is_controller_service_node = dynamic_cast<core::controller::ControllerServiceNode*>(&component) != nullptr;
    const bool is_linked_services = propertyName == "Linked Services";
    // We currently propagate properties to the ControllerServiceNode wrapper and to the actual ControllerService.
    // This could cause false positive warnings because the Node should only handle the linked services while implementation should contain everything else
    // We should probably remove the nodes and handle the linked services concept inside the impls
    // Only parse the property if both are true or both are false (i.e., not mixing controller service node and linked services)
    if ((is_controller_service_node && is_linked_services) || (!is_controller_service_node && !is_linked_services)) {
      parsePropertyNodeElement(propertyName, propertyValueNode, component, parameter_context);
    }
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

    auto funnel = std::make_unique<core::Processor>(name, uuid.value(), std::make_unique<minifi::Funnel>(name, uuid.value()));
    logger_->log_debug("Created funnel with UUID {} and name {}", id, name);
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

    auto port = std::make_unique<Port>(name, uuid.value(), std::make_unique<PortImpl>(name, uuid.value(), port_type));
    logger_->log_debug("Created port UUID {} and name {}", id, name);
    port->setScheduledState(core::RUNNING);
    port->setSchedulingStrategy(core::EVENT_DRIVEN);
    parent->addPort(std::move(port));
  }
}

void StructuredConfiguration::parseParameterContext(const flow::Node& node, core::ProcessGroup& parent) {
  if (!node) {
    return;
  }

  auto parameter_context_name = node.getString().value();
  if (parameter_context_name.empty()) {
    return;
  }

  if (parameter_contexts_.contains(parameter_context_name)) {
    parent.setParameterContext(parameter_contexts_.at(parameter_context_name).get());
  }
}


void StructuredConfiguration::validateComponentProperties(ConfigurableComponent& component, const std::string &component_name, const std::string &section) const {
  const auto& component_properties = component.getSupportedProperties();

  // Validate required properties
  for (const auto& [property_name, property] : component_properties) {
    if (property.getRequired()) {
      if (!property.getValue()) {
        std::string reason = utils::string::join_pack("required property '", property.getName(), "' is not set");
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

  logger_->log_error("{}", err_msg);

  throw std::invalid_argument(err_msg);
}

std::string StructuredConfiguration::getOrGenerateId(const Node& node) {
  if (node[schema_.identifier]) {
    if (auto opt_id_str = node[schema_.identifier].getString()) {
      auto id = opt_id_str.value();
      addNewId(id);
      return id;
    }
    throw std::invalid_argument("getOrGenerateId: idField '" + utils::string::join(",", schema_.identifier) + "' is expected to contain string.");
  }

  auto id = id_generator_->generate().to_string();
  logger_->log_debug("Generating random ID: id => [{}]", id);
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
  auto logInfoMessage = [&]() {
    if (infoMessage.empty()) {
      // Build a helpful info message for the user to inform them that a default is being used
      infoMessage = "Using default value for optional field '" + utils::string::join(",", field_name) + "'";
      if (auto name = node["name"]) {
        infoMessage += "' in component named '" + name.getString().value() + "'";
      }
      infoMessage += " [in '" + node.getPath() + "' section of configuration file]: ";

      infoMessage += default_value;
    }
    logger_->log_info("{}", infoMessage);
  };
  auto result = node[field_name];
  if (!result) {
    logInfoMessage();
    return default_value;
  }

  if (result.isSequence()) {
    if (result.empty()) {
      logInfoMessage();
      return default_value;
    }
    return (*result.begin()).getString().value();
  }
  return result.getString().value();
}

void StructuredConfiguration::addNewId(const std::string& uuid) {
  const auto [_, success] = uuids_.insert(uuid);
  if (!success) {
    throw Exception(ExceptionType::GENERAL_EXCEPTION, "UUID " + uuid + " is duplicated in the flow configuration");
  }
}

std::string StructuredConfiguration::serialize(const core::ProcessGroup& process_group) {
  gsl_Expects(flow_serializer_);
  return flow_serializer_->serialize(process_group, schema_, sensitive_values_encryptor_, {}, parameter_contexts_);
}

}  // namespace org::apache::nifi::minifi::core::flow
