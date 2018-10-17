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
#include <string>
#include <map>
#include <memory>
#include <utility>
#include "core/Core.h"
#include "capi/api.h"
#include "capi/expect.h"
#include "capi/Instance.h"
#include "capi/Plan.h"
#include "ResourceClaim.h"
#include "processors/GetFile.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/StringUtils.h"

using string_map = std::map<std::string, std::string>;

class API_INITIALIZER {
  static int initialized;
};

int API_INITIALIZER::initialized = initialize_api();

int initialize_api() {
  logging::LoggerConfiguration::getConfiguration().disableLogging();
  return 1;
}

void enable_logging() {
  logging::LoggerConfiguration::getConfiguration().enableLogging();
}

class DirectoryConfiguration {
 protected:
  DirectoryConfiguration() {
    minifi::setDefaultDirectory(DEFAULT_CONTENT_DIRECTORY);
  }
 public:
  static void initialize() {
    static DirectoryConfiguration configure;
  }
};

nifi_port *create_port(const char *port) {
  if (nullptr == port)
    return nullptr;
  nifi_port *p = new nifi_port();
  p->port_id = new char[strlen(port) + 1];
  memset(p->port_id, 0x00, strlen(port) + 1);
  strncpy(p->port_id, port, strlen(port));
  return p;
}

int free_port(nifi_port *port) {
  if (port == nullptr)
    return -1;
  delete[] port->port_id;
  delete port;
  return 0;
}

/**
 * Creates a NiFi Instance from the url and output port.
 * @param url http URL for NiFi instance
 * @param port Remote output port.
 */
nifi_instance *create_instance(const char *url, nifi_port *port) {
  // make sure that we have a thread safe way of initializing the content directory
  DirectoryConfiguration::initialize();

  nifi_instance *instance = new nifi_instance;

  instance->instance_ptr = new minifi::Instance(url, port->port_id);
  instance->port.port_id = port->port_id;

  return instance;
}

/**
 * Initializes the instance
 */
void initialize_instance(nifi_instance *instance) {
  auto minifi_instance_ref = static_cast<minifi::Instance*>(instance->instance_ptr);
  minifi_instance_ref->setRemotePort(instance->port.port_id);
}
/*
 typedef int c2_update_callback(char *);

 typedef int c2_stop_callback(char *);

 typedef int c2_start_callback(char *);

 */
void enable_async_c2(nifi_instance *instance, C2_Server *server, c2_stop_callback *c1, c2_start_callback *c2, c2_update_callback *c3) {
  auto minifi_instance_ref = static_cast<minifi::Instance*>(instance->instance_ptr);
  minifi_instance_ref->enableAsyncC2(server, c1, c2, c3);
}

/**
 * Sets a property within the nifi instance
 * @param instance nifi instance
 * @param key key in which we will set the valiue
 * @param value
 * @return -1 when instance or key are null
 */
int set_instance_property(nifi_instance *instance, const char *key, const char *value) {
  if (nullptr == instance || nullptr == instance->instance_ptr || nullptr == key) {
    return -1;
  }
  auto minifi_instance_ref = static_cast<minifi::Instance*>(instance->instance_ptr);
  minifi_instance_ref->getConfiguration()->set(key, value);
  return 0;
}

/**
 * Reclaims memory associated with a nifi instance structure.
 * @param instance nifi instance.
 */
void free_instance(nifi_instance* instance) {
  if (instance != nullptr) {
    delete ((minifi::Instance*) instance->instance_ptr);
    delete instance;
  }
}

/**
 * Creates a flow file record
 * @param file file to place into the flow file.
 */
flow_file_record* create_flowfile(const char *file, const size_t len) {
  flow_file_record *new_ff = new flow_file_record;
  new_ff->attributes = new string_map();
  new_ff->contentLocation = new char[len + 1];
  snprintf(new_ff->contentLocation, len + 1, "%s", file);
  std::ifstream in(file, std::ifstream::ate | std::ifstream::binary);
  // set the size of the flow file.
  new_ff->size = in.tellg();
  return new_ff;
}

/**
 * Creates a flow file record
 * @param file file to place into the flow file.
 */
flow_file_record* create_ff_object(const char *file, const size_t len, const uint64_t size) {
  if (nullptr == file) {
    return nullptr;
  }
  flow_file_record *new_ff = new flow_file_record;
  new_ff->attributes = new string_map();
  new_ff->ffp = 0;
  new_ff->contentLocation = new char[len + 1];
  snprintf(new_ff->contentLocation, len + 1, "%s", file);
  std::ifstream in(file, std::ifstream::ate | std::ifstream::binary);
  // set the size of the flow file.
  new_ff->size = size;
  return new_ff;
}

flow_file_record* create_ff_object_na(const char *file, const size_t len, const uint64_t size) {
  flow_file_record *new_ff = new flow_file_record;
  new_ff->attributes = nullptr;
  new_ff->contentLocation = new char[len + 1];
  snprintf(new_ff->contentLocation, len + 1, "%s", file);
  std::ifstream in(file, std::ifstream::ate | std::ifstream::binary);
  // set the size of the flow file.
  new_ff->size = size;
  return new_ff;
}
/**
 * Reclaims memory associated with a flow file object
 * @param ff flow file record.
 */
void free_flowfile(flow_file_record *ff) {
  if (ff != nullptr) {
    if (ff->in != nullptr) {
      auto instance = static_cast<nifi_instance*>(ff->in);
      auto minifi_instance_ref = static_cast<minifi::Instance*>(instance->instance_ptr);
      auto content_repo = minifi_instance_ref->getContentRepository();
      std::shared_ptr<minifi::ResourceClaim> claim = std::make_shared<minifi::ResourceClaim>(ff->contentLocation, content_repo);
      content_repo->remove(claim);
    }
    if (ff->ffp == nullptr) {
      auto map = static_cast<string_map*>(ff->attributes);
      delete map;
    }
    delete[] ff->contentLocation;
    delete ff;
  }
}

/**
 * Adds an attribute
 * @param ff flow file record
 * @param key key
 * @param value value to add
 * @param size size of value
 * @return 0 or -1 based on whether the attributed existed previously (-1) or not (0)
 */
uint8_t add_attribute(flow_file_record *ff, const char *key, void *value, size_t size) {
  auto attribute_map = static_cast<string_map*>(ff->attributes);
  const auto& ret = attribute_map->insert(std::pair<std::string, std::string>(key, std::string(static_cast<char*>(value), size)));
  return ret.second ? 0 : -1;
}

/**
 * Updates (or adds) an attribute
 * @param ff flow file record
 * @param key key
 * @param value value to add
 * @param size size of value
 */
void update_attribute(flow_file_record *ff, const char *key, void *value, size_t size) {
  auto attribute_map = static_cast<string_map*>(ff->attributes);
  (*attribute_map)[key] = std::string(static_cast<char*>(value), size);
}

/*
 * Obtains the attribute.
 * @param ff flow file record
 * @param key key
 * @param caller_attribute caller supplied object in which we will copy the data ptr
 * @return 0 if successful, -1 if the key does not exist
 */
uint8_t get_attribute(flow_file_record *ff, attribute *caller_attribute) {
  if (ff == nullptr) {
    return -1;
  }
  auto attribute_map = static_cast<string_map*>(ff->attributes);
  if (!attribute_map) {
    return -1;
  }
  auto find = attribute_map->find(caller_attribute->key);
  if (find != attribute_map->end()) {
    caller_attribute->value = static_cast<void*>(const_cast<char*>(find->second.data()));
    caller_attribute->value_size = find->second.size();
    return 0;
  }
  return -1;
}

int get_attribute_qty(const flow_file_record* ff) {
  if (ff == nullptr) {
    return 0;
  }
  auto attribute_map = static_cast<string_map*>(ff->attributes);
  return attribute_map ? attribute_map->size() : 0;
}

int get_all_attributes(const flow_file_record* ff, attribute_set *target) {
  if (ff == nullptr) {
    return 0;
  }
  auto attribute_map = static_cast<string_map*>(ff->attributes);
  if (!attribute_map || attribute_map->empty()) {
    return 0;
  }
  int i = 0;
  for (const auto& kv : *attribute_map) {
    if (i >= target->size) {
      break;
    }
    target->attributes[i].key = kv.first.data();
    target->attributes[i].value = static_cast<void*>(const_cast<char*>(kv.second.data()));
    target->attributes[i].value_size = kv.second.size();
    ++i;
  }
  return i;
}

/**
 * Removes a key from the attribute chain
 * @param ff flow file record
 * @param key key to remove
 * @return 0 if removed, -1 otherwise
 */
uint8_t remove_attribute(flow_file_record *ff, const char *key) {
  auto attribute_map = static_cast<string_map*>(ff->attributes);
  return attribute_map->erase(key) - 1;  // erase by key returns the number of elements removed (0 or 1)
}

/**
 * Transmits the flowfile
 * @param ff flow file record
 * @param instance nifi instance structure
 */
int transmit_flowfile(flow_file_record *ff, nifi_instance *instance) {
  auto minifi_instance_ref = static_cast<minifi::Instance*>(instance->instance_ptr);
  // in the unlikely event the user forgot to initialize the instance, we shall do it for them.
  if (UNLIKELY(minifi_instance_ref->isRPGConfigured() == false)) {
    minifi_instance_ref->setRemotePort(instance->port.port_id);
  }

  auto attribute_map = static_cast<string_map*>(ff->attributes);

  auto no_op = minifi_instance_ref->getNoOpRepository();

  auto content_repo = minifi_instance_ref->getContentRepository();

  std::shared_ptr<minifi::ResourceClaim> claim = std::make_shared<minifi::ResourceClaim>(ff->contentLocation, content_repo);
  claim->increaseFlowFileRecordOwnedCount();
  claim->increaseFlowFileRecordOwnedCount();

  auto ffr = std::make_shared<minifi::FlowFileRecord>(no_op, content_repo, *attribute_map, claim);
  ffr->addAttribute("nanofi.version", API_VERSION);
  ffr->setSize(ff->size);

  std::string port_uuid = instance->port.port_id;

  minifi_instance_ref->transfer(ffr);

  return 0;
}

flow *create_new_flow(nifi_instance *instance) {
  auto minifi_instance_ref = static_cast<minifi::Instance*>(instance->instance_ptr);
  flow *new_flow = new flow;

  auto execution_plan = new ExecutionPlan(minifi_instance_ref->getContentRepository(), minifi_instance_ref->getNoOpRepository(), minifi_instance_ref->getNoOpRepository());

  new_flow->plan = execution_plan;

  return new_flow;
}

flow *create_flow(nifi_instance *instance, const char *first_processor) {
  if (nullptr == instance || nullptr == instance->instance_ptr) {
    return nullptr;
  }
  auto minifi_instance_ref = static_cast<minifi::Instance*>(instance->instance_ptr);
  flow *new_flow = new flow;

  auto execution_plan = new ExecutionPlan(minifi_instance_ref->getContentRepository(), minifi_instance_ref->getNoOpRepository(), minifi_instance_ref->getNoOpRepository());

  new_flow->plan = execution_plan;

  if (first_processor != nullptr && strlen(first_processor) > 0) {
    // automatically adds it with success
    execution_plan->addProcessor(first_processor, first_processor);
  }
  return new_flow;
}

processor *add_python_processor(flow *flow, void (*ontrigger_callback)(processor_session *)) {
  if (nullptr == flow || nullptr == flow->plan || nullptr == ontrigger_callback) {
    return nullptr;
  }
  ExecutionPlan *plan = static_cast<ExecutionPlan*>(flow->plan);
  auto proc = plan->addCallback(nullptr, ontrigger_callback);
  processor *new_processor = new processor();
  new_processor->processor_ptr = proc.get();
  return new_processor;
}

flow *create_getfile(nifi_instance *instance, flow *parent_flow, GetFileConfig *c) {
  static const std::string first_processor = "GetFile";
  flow *new_flow = parent_flow == nullptr ? create_flow(instance, nullptr) : parent_flow;

  ExecutionPlan *plan = static_cast<ExecutionPlan*>(new_flow->plan);
  // automatically adds it with success
  auto getFile = plan->addProcessor(first_processor, first_processor);

  plan->setProperty(getFile, processors::GetFile::Directory.getName(), c->directory);
  plan->setProperty(getFile, processors::GetFile::KeepSourceFile.getName(), c->keep_source ? "true" : "false");
  plan->setProperty(getFile, processors::GetFile::Recurse.getName(), c->recurse ? "true" : "false");

  return new_flow;
}

processor *add_processor(flow *flow, const char *processor_name) {
  if (nullptr == flow || nullptr == processor_name) {
    return nullptr;
  }
  ExecutionPlan *plan = static_cast<ExecutionPlan*>(flow->plan);
  auto proc = plan->addProcessor(processor_name, processor_name);
  if (proc) {
    processor *new_processor = new processor();
    new_processor->processor_ptr = proc.get();
    return new_processor;
  }
  return nullptr;
}

processor *add_processor_with_linkage(flow *flow, const char *processor_name) {
  ExecutionPlan *plan = static_cast<ExecutionPlan*>(flow->plan);
  auto proc = plan->addProcessor(processor_name, processor_name, core::Relationship("success", "description"), true);
  if (proc) {
    processor *new_processor = new processor();
    new_processor->processor_ptr = proc.get();
    return new_processor;
  }
  return nullptr;
}

int set_property(processor *proc, const char *name, const char *value) {
  if (name != nullptr && value != nullptr && proc != nullptr) {
    core::Processor *p = static_cast<core::Processor*>(proc->processor_ptr);
    return p->setProperty(name, value) ? 0 : -2;
  }
  return -1;
}

int free_flow(flow *flow) {
  if (flow == nullptr || nullptr == flow->plan)
    return -1;
  auto execution_plan = static_cast<ExecutionPlan*>(flow->plan);
  delete execution_plan;
  delete flow;
  return 0;
}

flow_file_record *get_next_flow_file(nifi_instance *instance, flow *flow) {
  if (instance == nullptr || nullptr == flow || nullptr == flow->plan)
    return nullptr;
  auto execution_plan = static_cast<ExecutionPlan*>(flow->plan);
  execution_plan->reset();
  while (execution_plan->runNextProcessor()) {
  }
  auto ff = execution_plan->getCurrentFlowFile();
  if (ff == nullptr) {
    return nullptr;
  }
  auto claim = ff->getResourceClaim();

  if (claim != nullptr) {
    // create a flow file.
    claim->increaseFlowFileRecordOwnedCount();
    auto path = claim->getContentFullPath();
    auto ffr = create_ff_object_na(path.c_str(), path.length(), ff->getSize());
    ffr->ffp = ff.get();
    ffr->attributes = ff->getAttributesPtr();
    ffr->in = instance;
    return ffr;
  } else {
    return nullptr;
  }
}

size_t get_flow_files(nifi_instance *instance, flow *flow, flow_file_record **ff_r, size_t size) {
  if (nullptr == instance || nullptr == flow || nullptr == ff_r)
    return 0;
  auto execution_plan = static_cast<ExecutionPlan*>(flow->plan);
  size_t i = 0;
  for (; i < size; i++) {
    execution_plan->reset();
    auto ffr = get_next_flow_file(instance, flow);
    if (ffr == nullptr) {
      break;
    }
    ff_r[i] = ffr;
  }
  return i;
}

flow_file_record *get(nifi_instance *instance, flow *flow, processor_session *session) {
  if (nullptr == instance || nullptr == flow || nullptr == session)
    return nullptr;
  auto sesh = static_cast<core::ProcessSession*>(session->session);
  auto execution_plan = static_cast<ExecutionPlan*>(flow->plan);
  auto ff = sesh->get();
  execution_plan->setNextFlowFile(ff);
  if (ff == nullptr) {
    return nullptr;
  }
  auto claim = ff->getResourceClaim();

  if (claim != nullptr) {
    // create a flow file.
    claim->increaseFlowFileRecordOwnedCount();
    auto path = claim->getContentFullPath();
    auto ffr = create_ff_object_na(path.c_str(), path.length(), ff->getSize());
    ffr->attributes = ff->getAttributesPtr();
    ffr->ffp = ff.get();
    ffr->in = instance;
    return ffr;
  } else {
    return nullptr;
  }
}

int transfer(processor_session* session, flow *flow, const char *rel) {
  if (nullptr == session || nullptr == flow || rel == nullptr) {
    return -1;
  }
  auto sesh = static_cast<core::ProcessSession*>(session->session);
  auto execution_plan = static_cast<ExecutionPlan*>(flow->plan);
  if (nullptr == sesh || nullptr == execution_plan) {
    return -1;
  }
  core::Relationship relationship(rel, rel);
  auto ff = execution_plan->getNextFlowFile();
  if (nullptr == ff) {
    return -2;
  }
  sesh->transfer(ff, relationship);
  return 0;
}
