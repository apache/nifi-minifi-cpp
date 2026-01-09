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

#include "minifi-cpp/core/controller/ControllerService.h"
#include "minifi-cpp/core/ControllerServiceApiDefinition.h"

namespace org::apache::nifi::minifi::controllers {

enum class ProxyType {
  HTTP
};

class ProxyConfigurationServiceInterface : public virtual core::controller::ControllerService {
 public:
  static constexpr auto ProvidesApi = core::ControllerServiceApiDefinition {
    .artifact = "minifi-system",
    .group = "org.apache.nifi.minifi",
    .type = "org.apache.nifi.minifi.controllers.ProxyConfigurationServiceInterface",
  };

  virtual std::string getHost() const = 0;
  virtual std::optional<uint16_t> getPort() const = 0;
  virtual std::optional<std::string> getUsername() const = 0;
  virtual std::optional<std::string> getPassword() const = 0;
  virtual ProxyType getProxyType() const = 0;
};

}  // namespace org::apache::nifi::minifi::controllers
