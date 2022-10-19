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

#include <utility>

#include "controllers/AttributeProviderService.h"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::processors {

const core::Property UpdateAttribute::AttributeProviderService(
        core::PropertyBuilder::createProperty("Attribute Provider Service")
                ->withDescription("Provides a list of key-value pair records which can be used in other (dynamic) properties using Expression Language.")
                ->asType<controllers::AttributeProviderService>()
                ->build());

const core::Relationship UpdateAttribute::Success("success", "All files are routed to success");
const core::Relationship UpdateAttribute::Failure("failure", "Failed files are transferred to failure");

void UpdateAttribute::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void UpdateAttribute::onSchedule(core::ProcessContext* context, core::ProcessSessionFactory* /*sessionFactory*/) {
  gsl_Expects(context);

  attributes_.clear();
  const auto& dynamic_prop_keys = context->getDynamicPropertyKeys();
  logger_->log_info("UpdateAttribute registering %d keys", dynamic_prop_keys.size());

  attributes_.reserve(dynamic_prop_keys.size());

  for (const auto& key : dynamic_prop_keys) {
    attributes_.emplace_back(core::PropertyBuilder::createProperty(key)->withDescription("auto generated")->supportsExpressionLanguage(true)->build());
    logger_->log_info("UpdateAttribute registered attribute '%s'", key);
  }

  attribute_provider_service_ = controllers::AttributeProviderService::getFromProperty(*context, AttributeProviderService);
}

void UpdateAttribute::onTrigger(core::ProcessContext* context, core::ProcessSession* session) {
  auto flow_file = session->get();

  // Do nothing if there are no incoming files
  if (!flow_file) {
    return;
  }

  try {
    for (const auto& attribute : attributes_) {
      std::string value;
      context->getDynamicProperty(attribute, value, flow_file, getAttributeMap());
      flow_file->setAttribute(attribute.getName(), value);
      logger_->log_info("Set attribute '%s' of flow file '%s' with value '%s'", attribute.getName(), flow_file->getUUIDStr(), value);
    }
    session->transfer(flow_file, Success);
  } catch (const std::exception& e) {
    logger_->log_error("Caught exception while updating attributes: %s", e.what());
    session->transfer(flow_file, Failure);
    yield();
  }
}

std::unordered_map<std::string, std::string> UpdateAttribute::getAttributeMap() const {
  if (!attribute_provider_service_) {
    return {};
  }

  auto attribute_map_list_opt = attribute_provider_service_->getAttributes();
  if (!attribute_map_list_opt || attribute_map_list_opt->empty()) {
    return {};
  }

  // flatten vector of maps to a single map
  std::unordered_map<std::string, std::string> ret;
  for (auto& attribute_map : *attribute_map_list_opt) {
    for (auto& [key, value] : attribute_map) {
      ret[key] = std::move(value);
    }
  }

  return ret;
}

REGISTER_RESOURCE(UpdateAttribute, Processor);

}  // namespace org::apache::nifi::minifi::processors
