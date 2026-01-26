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
#include "minifi-cpp/core/controller/ControllerServiceInterface.h"
#include "minifi-cpp/core/ControllerServiceTypeDefinition.h"
#include "minifi-cpp/core/ControllerServiceMetadata.h"

namespace org::apache::nifi::minifi::core::controller {

// A base class that helps with controller service development, contains common functionalities.
class ControllerServiceBase : public ControllerServiceApi {
 public:
  explicit ControllerServiceBase(ControllerServiceMetadata metadata)
      : name_(std::move(metadata.name)),
        uuid_(metadata.uuid),
        logger_(std::move(metadata.logger)) {}

  virtual void initialize() {}

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

  ~ControllerServiceBase() override {}

  virtual void onEnable() {}

  /**
   * Function is called when Controller Services are enabled and being run
   */
  void onEnable(ControllerServiceContext& context, const std::shared_ptr<Configure>& configuration, const std::vector<std::shared_ptr<ControllerServiceInterface>>& linked_services) final {
    configuration_ = configuration;
    linked_services_ = linked_services;
    gsl_Expects(!context_);
    context_ = &context;
    auto guard = gsl::finally([&] {context_ = nullptr;});
    onEnable();
  }

  [[nodiscard]] nonstd::expected<std::string, std::error_code> getProperty(std::string_view name) const {
    gsl_Expects(context_);
    return context_->getProperty(name);
  }

  [[nodiscard]] nonstd::expected<std::vector<std::string>, std::error_code> getAllPropertyValues(std::string_view name) const {
    gsl_Expects(context_);
    return context_->getAllPropertyValues(name);
  }

  /**
   * Function is called when Controller Services are disabled
   */
  void notifyStop() override {}

  std::string getName() const {
    return name_;
  }

  utils::Identifier getUUID() const {
    return uuid_;
  }


  static constexpr auto ImplementsApis = std::array<ControllerServiceTypeDefinition, 0>{};

 protected:
  std::string name_;
  utils::Identifier uuid_;
  std::vector<std::shared_ptr<controller::ControllerServiceInterface> > linked_services_;
  std::shared_ptr<Configure> configuration_;
  // valid during initialize, sink for supported properties
  ControllerServiceDescriptor* descriptor_{nullptr};
  // valid during onEnable, provides property access
  ControllerServiceContext* context_{nullptr};

  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::core::controller
