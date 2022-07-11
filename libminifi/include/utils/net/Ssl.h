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
#include <memory>
#include <optional>

#include "core/ProcessContext.h"
#include "core/Property.h"
#include "core/logging/Logger.h"

namespace org::apache::nifi::minifi::utils::net {

struct SslData {
  std::string ca_loc;
  std::string cert_loc;
  std::string key_loc;
  std::string key_pw;

  bool isValid() const {
    return !cert_loc.empty() && !key_loc.empty();
  }
};

std::optional<utils::net::SslData> getSslData(const core::ProcessContext& context, const core::Property& ssl_prop, const std::shared_ptr<core::logging::Logger>& logger);

}  // namespace org::apache::nifi::minifi::utils::net
