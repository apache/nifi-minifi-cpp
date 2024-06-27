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
#pragma once


#include "core/Core.h"
#include "core/flow/Node.h"
#include "core/flow/FlowSchema.h"

namespace org::apache::nifi::minifi::core::flow {

class FlowMigrator : public CoreComponent {
 public:
  explicit FlowMigrator(const std::string_view name, const utils::Identifier& uuid = {}) : core::CoreComponent(name, uuid) {}

  virtual void migrate(Node& flow_root, const FlowSchema& schema) = 0;

 protected:
  void doOnProcessGroup(Node& process_group, const FlowSchema& schema, auto func) const {
    func(process_group, schema);
    const auto process_group_children = process_group[schema.process_groups];
    if (process_group.isSequence()) {
      for (auto child_process_group_node : process_group_children) {
       doOnProcessGroup(child_process_group_node, schema, func);
      }
    }
  }

  [[nodiscard]] std::vector<Node> getProcessors(const Node& root_node, const FlowSchema& schema, const std::string_view processor_to_get) const {
    std::vector<Node> processors;
    auto root_group = root_node[schema.root_group];
    doOnProcessGroup(root_group, schema, [&processors, processor_to_get](auto process_group, const FlowSchema& schema) {
      for (auto processor_node : process_group[schema.processors]) {
        auto processor_type_str = processor_node[schema.type].getString();
        if (auto processor_type = processor_node[schema.type].getString(); processor_type && processor_type->find(processor_to_get) != std::string::npos) {
          processors.push_back(processor_node);
        }
      }
    });

    return processors;
  }
};


}  // namespace org::apache::nifi::minifi::core::flow
