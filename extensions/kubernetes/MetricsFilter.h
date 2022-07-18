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

#include <functional>
#include <string>

#include "utils/expected.h"

namespace org::apache::nifi::minifi::kubernetes::metrics {

[[nodiscard]] nonstd::expected<std::string, std::string> filter(
    const std::string& metrics_json,
    const std::function<bool(const std::string& name_space, const std::string& pod_name, const std::string& container_name)>& filter_function);

}  // namespace org::apache::nifi::minifi::kubernetes::metrics
