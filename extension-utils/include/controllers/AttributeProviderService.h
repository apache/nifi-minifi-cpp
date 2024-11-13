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
#include <optional>
#include <unordered_map>
#include <vector>

#include "core/controller/ControllerService.h"
#include "minifi-cpp/controllers/AttributeProviderService.h"

namespace org::apache::nifi::minifi::controllers {

class AttributeProviderServiceImpl : public core::controller::ControllerServiceImpl, public virtual AttributeProviderService {
 public:
  using ControllerServiceImpl::ControllerServiceImpl;

  void yield() override {}
  bool isRunning() const override { return getState() == core::controller::ControllerServiceState::ENABLED; }
  bool isWorkAvailable() override { return false; }
};

}  // namespace org::apache::nifi::minifi::controllers

