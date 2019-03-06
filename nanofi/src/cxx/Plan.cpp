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

#include "cxx/Plan.h"
#include "cxx/CallbackProcessor.h"
#include <memory>
#include <vector>
#include <set>
#include <string>

std::shared_ptr<utils::IdGenerator> ExecutionPlan::id_generator_ = utils::IdGenerator::getIdGenerator();
std::unordered_map<std::string, std::shared_ptr<ExecutionPlan>> ExecutionPlan::proc_plan_map_ = {};
std::map<std::string, processor_logic*> ExecutionPlan::custom_processors = {};

ExecutionPlan::ExecutionPlan(std::shared_ptr<core::ContentRepository> content_repo, std::shared_ptr<core::Repository> flow_repo, std::shared_ptr<core::Repository> prov_repo)
    : content_repo_(content_repo),
      flow_repo_(flow_repo),
      prov_repo_(prov_repo),
      finalized(false),
      location(-1),
      current_flowfile_(nullptr),
      logger_(logging::LoggerFactory<ExecutionPlan>::getLogger()) {
  stream_factory = org::apache::nifi::minifi::io::StreamFactory::getInstance(std::make_shared<minifi::Configure>());
}

/**
 * Add a callback to obtain and pass processor session to a generated processor
 *
 */
std::shared_ptr<core::Processor> ExecutionPlan::addSimpleCallback(void *obj, std::function<void(core::ProcessSession*)> fp) {
  if (finalized) {
    return nullptr;
  }

  auto simple_func_wrapper = [fp](core::ProcessSession *session, core::ProcessContext *context)->void { fp(session); };

  return addCallback(obj, simple_func_wrapper);
}

std::shared_ptr<core::Processor> ExecutionPlan::addCallback(void *obj, std::function<void(core::ProcessSession*, core::ProcessContext *)> fp) {
  if (finalized) {
    return nullptr;
  }

  auto proc = createCallback(obj, fp);

  if (!proc)
    return nullptr;

  return addProcessor(proc, CallbackProcessorName, core::Relationship("success", "description"), true);
}

bool ExecutionPlan::setProperty(const std::shared_ptr<core::Processor> proc, const std::string &prop, const std::string &value) {
  uint32_t i = 0;
  logger_->log_debug("Attempting to set property %s %s for %s", prop, value, proc->getName());
  for (i = 0; i < processor_queue_.size(); i++) {
    if (processor_queue_.at(i) == proc) {
      break;
    }
  }

  if (i >= processor_queue_.size() || i >= processor_contexts_.size()) {
    return false;
  }

  return processor_contexts_.at(i)->setProperty(prop, value);
}

std::shared_ptr<core::Processor> ExecutionPlan::addProcessor(const std::shared_ptr<core::Processor> &processor, const std::string &name, core::Relationship relationship, bool linkToPrevious) {
  if (finalized) {
    return nullptr;
  }

  utils::Identifier uuid;
  id_generator_->generate(uuid);

  processor->setStreamFactory(stream_factory);
  // initialize the processor
  processor->initialize();

  processor_mapping_[processor->getUUIDStr()] = processor;

  if (!linkToPrevious) {
    termination_ = relationship;
  } else {
    std::shared_ptr<core::Processor> last = processor_queue_.back();

    if (last == nullptr) {
      last = processor;
      termination_ = relationship;
    }

    relationships_.push_back(connectProcessors(last, processor, relationship, true));
  }

  std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(processor);

  processor_nodes_.push_back(node);

  std::shared_ptr<core::ProcessContext> context = std::make_shared<core::ProcessContext>(node, controller_services_provider_, prov_repo_, flow_repo_, content_repo_);
  processor_contexts_.push_back(context);

  processor_queue_.push_back(processor);

  return processor;
}

std::shared_ptr<core::Processor> ExecutionPlan::addProcessor(const std::string &processor_name, const std::string &name, core::Relationship relationship, bool linkToPrevious) {
  if (finalized) {
    return nullptr;
  }
  auto processor = ExecutionPlan::createProcessor(processor_name, name);
  if (!processor) {
    return nullptr;
  }
  return addProcessor(processor, name, relationship, linkToPrevious);
}

void ExecutionPlan::reset() {
  process_sessions_.clear();
  factories_.clear();
  location = -1;
  for (auto proc : processor_queue_) {
    while (proc->getActiveTasks() > 0) {
      proc->decrementActiveTask();
    }
  }
}

bool ExecutionPlan::runNextProcessor(std::function<void(const std::shared_ptr<core::ProcessContext>, const std::shared_ptr<core::ProcessSession>)> verify,
                                     std::shared_ptr<flowfile_input_params> input_ff_params) {
  if (!finalized) {
    finalize();
  }
  location++;
  if (location >= processor_queue_.size()) {
    return false;
  }

  std::shared_ptr<core::Processor> processor = processor_queue_[location];
  std::shared_ptr<core::ProcessContext> context = processor_contexts_[location];
  std::shared_ptr<core::ProcessSessionFactory> factory = std::make_shared<core::ProcessSessionFactory>(context);
  factories_.push_back(factory);
  if (std::find(configured_processors_.begin(), configured_processors_.end(), processor) == configured_processors_.end()) {
    processor->onSchedule(context, factory);
    configured_processors_.push_back(processor);
  }
  std::shared_ptr<core::ProcessSession> current_session = std::make_shared<core::ProcessSession>(context);
  process_sessions_.push_back(current_session);
  if (input_ff_params) {
    std::shared_ptr<minifi::FlowFileRecord> flowFile = std::static_pointer_cast<minifi::FlowFileRecord>(current_session->create());
    for(const auto& kv : input_ff_params->attributes) {
      flowFile->setAttribute(kv.first, kv.second);
    }
    current_session->importFrom(*(input_ff_params->content_stream.get()), flowFile);
    current_session->transfer(flowFile, core::Relationship("success", "success"));
    relationships_[relationships_.size()-1]->put(std::static_pointer_cast<core::FlowFile>(flowFile));
  }
  processor->incrementActiveTasks();
  processor->setScheduledState(core::ScheduledState::RUNNING);
  if (verify != nullptr) {
    verify(context, current_session);
  } else {
    logger_->log_debug("Running %s", processor->getName());
    processor->onTrigger(context, current_session);
  }
  current_session->commit();
  current_flowfile_ = current_session->get();
  auto hasMore = location + 1 < processor_queue_.size();
  if (!hasMore && !current_flowfile_) {
    std::set<std::shared_ptr<core::FlowFile>> expired;
    current_flowfile_ = relationships_.back()->poll(expired);
  }
  return hasMore;
}

std::set<std::shared_ptr<provenance::ProvenanceEventRecord>> ExecutionPlan::getProvenanceRecords() {
  return process_sessions_.at(location)->getProvenanceReporter()->getEvents();
}

std::shared_ptr<core::FlowFile> ExecutionPlan::getCurrentFlowFile() {
  return current_flowfile_;
}

std::shared_ptr<core::ProcessSession> ExecutionPlan::getCurrentSession() {
  return current_session_;
}

std::shared_ptr<minifi::Connection> ExecutionPlan::buildFinalConnection(std::shared_ptr<core::Processor> processor, bool set_dst) {
  return connectProcessors(processor, processor, termination_, set_dst);
}

void ExecutionPlan::finalize() {
  if (failure_handler_) {
    auto failure_proc = createProcessor(CallbackProcessorName, CallbackProcessorName);

    std::shared_ptr<processors::CallbackProcessor> callback_proc = std::static_pointer_cast<processors::CallbackProcessor>(failure_proc);
    callback_proc->setCallback(nullptr, std::bind(&FailureHandler::operator(), failure_handler_, std::placeholders::_1));

    for (const auto& proc : processor_queue_) {
      for (const auto& rel : proc->getSupportedRelationships()) {
        if (rel.getName() == "failure") {
          relationships_.push_back(connectProcessors(proc, failure_proc, core::Relationship("failure", "failure collector"), true));
          break;
        }
      }
    }

    std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(failure_proc);

    processor_nodes_.push_back(node);

    std::shared_ptr<core::ProcessContext> context = std::make_shared<core::ProcessContext>(node, controller_services_provider_, prov_repo_, flow_repo_, content_repo_);
    processor_contexts_.push_back(context);

    processor_queue_.push_back(failure_proc);
  }

  if (relationships_.size() > 0) {
    relationships_.push_back(buildFinalConnection(processor_queue_.back()));
  } else {
    for (auto processor : processor_queue_) {
      relationships_.push_back(buildFinalConnection(processor, true));
    }
  }

  finalized = true;
}

std::shared_ptr<core::Processor> ExecutionPlan::createProcessor(const std::string &processor_name, const std::string &name) {
  utils::Identifier uuid;
  id_generator_->generate(uuid);

  auto custom_proc = custom_processors.find(processor_name);

  if(custom_proc != custom_processors.end()) {
    auto c_func = custom_proc->second;
    auto wrapper_func = [c_func](core::ProcessSession * session, core::ProcessContext * context) {
      return c_func(reinterpret_cast<processor_session*>(session), reinterpret_cast<processor_context*>(context));
    };
    return createCallback(nullptr, wrapper_func);
  }


  auto ptr = core::ClassLoader::getDefaultClassLoader().instantiate(processor_name, uuid);
  if (nullptr == ptr) {
    return nullptr;
  }
  std::shared_ptr<core::Processor> processor = std::static_pointer_cast<core::Processor>(ptr);

  processor->setName(name);
  return processor;
}

std::shared_ptr<core::Processor> ExecutionPlan::createCallback(void *obj, std::function<void(core::ProcessSession*, core::ProcessContext *)> fp) {
  auto ptr = createProcessor(CallbackProcessorName, CallbackProcessorName);
  if (!ptr)
    return nullptr;

  std::shared_ptr<processors::CallbackProcessor> processor = std::static_pointer_cast<processors::CallbackProcessor>(ptr);
  processor->setCallback(obj, fp);

  return ptr;
}

std::shared_ptr<minifi::Connection> ExecutionPlan::connectProcessors(std::shared_ptr<core::Processor> src_proc, std::shared_ptr<core::Processor> dst_proc, core::Relationship relationship,
                                                                     bool set_dst) {
  std::stringstream connection_name;
  connection_name << src_proc->getUUIDStr() << "-to-" << dst_proc->getUUIDStr();
  std::shared_ptr<minifi::Connection> connection = std::make_shared<minifi::Connection>(flow_repo_, content_repo_, connection_name.str());
  connection->addRelationship(relationship);

  // link the connections so that we can test results at the end for this
  connection->setSource(src_proc);

  utils::Identifier uuid_copy, uuid_copy_next;
  src_proc->getUUID(uuid_copy);
  connection->setSourceUUID(uuid_copy);
  if (set_dst) {
    connection->setDestination(dst_proc);
    dst_proc->getUUID(uuid_copy_next);
    connection->setDestinationUUID(uuid_copy_next);
    if (src_proc != dst_proc) {
      dst_proc->addConnection(connection);
    }
  }
  src_proc->addConnection(connection);

  return connection;
}

bool ExecutionPlan::setFailureCallback(std::function<void(flow_file_record*)> onerror_callback) {
  if (finalized && !failure_handler_) {
    return false;  // Already finalized the flow without failure handler processor
  }
  if (!failure_handler_) {
    failure_handler_ = std::make_shared<FailureHandler>(getContentRepo());
  }
  failure_handler_->setCallback(onerror_callback);
  return true;
}

bool ExecutionPlan::setFailureStrategy(FailureStrategy start) {
  if (!failure_handler_) {
    return false;
  }
  failure_handler_->setStrategy(start);
  return true;
}

bool ExecutionPlan::addCustomProcessor(const char * name, processor_logic* logic) {
  if(CallbackProcessorName == name) {
    return false;  // This name cannot be registered
  }

  if (custom_processors.count(name) > 0 ) {
    return false;  // Already exists
  }
  custom_processors[name] = logic;
  return true;
}

int ExecutionPlan::deleteCustomProcessor(const char * name) {
  return custom_processors.erase(name);
}

