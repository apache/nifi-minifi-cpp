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
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "minifi-cpp/core/state/MetricsPublisher.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::state {

gsl::not_null<std::unique_ptr<MetricsPublisher>> createMetricsPublisher(const std::string& name, const std::shared_ptr<Configure>& configuration,
  const std::shared_ptr<state::response::ResponseNodeLoader>& response_node_loader);
std::vector<gsl::not_null<std::unique_ptr<MetricsPublisher>>> createMetricsPublishers(
  const std::shared_ptr<Configure>& configuration, const std::shared_ptr<state::response::ResponseNodeLoader>& response_node_loader);

}  // namespace org::apache::nifi::minifi::state
