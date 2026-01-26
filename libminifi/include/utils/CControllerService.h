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
#include <vector>

#include "minifi-c/minifi-c.h"
#include "minifi-cpp/Exception.h"
#include "minifi-cpp/core/ControllerServiceMetadata.h"
#include "minifi-cpp/core/Property.h"
#include "minifi-cpp/core/controller/ControllerServiceApi.h"

namespace org::apache::nifi::minifi::utils {
class CControllerService;


struct CControllerServiceClassDescription {
  std::string group_name;
  std::string name;
  std::string version;

  std::vector<core::Property> class_properties;

  MinifiControllerServiceCallbacks callbacks;
};

class CControllerService final : public core::controller::ControllerServiceApi, public core::controller::ControllerServiceInterface {
 public:
  CControllerService(CControllerServiceClassDescription class_description, core::ControllerServiceMetadata metadata)
    : class_description_(std::move(class_description)),
      metadata_(std::move(metadata)) {
    MinifiControllerServiceMetadata c_metadata;
    auto uuid_str = metadata_.uuid.to_string();
    c_metadata.uuid = MinifiStringView{.data = uuid_str.data(), .length = uuid_str.length()};
    c_metadata.name = MinifiStringView{.data = metadata_.name.data(), .length = metadata_.name.length()};
    c_metadata.logger = reinterpret_cast<MinifiLogger*>(&metadata_.logger);
    impl_ = class_description_.callbacks.create(c_metadata);
  }
  CControllerService(CControllerServiceClassDescription class_description, core::ControllerServiceMetadata metadata, gsl::owner<void*> impl)
      : class_description_(std::move(class_description)),
        impl_(impl),
        metadata_(std::move(metadata)) {}
  ~CControllerService() override {
    class_description_.callbacks.destroy(impl_);
  }

  void initialize(core::controller::ControllerServiceDescriptor& descriptor) override {
    descriptor.setSupportedProperties(std::span(class_description_.class_properties));
  }

  void onEnable(core::controller::ControllerServiceContext& controller_service_context,
      const std::shared_ptr<Configure>&,
      const std::vector<std::shared_ptr<core::controller::ControllerServiceInterface>>&) override {
    const auto enable_status = class_description_.callbacks.enable(impl_, reinterpret_cast<MinifiControllerServiceContext*>(&controller_service_context));
    if (enable_status != MINIFI_STATUS_SUCCESS) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Could not enable controller service");
    }
  }

  void notifyStop() override {
    class_description_.callbacks.notifyStop(impl_);
  }

  ControllerServiceInterface* getControllerServiceInterface() override {
    return this;
  }

  [[nodiscard]] std::string getName() const {
    return metadata_.name;
  }

  [[nodiscard]] Identifier getUUID() const {
    return metadata_.uuid;
  }

  [[nodiscard]] void* getImpl() const {
    return this->impl_;
  }

  [[nodiscard]] const CControllerServiceClassDescription& getClassDescription() const {
    return class_description_;
  }

 private:
  CControllerServiceClassDescription class_description_;
  gsl::owner<void*> impl_;
  minifi::core::ControllerServiceMetadata metadata_;
};

void useCControllerServiceClassDescription(const MinifiControllerServiceClassDefinition& class_description,
    const BundleIdentifier& bundle_id,
    const std::function<void(ClassDescription, CControllerServiceClassDescription)>& fn);

}  // namespace org::apache::nifi::minifi::utils
