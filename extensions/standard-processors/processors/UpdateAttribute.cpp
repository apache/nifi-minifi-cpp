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

#include "core/PropertyDefinitionBuilder.h"
#include "core/Resource.h"
#include "core/ProcessContext.h"

namespace org::apache::nifi::minifi::processors {

void UpdateAttribute::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void UpdateAttribute::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  auto flow_file = session.get();

  // Do nothing if there are no incoming files
  if (!flow_file) {
    return;
  }

  try {
    for (const auto& [dyn_prop_name, dyn_prop_value] : context.getDynamicProperties(flow_file.get())) {
      flow_file->setAttribute(dyn_prop_name, dyn_prop_value);
      logger_->log_debug("Set attribute '{}' of flow file '{}' with value '{}'", dyn_prop_name, flow_file->getUUIDStr(), dyn_prop_value);
    }
    session.transfer(flow_file, Success);
  } catch (const std::exception &e) {
    logger_->log_error("Caught exception while updating attributes: {}", e.what());
    session.transfer(flow_file, Failure);
    yield();
  }
}

REGISTER_RESOURCE(UpdateAttribute, Processor);

}  // namespace org::apache::nifi::minifi::processors
