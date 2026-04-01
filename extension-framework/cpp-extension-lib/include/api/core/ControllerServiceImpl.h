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

#include <memory>
#include <string>

#include "ControllerServiceContext.h"
#include "minifi-cpp/core/ControllerServiceMetadata.h"
#include "minifi-cpp/utils/Id.h"
#include "utils/SmallString.h"

namespace org::apache::nifi::minifi::api {

class Connection;

namespace core {

class ControllerServiceImpl {
 public:
  explicit ControllerServiceImpl(minifi::core::ControllerServiceMetadata metadata);

  ControllerServiceImpl(const ControllerServiceImpl&) = delete;
  ControllerServiceImpl(ControllerServiceImpl&&) = delete;
  ControllerServiceImpl& operator=(const ControllerServiceImpl&) = delete;
  ControllerServiceImpl& operator=(ControllerServiceImpl&&) = delete;

  virtual ~ControllerServiceImpl();

  MinifiStatus enable(ControllerServiceContext&);
  void notifyStop();

  [[nodiscard]] std::string getName() const;
  [[nodiscard]] minifi::utils::Identifier getUUID() const;
  [[nodiscard]] minifi::utils::SmallString<36> getUUIDStr() const;

 protected:
  virtual MinifiStatus enableImpl(api::core::ControllerServiceContext&) = 0;
  virtual void notifyStopImpl() {}

  minifi::core::ControllerServiceMetadata metadata_;

  std::shared_ptr<minifi::core::logging::Logger> logger_;
};

}  // namespace core
}  // namespace org::apache::nifi::minifi::api
