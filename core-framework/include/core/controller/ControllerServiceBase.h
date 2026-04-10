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
#include <utility>
#include <vector>

#include "minifi-cpp/properties/Configure.h"
#include "core/Core.h"
#include "core/ConfigurableComponentImpl.h"
#include "core/Connectable.h"
#include "minifi-cpp/core/controller/ControllerServiceApi.h"
#include "minifi-cpp/core/controller/ControllerServiceHandle.h"
#include "minifi-cpp/core/ControllerServiceApiDefinition.h"
#include "minifi-cpp/core/controller/ControllerServiceMetadata.h"

#define ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES \
  bool supportsDynamicProperties() const override { return SupportsDynamicProperties; }

namespace org::apache::nifi::minifi::core::controller {

class ControllerServiceBase : public ControllerServiceApi {
 public:
  explicit ControllerServiceBase(ControllerServiceMetadata metadata)
      : name_(std::move(metadata.name)),
        uuid_(metadata.uuid),
        logger_(std::move(metadata.logger)) {}

  virtual void initialize() = 0;

  void initialize(ControllerServiceDescriptor& descriptor) final {
    gsl_Expects(!descriptor_);
    descriptor_ = &descriptor;
    auto guard = gsl::finally([&] {descriptor_ = nullptr;});
    initialize();
  }

  void setSupportedProperties(std::span<const PropertyReference> properties) {
    gsl_Expects(descriptor_);
    descriptor_->setSupportedProperties(properties);
  }

  ControllerServiceBase(const ControllerServiceBase&) = delete;
  ControllerServiceBase(ControllerServiceBase&&) = delete;
  ControllerServiceBase& operator=(const ControllerServiceBase&) = delete;
  ControllerServiceBase& operator=(ControllerServiceBase&&) = delete;

  ~ControllerServiceBase() noexcept override = default;

  virtual void onEnable() = 0;

  void onEnable(ControllerServiceContext& context, const std::shared_ptr<Configure>& configuration, const std::vector<std::shared_ptr<ControllerServiceHandle>>& linked_services) final {
    configuration_ = configuration;
    linked_services_ = linked_services;
    gsl_Expects(!context_);
    context_ = &context;
    auto guard = gsl::finally([&] {context_ = nullptr;});
    onEnable();
  }

  [[nodiscard]] std::expected<std::string, std::error_code> getProperty(std::string_view name) const {
    gsl_Expects(context_);
    return context_->getProperty(name);
  }

  [[nodiscard]] std::expected<std::vector<std::string>, std::error_code> getAllPropertyValues(std::string_view name) const {
    gsl_Expects(context_);
    return context_->getAllPropertyValues(name);
  }

  void notifyStop() override {}

  std::string getName() const {
    return name_;
  }

  utils::Identifier getUUID() const {
    return uuid_;
  }


  static constexpr auto ImplementsApis = std::array<ControllerServiceApiDefinition, 0>{};

 protected:
  std::string name_;
  utils::Identifier uuid_;
  std::vector<std::shared_ptr<controller::ControllerServiceHandle> > linked_services_;
  std::shared_ptr<Configure> configuration_;
  // valid during initialize, sink for supported properties
  ControllerServiceDescriptor* descriptor_{nullptr};
  // valid during onEnable, provides property access
  ControllerServiceContext* context_{nullptr};

  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::core::controller
