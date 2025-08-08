/**
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

#include "core/ProcessorDescriptor.h"
#include "utils/minifi-c-utils.h"

namespace org::apache::nifi::minifi::core {

void ProcessorDescriptor::setSupportedRelationships(std::span<const RelationshipDefinition> relationships) {
  std::vector<MinifiRelationship> rels;
  for (auto& rel : relationships) {
    rels.push_back(MinifiRelationship{
      .name = utils::toStringView(rel.name),
      .description = utils::toStringView(rel.description)
    });
  }
  MinifiProcessorDescriptorSetSupportedRelationships(impl_, gsl::narrow<uint32_t>(rels.size()), rels.data());
}

void ProcessorDescriptor::setSupportedProperties(std::span<const PropertyReference> properties) {
  std::vector<std::vector<MinifiStringView>> cache;
  auto props = utils::toProperties(properties, cache);
  MinifiProcessorDescriptorSetSupportedProperties(impl_, gsl::narrow<uint32_t>(props.size()), props.data());
}


}  // namespace org::apache::nifi::minifi::core

