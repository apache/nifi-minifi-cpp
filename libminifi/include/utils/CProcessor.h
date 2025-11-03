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

#include "minifi-cpp/core/Annotation.h"
#include "core/ProcessorMetrics.h"
#include "minifi-c/minifi-c.h"
#include "minifi-cpp/agent/agent_docs.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "minifi-cpp/core/ProcessorApi.h"
#include "minifi-cpp/core/ProcessorDescriptor.h"
#include "minifi-cpp/core/ProcessorMetadata.h"

namespace org::apache::nifi::minifi::utils {

class CProcessor;

class CProcessorMetricsWrapper : public minifi::core::ProcessorMetricsExtension {
 public:
  explicit CProcessorMetricsWrapper(const CProcessor& source_processor)
      : source_processor_(source_processor) {
  }

  std::vector<minifi::state::response::SerializedResponseNode> serialize() override;

  std::vector<minifi::state::PublishedMetric> calculateMetrics() override;

 private:
  const CProcessor& source_processor_;
};

struct CProcessorClassDescription {
  std::string name;
  std::vector<minifi::core::Property> class_properties;
  std::vector<minifi::core::Relationship> class_relationships;
  bool supports_dynamic_properties;
  bool supports_dynamic_relationships;
  minifi::core::annotation::Input input_requirement;
  bool is_single_threaded;

  MinifiProcessorCallbacks callbacks;
};

class CProcessor : public minifi::core::ProcessorApi {
 public:
  CProcessor(CProcessorClassDescription class_description, minifi::core::ProcessorMetadata metadata)
      : class_description_(std::move(class_description)),
        metrics_extension_(std::make_shared<CProcessorMetricsWrapper>(*this)) {
    metadata_ = metadata;
    MinifiProcessorMetadata c_metadata;
    auto uuid_str = metadata.uuid.to_string();
    c_metadata.uuid = MinifiStringView{.data = uuid_str.data(), .length = gsl::narrow<uint32_t>(uuid_str.length())};
    c_metadata.name = MinifiStringView{.data = metadata.name.data(), .length = gsl::narrow<uint32_t>(metadata.name.length())};
    c_metadata.logger = reinterpret_cast<MinifiLogger*>(&metadata_.logger);
    impl_ = class_description_.callbacks.create(c_metadata);
  }
  CProcessor(CProcessorClassDescription class_description, minifi::core::ProcessorMetadata metadata, OWNED void* impl)
      : class_description_(std::move(class_description)),
        impl_(impl),
        metadata_(metadata),
        metrics_extension_(std::make_shared<CProcessorMetricsWrapper>(*this)) {}
  ~CProcessor() override {
    class_description_.callbacks.destroy(impl_);
  }

  bool isWorkAvailable() override {
    return static_cast<bool>(class_description_.callbacks.isWorkAvailable(impl_));
  }

  void restore(const std::shared_ptr<minifi::core::FlowFile>& file) override {
    class_description_.callbacks.restore(impl_, reinterpret_cast<OWNED MinifiFlowFile*>(new std::shared_ptr<minifi::core::FlowFile>(file)));
  }

  bool supportsDynamicProperties() const override {
    return class_description_.supports_dynamic_properties;
  }

  bool supportsDynamicRelationships() const override {
    return class_description_.supports_dynamic_relationships;
  }

  void initialize(minifi::core::ProcessorDescriptor& descriptor) override {
    descriptor.setSupportedProperties(std::span(class_description_.class_properties));

    std::vector<minifi::core::RelationshipDefinition> relationships;
    for (auto& rel : class_description_.class_relationships) {relationships.push_back(rel.getDefinition());}
    descriptor.setSupportedRelationships(relationships);
  }

  bool isSingleThreaded() const override {
    return class_description_.is_single_threaded;
  }

  std::string getProcessorType() const override {
    return class_description_.name;
  }

  bool getTriggerWhenEmpty() const override {
    return static_cast<bool>(class_description_.callbacks.getTriggerWhenEmpty(impl_));
  }

  void onTrigger(minifi::core::ProcessContext& process_context, minifi::core::ProcessSession& process_session) override {
    std::optional<std::string> error;
    auto status = class_description_.callbacks.onTrigger(impl_, reinterpret_cast<MinifiProcessContext*>(&process_context), reinterpret_cast<MinifiProcessSession*>(&process_session));
    if (status == MINIFI_STATUS_PROCESSOR_YIELD) {
      process_context.yield();
      return;
    }
    if (status != MINIFI_STATUS_SUCCESS) {
      throw minifi::Exception(minifi::ExceptionType::PROCESSOR_EXCEPTION, "Error while triggering processor");
    }
  }

  void onSchedule(minifi::core::ProcessContext& process_context, minifi::core::ProcessSessionFactory& /*process_session_factory*/) override {
    std::optional<std::string> error;
    auto status = class_description_.callbacks.onSchedule(impl_, reinterpret_cast<MinifiProcessContext*>(&process_context));
    if (status != MINIFI_STATUS_SUCCESS) {
      throw minifi::Exception(minifi::ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Error while scheduling processor");
    }
  }

  void onUnSchedule() override {
    class_description_.callbacks.onUnSchedule(impl_);
  }

  void notifyStop() override {
    class_description_.callbacks.onUnSchedule(impl_);
  }

  minifi::core::annotation::Input getInputRequirement() const override {
    return class_description_.input_requirement;
  }

  std::shared_ptr<minifi::core::ProcessorMetricsExtension> getMetricsExtension() const override {
    return metrics_extension_;
  }

  std::vector<minifi::state::PublishedMetric> getCustomMetrics() const;

  void forEachLogger(const std::function<void(std::shared_ptr<minifi::core::logging::Logger>)>&) override {}

  std::string getName() const {
    return metadata_.name;
  }

  minifi::utils::Identifier getUUID() const {
    return metadata_.uuid;
  }

 private:
  CProcessorClassDescription class_description_;
  OWNED void* impl_;
  minifi::core::ProcessorMetadata metadata_;
  gsl::not_null<std::shared_ptr<minifi::core::ProcessorMetricsExtension>> metrics_extension_;
};

void useCProcessorClassDescription(const MinifiProcessorClassDescription& class_description, const std::function<void(ClassDescription, CProcessorClassDescription)>& fn);

}  // namespace org::apache::nifi::minifi::utils
