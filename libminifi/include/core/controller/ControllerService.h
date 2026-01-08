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
#include "minifi-cpp/core/ControllerServiceApiDefinition.h"

namespace org::apache::nifi::minifi::core::controller {

enum ControllerServiceState {
  DISABLED,
  DISABLING,
  ENABLING,
  ENABLED
};

/**
 * Controller Service base class that contains some pure virtual methods.
 *
 * Design: OnEnable is executed when the controller service is being enabled.
 * Note that keeping state here must be protected  in this function.
 */
class ControllerService : public ConfigurableComponentImpl, public CoreComponentImpl {
  class ControllerServiceDescriptorImpl : public ControllerServiceDescriptor {
   public:
    explicit ControllerServiceDescriptorImpl(ControllerService& impl): impl_(impl) {}
    void setSupportedProperties(std::span<const PropertyReference> properties) override;

   private:
    ControllerService& impl_;
  };

  class ControllerServiceContextImpl : public ControllerServiceContext {
    public:
      explicit ControllerServiceContextImpl(ControllerService& impl): impl_(impl) {}
      [[nodiscard]] nonstd::expected<std::string, std::error_code> getProperty(std::string_view name) const override;
      [[nodiscard]] nonstd::expected<std::vector<std::string>, std::error_code> getAllPropertyValues(std::string_view name) const override;

    private:
      ControllerService& impl_;
  };

 public:
  explicit ControllerService(std::string_view name, const utils::Identifier& uuid, std::unique_ptr<ControllerServiceApi> impl)
      : CoreComponentImpl(name, uuid),
        impl_(std::move(impl)),
        configuration_(Configure::create()) {
    current_state_ = DISABLED;
  }

  void initialize() final {
    ControllerServiceDescriptorImpl descriptor{*this};
    impl_->initialize(descriptor);
  }

  bool supportsDynamicRelationships() const final {
    return false;
  }

  ~ControllerService() override {
    notifyStop();
  }

  /**
   * Replaces the configuration object within the controller service.
   */
  void setConfiguration(const std::shared_ptr<Configure> &configuration) {
    configuration_ = configuration;
  }

  ControllerServiceState getState() const {
    return current_state_.load();
  }

  /**
   * Function is called when Controller Services are enabled and being run
   */
  void onEnable() {
    ControllerServiceContextImpl context{*this};
    std::vector<std::shared_ptr<ControllerServiceInterface>> service_interfaces;
    for (auto& service : linked_services_) {
      service_interfaces.emplace_back(std::shared_ptr<ControllerServiceInterface>(service, service->impl_->getControllerServiceInterface()));
    }
    impl_->onEnable(context, configuration_, service_interfaces);
  }

  /**
   * Function is called when Controller Services are disabled
   */
  void notifyStop() {
    impl_->notifyStop();
  }

  void setState(ControllerServiceState state) {
    current_state_ = state;
    if (state == DISABLED) {
      notifyStop();
    }
  }

  void setLinkedControllerServices(const std::vector<std::shared_ptr<controller::ControllerService>> &services) {
    linked_services_ = services;
  }

  template<typename T = ControllerServiceInterface>
  gsl::not_null<T*> getImplementation() {
    return gsl::make_not_null(dynamic_cast<T*>(impl_.get()));
  }

  bool supportsDynamicProperties() const final {
    return false;
  }

  static constexpr auto ImplementsApis = std::array<ControllerServiceApiDefinition, 0>{};

 protected:
  std::unique_ptr<ControllerServiceApi> impl_;
  std::vector<std::shared_ptr<controller::ControllerService> > linked_services_;
  std::shared_ptr<Configure> configuration_;
  mutable std::atomic<ControllerServiceState> current_state_;
  bool canEdit() override {
    return true;
  }
};

inline void ControllerService::ControllerServiceDescriptorImpl::setSupportedProperties(std::span<const PropertyReference> properties) {
  impl_.setSupportedProperties(properties);
}

inline nonstd::expected<std::string, std::error_code> ControllerService::ControllerServiceContextImpl::getProperty(std::string_view name) const {
  return impl_.getProperty(name);
}

inline nonstd::expected<std::vector<std::string>, std::error_code> ControllerService::ControllerServiceContextImpl::getAllPropertyValues(std::string_view name) const {
  return impl_.getAllPropertyValues(name);
}


}  // namespace org::apache::nifi::minifi::core::controller
