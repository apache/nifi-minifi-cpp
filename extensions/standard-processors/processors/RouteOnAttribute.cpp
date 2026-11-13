// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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

void RouteOnAttribute::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  route_properties_ = context.getDynamicProperties();

  const auto static_relationships = RouteOnAttribute::Relationships;
  std::vector<core::RelationshipDefinition> relationships(static_relationships.begin(), static_relationships.end());

  for (const auto& route : route_properties_) {
    core::RelationshipDefinition route_rel{ route.first, "Dynamic route" };
    route_rels_[route.first] = route_rel;
    relationships.push_back(route_rel);
    logger_->log_info("RouteOnAttribute registered route '{}' with expression '{}'", route.first, route.second);
  }
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
    context.yield();
  }
}

REGISTER_RESOURCE(RouteOnAttribute, Processor);

}  // namespace org::apache::nifi::minifi::processors
