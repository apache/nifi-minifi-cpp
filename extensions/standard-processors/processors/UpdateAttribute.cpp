/**
 * @file UpdateAttribute.cpp
 * UpdateAttribute class implementation
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

#include "UpdateAttribute.h"

#include <memory>
#include <string>
#include <set>

#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Relationship UpdateAttribute::Success("success", "All files are routed to success");
core::Relationship UpdateAttribute::Failure("failure", "Failed files are transferred to failure");

void UpdateAttribute::initialize() {
  std::set<core::Property> properties;
  setSupportedProperties(properties);

  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  relationships.insert(Failure);
  setSupportedRelationships(relationships);
}

void UpdateAttribute::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* /*sessionFactory*/) {
  attributes_.clear();
  const auto &dynamic_prop_keys = context->getDynamicPropertyKeys();
  logger_->log_info("UpdateAttribute registering %d keys", dynamic_prop_keys.size());

  for (const auto &key : dynamic_prop_keys) {
    attributes_.emplace_back(core::PropertyBuilder::createProperty(key)->withDescription("auto generated")->supportsExpressionLanguage(true)->build());
    logger_->log_info("UpdateAttribute registered attribute '%s'", key);
  }
}

void UpdateAttribute::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  auto flow_file = session->get();

  // Do nothing if there are no incoming files
  if (!flow_file) {
    return;
  }

  try {
    for (const auto &attribute : attributes_) {
      std::string value;
      context->getDynamicProperty(attribute, value, flow_file);
      flow_file->setAttribute(attribute.getName(), value);
      logger_->log_info("Set attribute '%s' of flow file '%s' with value '%s'", attribute.getName(), flow_file->getUUIDStr(), value);
    }
    session->transfer(flow_file, Success);
  } catch (const std::exception &e) {
    logger_->log_error("Caught exception while updating attributes: %s", e.what());
    session->transfer(flow_file, Failure);
    yield();
  }
}

REGISTER_RESOURCE(UpdateAttribute, "This processor updates the attributes of a FlowFile using properties that are added by the user. "
  "This allows you to set default attribute changes that affect every FlowFile going through the processor, equivalent to the \"basic\" usage in Apache NiFi.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
