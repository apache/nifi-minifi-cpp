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

#include <string>
#include <unordered_set>

namespace org::apache::nifi::minifi::c2 {

enum class FlowStatusQueryType {
  processor,
  connection,
  instance,
  systemdiagnostics
};

enum class FlowStatusQueryOption {
  health,
  stats,
  bulletins,
  processorstats,
  flowfilerepositoryusage,
  contentrepositoryusage
};

struct FlowStatusRequest {
  FlowStatusQueryType query_type;
  std::string identifier;
  std::unordered_set<FlowStatusQueryOption> options;

  // The format of the request is in the "<type>:<component>:<options>" format where the component is optional depending on the request type
  // The options are listed in a comma-separated format. For example a request for a processor status would look like: processor:GenerateFlowFile:health,stats
  explicit FlowStatusRequest(std::string_view query_string);
};

}  // namespace org::apache::nifi::minifi::c2
