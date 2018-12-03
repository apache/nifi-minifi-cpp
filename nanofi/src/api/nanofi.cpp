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
#include <exception>
#include <stdio.h>

#include "api/nanofi.h"
#include "core/Core.h"
#include "core/expect.h"
#include "cxx/Instance.h"
#include "cxx/Plan.h"
#include "cxx/CallbackProcessor.h"
#include "ResourceClaim.h"
#include "processors/GetFile.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/StringUtils.h"
#include "io/DataStream.h"
#include "core/cxxstructs.h"

using string_map = std::map<std::string, std::string>;

class API_INITIALIZER {
 public:
  static int initialized;
};

int API_INITIALIZER::initialized = initialize_api();

static nifi_instance* standalone_instance = nullptr;

int initialize_api() {
  logging::LoggerConfiguration::getConfiguration().disableLogging();
  return 1;
}

void enable_logging() {
  logging::LoggerConfiguration::getConfiguration().enableLogging();
}

void set_terminate_callback(void (*terminate_callback)()) {
  std::set_terminate(terminate_callback);
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

/**
 * Creates a NiFi Instance from the url and output port.
 * @param url http URL for NiFi instance
 * @param port Remote output port.
 * @Deprecated for API version 0.2 in favor of the following prototype
 * nifi_instance *create_instance(nifi_port const *port) {
 */
nifi_instance *create_instance(const char *url, nifi_port *port) {
  // make sure that we have a thread safe way of initializing the content directory
  DirectoryConfiguration::initialize();

  // need reinterpret cast until we move to C for this module.
  nifi_instance *instance = reinterpret_cast<nifi_instance*>(malloc(sizeof(nifi_instance)));
  /**
   * This API will gradually move away from C++, hence malloc is used for nifi_instance
   * Since minifi::Instance is currently being used, then we need to use new in that case.
   */
  instance->instance_ptr = new minifi::Instance(url, port->port_id);
  // may have to translate port ID here in the future
  // need reinterpret cast until we move to C for this module.
  instance->port.port_id = reinterpret_cast<char*>(malloc(strlen(port->port_id) + 1));
  snprintf(instance->port.port_id, strlen(port->port_id) + 1, "%s", port->port_id);
  return instance;
}

standalone_processor *create_processor(const char *name) {
  static int proc_counter = 0;
  auto ptr = ExecutionPlan::createProcessor(name, name);
  if (!ptr) {
    return nullptr;
  }
  if (standalone_instance == nullptr) {
    nifi_port port;
    char portnum[] = "98765";
    port.port_id = portnum;
    standalone_instance = create_instance("internal_standalone", &port);
  }
  std::string flow_name = std::to_string(proc_counter++);
  auto flow = create_flow(standalone_instance, flow_name.c_str());
  std::shared_ptr<ExecutionPlan> plan(flow);
  plan->addProcessor(ptr, name);
  ExecutionPlan::addProcessorWithPlan(ptr->getUUIDStr(), plan);
  return static_cast<standalone_processor*>(ptr.get());
}

void free_standalone_processor(standalone_processor* proc) {
  if (proc == nullptr) {
    return;
  }
  ExecutionPlan::removeProcWithPlan(proc->getUUIDStr());

  if (ExecutionPlan::getProcWithPlanQty() == 0) {
    // The instance is not needed any more as there are no standalone processors in the system
    free_instance(standalone_instance);
    standalone_instance = nullptr;
  }
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
    free(instance->port.port_id);
    free(instance);
  }
}

/**
 * Creates a flow file record
 * @param file file to place into the flow file.
 */
flow_file_record* create_flowfile(const char *file, const size_t len) {
  flow_file_record *new_ff = (flow_file_record*) malloc(sizeof(flow_file_record));
  new_ff->attributes = new string_map();
  new_ff->contentLocation = (char*) malloc(sizeof(char) * (len + 1));
  snprintf(new_ff->contentLocation, len + 1, "%s", file);
  std::ifstream in(file, std::ifstream::ate | std::ifstream::binary);
  // set the size of the flow file.
  new_ff->size = in.tellg();
  new_ff->keepContent = 0;
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
  flow_file_record *new_ff = create_ff_object_na(file, len, size);
  new_ff->attributes = new string_map();
  new_ff->ffp = 0;
  return new_ff;
}

flow_file_record* create_ff_object_na(const char *file, const size_t len, const uint64_t size) {
  flow_file_record *new_ff = (flow_file_record*) malloc(sizeof(flow_file_record));
  new_ff->attributes = nullptr;
  new_ff->contentLocation = (char*) malloc(sizeof(char) * (len + 1));
  snprintf(new_ff->contentLocation, len + 1, "%s", file);
  // set the size of the flow file.
  new_ff->size = size;
  new_ff->crp = static_cast<void*>(new std::shared_ptr<minifi::core::ContentRepository>);
  new_ff->keepContent = 0;
  return new_ff;
}
/**
 * Reclaims memory associated with a flow file object
 * @param ff flow file record.
 */
void free_flowfile(flow_file_record *ff) {
  if (ff == nullptr) {
    return;
  }
  auto content_repo_ptr = static_cast<std::shared_ptr<minifi::core::ContentRepository>*>(ff->crp);
  if (content_repo_ptr->get() && (ff->keepContent == 0)) {
    std::shared_ptr<minifi::ResourceClaim> claim = std::make_shared<minifi::ResourceClaim>(ff->contentLocation, *content_repo_ptr);
    (*content_repo_ptr)->remove(claim);
  }
  if (ff->ffp == nullptr) {
    auto map = static_cast<string_map*>(ff->attributes);
    delete map;
  } else {
    auto ff_sptr = reinterpret_cast<std::shared_ptr<core::FlowFile>*>(ff->ffp);
    delete ff_sptr;
  }
  free(ff->contentLocation);
  free(ff);
  delete content_repo_ptr;
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
uint8_t get_attribute(const flow_file_record * ff, attribute * caller_attribute) {
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

int get_content(const flow_file_record* ff, uint8_t* target, int size) {
  if (ff == nullptr || target == nullptr || size == 0) {
    return 0;
  }
  auto content_repo = static_cast<std::shared_ptr<minifi::core::ContentRepository>*>(ff->crp);
  std::shared_ptr<minifi::ResourceClaim> claim = std::make_shared<minifi::ResourceClaim>(ff->contentLocation, *content_repo);
  auto stream = (*content_repo)->read(claim);
  return stream->read(target, size);
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

flow * create_new_flow(nifi_instance * instance) {
  return create_flow(instance, "");
}

flow *create_flow(nifi_instance *instance, const char *first_processor) {
  if (nullptr == instance || nullptr == instance->instance_ptr) {
    return nullptr;
  }
  auto minifi_instance_ref = static_cast<minifi::Instance*>(instance->instance_ptr);
  flow * area = static_cast<flow*>(malloc(1*sizeof(flow)));
  if(area == nullptr) {
    return nullptr;
  }
  flow *new_flow = new(area) flow(minifi_instance_ref->getContentRepository(), minifi_instance_ref->getNoOpRepository(), minifi_instance_ref->getNoOpRepository());

  if (first_processor != nullptr && strlen(first_processor) > 0) {
    // automatically adds it with success
    new_flow->addProcessor(first_processor, first_processor);
  }
  return new_flow;
}

processor *add_python_processor(flow *flow, void (*ontrigger_callback)(processor_session *)) {
  if (nullptr == flow || nullptr == ontrigger_callback) {
    return nullptr;
  }
  auto lambda = [ontrigger_callback](core::ProcessSession *ps) {
    ontrigger_callback(static_cast<processor_session*>(ps));  //Meh, sorry for this
  };
  auto proc = flow->addSimpleCallback(nullptr, lambda);
  return static_cast<processor*>(proc.get());
}

flow * create_getfile(nifi_instance * instance, flow * parent_flow, GetFileConfig * c) {
  static const std::string first_processor = "GetFile";
  flow *new_flow = parent_flow == nullptr ? create_flow(instance, nullptr) : parent_flow;

  // automatically adds it with success
  auto getFile = new_flow->addProcessor(first_processor, first_processor);

  new_flow->setProperty(getFile, processors::GetFile::Directory.getName(), c->directory);
  new_flow->setProperty(getFile, processors::GetFile::KeepSourceFile.getName(), c->keep_source ? "true" : "false");
  new_flow->setProperty(getFile, processors::GetFile::Recurse.getName(), c->recurse ? "true" : "false");

  return new_flow;
}

processor *add_processor(flow *flow, const char *processor_name) {
  if (nullptr == flow || nullptr == processor_name) {
    return nullptr;
  }

  auto proc = flow->addProcessor(processor_name, processor_name, core::Relationship("success", "description"), flow->hasProcessor());
  return static_cast<processor*>(proc.get());
}

int add_failure_callback(flow *flow, void (*onerror_callback)(flow_file_record*)) {
  return flow->setFailureCallback(onerror_callback) ? 0 : 1;
}

int set_failure_strategy(flow *flow, FailureStrategy strategy) {
  return flow->setFailureStrategy(strategy) ? 0 : -1;
}

int set_propery_internal(core::Processor* proc, const char *name, const char *value) {
  if (name != nullptr && value != nullptr) {
    bool success = proc->setProperty(name, value) || (proc->supportsDynamicProperties() && proc->setDynamicProperty(name, value));
    return success ? 0 : -2;
  }
  return -1;
}

int set_property(processor *proc, const char *name, const char *value) {
  if (proc != nullptr) {
    return set_propery_internal(proc, name, value);
  }
  return -1;
}

int set_standalone_property(standalone_processor *proc, const char *name, const char *value) {
  if (proc != nullptr) {
    return set_propery_internal(proc, name, value);
  }
  return -1;
}

char * get_property(const processor_context *  context, const char * name) {
  std::string value;
  if(!context->getDynamicProperty(name, value)) {
    return nullptr;
  }
  size_t len = value.length();
  char * ret_val = (char*)malloc((len +1) * sizeof(char));
  strncpy(ret_val, value.data(), len);
  ret_val[len] = '\0';
  return ret_val;
}

int free_flow(flow *flow) {
  if (flow == nullptr)
    return -1;
  flow->~flow();
  free(flow);
  return 0;
}

flow_file_record* flowfile_to_record(std::shared_ptr<core::FlowFile> ff, const std::shared_ptr<minifi::core::ContentRepository>& crp) {
  auto claim = ff->getResourceClaim();
  if(claim == nullptr) {
    return nullptr;
  }

  // create a flow file.
  claim->increaseFlowFileRecordOwnedCount();
  auto path = claim->getContentFullPath();
  auto ffr = create_ff_object_na(path.c_str(), path.length(), ff->getSize());
  ffr->ffp = static_cast<void*>(new std::shared_ptr<core::FlowFile>(ff));
  ffr->attributes = ff->getAttributesPtr();
  auto content_repo_ptr = static_cast<std::shared_ptr<minifi::core::ContentRepository>*>(ffr->crp);
  *content_repo_ptr = crp;
  return ffr;
}

flow_file_record* flowfile_to_record(std::shared_ptr<core::FlowFile> ff, ExecutionPlan* plan) {
  if (ff == nullptr) {
    return nullptr;
  }

  return flowfile_to_record(ff, plan->getContentRepo());
}

flow_file_record* get_flowfile(processor_session* session, processor_context* context) {
  auto ff = session->get();
  if(!ff) {
    return nullptr;
  }

  auto ffr = flowfile_to_record(ff, context->getContentRepository());
  // The content of the flow file must be kept in a processor logic
  ffr->keepContent = 1;
  return ffr;
}

flow_file_record * get_next_flow_file(nifi_instance * instance, flow * flow) {
  if (instance == nullptr || nullptr == flow)
    return nullptr;
  flow->reset();
  while (flow->runNextProcessor()) {
  }
  return flowfile_to_record(flow->getCurrentFlowFile(), flow);
}

size_t get_flow_files(nifi_instance *instance, flow *flow, flow_file_record **ff_r, size_t size) {
  if (nullptr == instance || nullptr == flow || nullptr == ff_r)
    return 0;
  size_t i = 0;
  for (; i < size; i++) {
    flow->reset();
    auto ffr = get_next_flow_file(instance, flow);
    if (ffr == nullptr) {
      break;
    }
    ff_r[i] = ffr;
  }
  return i;
}

flow_file_record * get(nifi_instance * instance, flow * flow, processor_session * session) {
  if (nullptr == instance || nullptr == flow || nullptr == session)
    return nullptr;
  auto ff = session->get();
  flow->setNextFlowFile(ff);
  return flowfile_to_record(ff, flow);
}

flow_file_record *invoke(standalone_processor* proc) {
  return invoke_ff(proc, nullptr);
}


flow_file_record *invoke_ff(standalone_processor* proc, const flow_file_record *input_ff) {
  if (proc == nullptr) {
    return nullptr;
  }
  auto plan = ExecutionPlan::getPlan(proc->getUUIDStr());
  if (!plan) {
    // This is not a standalone processor, shouldn't be used with invoke!
    return nullptr;
  }

  plan->reset();

  if (input_ff) {
    auto ff_data = std::make_shared<flowfile_input_params>();
    auto content_repo = static_cast<std::shared_ptr<minifi::core::ContentRepository> *>(input_ff->crp);
    std::shared_ptr<minifi::ResourceClaim> claim = std::make_shared<minifi::ResourceClaim>(input_ff->contentLocation,
                                                                                           *content_repo);
    ff_data->content_stream = (*content_repo)->read(claim);
    ff_data->attributes = *static_cast<std::map<std::string, std::string> *>(input_ff->attributes);

    plan->runNextProcessor(nullptr, ff_data);
  }
  while (plan->runNextProcessor()) {
  }
  return flowfile_to_record(plan->getCurrentFlowFile(), plan.get());
}

flow_file_record *invoke_chunk(standalone_processor* proc, uint8_t* buf, uint64_t size) {
  if (proc == nullptr || buf == nullptr || size == 0) {
    return nullptr;
  }

  auto plan = ExecutionPlan::getPlan(proc->getUUIDStr());
  if (!plan) {
    // This is not a standalone processor, shouldn't be used with invoke!
    return nullptr;
  }

  plan->reset();

  auto ff_data = std::make_shared<flowfile_input_params>();
  ff_data->content_stream = std::make_shared<minifi::io::DataStream>();
  ff_data->content_stream->writeData(buf, size);

  plan->runNextProcessor(nullptr, ff_data);
  while (plan->runNextProcessor()) {
  }

  return flowfile_to_record(plan->getCurrentFlowFile(), plan.get());
}

flow_file_record *invoke_file(standalone_processor* proc, const char* path) {
  FILE *fileptr;
  uint8_t *buffer;
  uint64_t filelen;

  fileptr = fopen(path, "rb");
  if (fileptr == nullptr) {
    return nullptr;
  }
  fseek(fileptr, 0, SEEK_END);
  filelen = ftell(fileptr);
  rewind(fileptr);

  buffer = (uint8_t *)malloc((filelen+1)*sizeof(uint8_t)); // Enough memory for file + \0
  fread(buffer, filelen, 1, fileptr);
  fclose(fileptr);

  flow_file_record* ffr = invoke_chunk(proc, buffer, filelen);
  free(buffer);
  return ffr;
}

int transfer(processor_session* session, flow *flow, const char *rel) {
  if (nullptr == session || nullptr == flow || rel == nullptr) {
    return -1;
  }

  core::Relationship relationship(rel, rel);
  auto ff = flow->getNextFlowFile();
  if (nullptr == ff) {
    return -2;
  }
  session->transfer(ff, relationship);
  return 0;
}

int add_custom_processor(const char * name, processor_logic* logic) {
  return ExecutionPlan::addCustomProcessor(name, logic) ? 0 : -1;
}

int delete_custom_processor(const char * name) {
  return ExecutionPlan::deleteCustomProcessor(name) - 1;
}

int transfer_to_relationship(flow_file_record * ffr, processor_session * ps, const char * relationship) {
  if(ffr == nullptr || ffr->ffp == nullptr || ps == nullptr || relationship == nullptr || strlen(relationship) == 0) {
    return -1;
  }
  auto ff_sptr = reinterpret_cast<std::shared_ptr<core::FlowFile>*>(ffr->ffp);
  ps->transfer(*ff_sptr, core::Relationship(relationship, "desc"));
  return 0;
}
