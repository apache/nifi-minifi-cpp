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
 */
nifi_instance *create_instance(char *url, nifi_port *port) {
  // make sure that we have a thread safe way of initializing the content directory
  DirectoryConfiguration::initialize();

  nifi_instance *instance = new nifi_instance;

  instance->instance_ptr = new minifi::Instance(url, port->pord_id);
  instance->port.pord_id = port->pord_id;

  return instance;
}

/**
 * Initializes the instance
 */
void initialize_instance(nifi_instance *instance) {
  auto minifi_instance_ref = static_cast<minifi::Instance*>(instance->instance_ptr);
  minifi_instance_ref->setRemotePort(instance->port.pord_id);
}

/**
 * Sets a property within the nifi instance
 * @param instance nifi instance
 * @param key key in which we will set the valiue
 * @param value
 */
void set_property(nifi_instance *instance, char *key, char *value) {
  auto minifi_instance_ref = static_cast<minifi::Instance*>(instance->instance_ptr);
  minifi_instance_ref->getConfiguration()->set(key, value);
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
flow_file_record* create_flowfile(const char *file) {
  flow_file_record *new_ff = new flow_file_record;
  new_ff->attributes = new std::map<std::string, std::string>();
  new_ff->contentLocation = new char[strlen(file)];
  snprintf(new_ff->contentLocation, strlen(file), "%s", file);
  std::ifstream in(file, std::ifstream::ate | std::ifstream::binary);
  // set the size of the flow file.
  new_ff->size = in.tellg();

  return new_ff;
}

/**
 * Reclaims memory associated with a flow file object
 * @param ff flow file record.
 */
void free_flowfile(flow_file_record *ff) {
  if (ff != nullptr) {
    auto map = static_cast<std::map<std::string, std::string>*>(ff->attributes);
    delete[] ff->contentLocation;
    delete map;
    delete ff;
  }
}

/**
 * Adds an attribute
 * @param ff flow file record
 * @param key key
 * @param value value to add
 * @param size size of value
 * @return 0
 */
uint8_t add_attribute(flow_file_record *ff, char *key, void *value, size_t size) {
  auto attribute_map = static_cast<std::map<std::string, std::string>*>(ff->attributes);
  attribute_map->insert(std::pair<std::string, std::string>(key, std::string(static_cast<char*>(value), size)));
  return 0;
}

/*
 * Obtains the attribute.
 * @param ff flow file record
 * @param key key
 * @param caller_attribute caller supplied object in which we will copy the data ptr
 * @return 0 if successful, -1 if the key does not exist
 */
uint8_t get_attribute(flow_file_record *ff, char *key, attribute *caller_attribute) {
  auto attribute_map = static_cast<std::map<std::string, std::string>*>(ff->attributes);
  auto find = attribute_map->find(key);
  if (find != attribute_map->end()) {
    caller_attribute->key = key;
    caller_attribute->value = static_cast<void*>(const_cast<char*>(find->second.data()));
    caller_attribute->value_size = find->second.size();
    return 0;
  }
  return -1;
}

/**
 * Removes a key from the attribute chain
 * @param ff flow file record
 * @param key key to remove
 * @return 0 if removed, -1 otherwise
 */
uint8_t remove_attribute(flow_file_record *ff, char *key) {
  auto attribute_map = static_cast<std::map<std::string, std::string>*>(ff->attributes);
  auto find = attribute_map->find(key);
  if (find != attribute_map->end()) {
    attribute_map->erase(find);
    return 0;
  }
  return -1;
}

/**
 * Transmits the flowfile
 * @param ff flow file record
 * @param instance nifi instance structure
 */
void transmit_flowfile(flow_file_record *ff, nifi_instance *instance) {
  auto minifi_instance_ref = static_cast<minifi::Instance*>(instance->instance_ptr);
  // in the unlikely event the user forgot to initialize the instance, we shall do it for them.
  if (UNLIKELY(minifi_instance_ref->isRPGConfigured() == false)) {
    minifi_instance_ref->setRemotePort(instance->port.pord_id);
  }

  auto attribute_map = static_cast<std::map<std::string, std::string>*>(ff->attributes);

  auto no_op = minifi_instance_ref->getNoOpRepository();

  auto content_repo = minifi_instance_ref->getContentRepository();

  std::shared_ptr<minifi::ResourceClaim> claim = std::make_shared<minifi::ResourceClaim>(ff->contentLocation, content_repo);
  claim->increaseFlowFileRecordOwnedCount();
  claim->increaseFlowFileRecordOwnedCount();

  auto ffr = std::make_shared<minifi::FlowFileRecord>(no_op, content_repo, *attribute_map, claim);
  ffr->addAttribute("nanofi.version", API_VERSION);
  ffr->setSize(ff->size);

  std::string port_uuid = instance->port.pord_id;

  minifi_instance_ref->transfer(ffr);
}

flow *create_flow(nifi_instance *instance, const char *first_processor) {
  auto minifi_instance_ref = static_cast<minifi::Instance*>(instance->instance_ptr);
  flow *new_flow = new flow;

  auto execution_plan = new ExecutionPlan(minifi_instance_ref->getContentRepository(), minifi_instance_ref->getNoOpRepository(), minifi_instance_ref->getNoOpRepository());

  new_flow->plan = execution_plan;

  // automatically adds it with success
  execution_plan->addProcessor(first_processor, first_processor);

  return new_flow;
}

void free_flow(flow *flow) {
  if (flow == nullptr)
    return;
  auto execution_plan = static_cast<ExecutionPlan*>(flow->plan);
  delete execution_plan;
  delete flow;
}

flow_file_record *get_next_flow_file(nifi_instance *instance, flow *flow) {
  auto execution_plan = static_cast<ExecutionPlan*>(flow->plan);

  execution_plan->reset();
  while (execution_plan->runNextProcessor()) {
  }
  auto ff = execution_plan->getCurrentFlowFile();
  if (ff == nullptr)
    return nullptr;
  auto claim = ff->getResourceClaim();

  if (claim != nullptr) {
    // create a flow file.
    claim->increaseFlowFileRecordOwnedCount();
    auto path = claim->getContentFullPath();
    auto ffr = create_flowfile(path.c_str());
    std::cout << "dang created " << path << " " << ff->getSize() << std::endl;
    return ffr;
  } else {
    return nullptr;
  }
}

size_t get_flow_files(nifi_instance *instance, flow *flow, flow_file_record **ff_r, size_t size) {
  auto execution_plan = static_cast<ExecutionPlan*>(flow->plan);
  int i = 0;
  for (; i < size; i++) {
    execution_plan->reset();
    while (execution_plan->runNextProcessor()) {
    }
    auto ff = execution_plan->getCurrentFlowFile();
    if (ff == nullptr)
      break;
    auto claim = ff->getResourceClaim();

    if (claim != nullptr) {
      claim->increaseFlowFileRecordOwnedCount();

      auto path = claim->getContentFullPath();
      // create a flow file.
      ff_r[i] = create_flowfile(path.c_str());
    } else {
      break;
    }
  }
  return i;
}
