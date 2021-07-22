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
#include <set>

#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Relationship RouteOnAttribute::Unmatched("unmatched", "Files which do not match any expression are routed here");
core::Relationship RouteOnAttribute::Failure("failure", "Failed files are transferred to failure");

void RouteOnAttribute::initialize() {
  std::set<core::Property> properties;
  setSupportedProperties(properties);
  std::set<core::Relationship> relationships;
  relationships.insert(Unmatched);
  relationships.insert(Failure);
  setSupportedRelationships(relationships);
}

void RouteOnAttribute::onDynamicPropertyModified(const core::Property& /*orig_property*/, const core::Property &new_property) {
  // Update the routing table when routes are added via dynamic properties.
  route_properties_[new_property.getName()] = new_property;

  std::set<core::Relationship> relationships;

  for (const auto &route : route_properties_) {
    core::Relationship route_rel { route.first, "Dynamic route" };
    route_rels_[route.first] = route_rel;
    relationships.insert(route_rel);
    logger_->log_info("RouteOnAttribute registered route '%s' with expression '%s'", route.first, route.second.getValue().to_string());
  }

  relationships.insert(Unmatched);
  relationships.insert(Failure);
  setSupportedRelationships(relationships);
}

void RouteOnAttribute::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  auto flow_file = session->get();

  // Do nothing if there are no incoming files
  if (!flow_file) {
    return;
  }

  try {
    bool did_match = false;

    // Perform dynamic routing logic
    for (const auto &route : route_properties_) {
      std::string do_route;
      context->getDynamicProperty(route.second, do_route, flow_file);

      if (do_route == "true") {
        did_match = true;
        auto clone = session->clone(flow_file);
        session->transfer(clone, route_rels_[route.first]);
      }
    }

    if (!did_match) {
      session->transfer(flow_file, Unmatched);
    } else {
      session->remove(flow_file);
    }
  } catch (const std::exception &e) {
    logger_->log_error("Caught exception while updating attributes: %s", e.what());
    session->transfer(flow_file, Failure);
    yield();
  }
}

REGISTER_RESOURCE(RouteOnAttribute, "Routes FlowFiles based on their Attributes using the Attribute Expression Language.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
