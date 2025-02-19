/**
 * @file RouteOnAttribute.cpp
 * RouteOnAttribute class implementation
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

#include "RouteOnAttribute.h"

#include <memory>
#include <string>
#include <vector>

#include "core/Resource.h"

namespace org::apache::nifi::minifi::processors {

void RouteOnAttribute::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void RouteOnAttribute::onSchedule(core::ProcessContext &, core::ProcessSessionFactory&) {
  route_properties_ = getDynamicProperties();

  const auto static_relationships = RouteOnAttribute::Relationships;
  std::vector<core::RelationshipDefinition> relationships(static_relationships.begin(), static_relationships.end());

  for (const auto& route : route_properties_) {
    core::RelationshipDefinition route_rel{ route.first, "Dynamic route" };
    route_rels_[route.first] = route_rel;
    relationships.push_back(route_rel);
    logger_->log_info("RouteOnAttribute registered route '{}' with expression '{}'", route.first, route.second);
  }
  setSupportedRelationships(relationships);
}

void RouteOnAttribute::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  auto flow_file = session.get();

  // Do nothing if there are no incoming files
  if (!flow_file) {
    return;
  }

  try {
    bool did_match = false;

    // Perform dynamic routing logic
    for (const auto &route : route_properties_) {
      std::string do_route = context.getDynamicProperty(route.first, flow_file.get()).value_or("");

      if (do_route == "true") {
        did_match = true;
        auto clone = session.clone(*flow_file);
        session.transfer(clone, route_rels_[route.first]);
      }
    }

    if (!did_match) {
      session.transfer(flow_file, Unmatched);
    } else {
      session.remove(flow_file);
    }
  } catch (const std::exception &e) {
    logger_->log_error("Caught exception while updating attributes: type: {}, what: {}", typeid(e).name(), e.what());
    session.transfer(flow_file, Failure);
    yield();
  }
}

REGISTER_RESOURCE(RouteOnAttribute, Processor);

}  // namespace org::apache::nifi::minifi::processors
