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

#include "minifi-c/minifi-c.h"

#include <memory>
#include <vector>

#include "agent/agent_docs.h"
#include "core/ProcessorMetrics.h"
#include "minifi-cpp/core/Annotation.h"
#include "minifi-cpp/core/ClassLoader.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "minifi-cpp/core/ProcessSession.h"
#include "minifi-cpp/core/ProcessorApi.h"
#include "minifi-cpp/core/ProcessorDescriptor.h"
#include "minifi-cpp/core/ProcessorFactory.h"
#include "minifi-cpp/core/ProcessorMetadata.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "minifi-cpp/core/state/PublishedMetricProvider.h"
#include "minifi-cpp/core/state/Value.h"
#include "minifi-cpp/Exception.h"
#include "minifi-cpp/core/extension/ExtensionManager.h"
#include "utils/PropertyErrors.h"

namespace minifi = org::apache::nifi::minifi;

namespace {

std::string toString(MinifiStringView sv) {
  return std::string(sv.data, sv.length);
}

std::string_view toStringView(MinifiStringView sv) {
  return std::string_view(sv.data, sv.length);
}

minifi::core::annotation::Input toInputRequirement(MinifiInputRequirement req) {
  switch (req) {
    case MINIFI_INPUT_REQUIRED: return minifi::core::annotation::Input::INPUT_REQUIRED;
    case MINIFI_INPUT_ALLOWED: return minifi::core::annotation::Input::INPUT_ALLOWED;
    case MINIFI_INPUT_FORBIDDEN: return minifi::core::annotation::Input::INPUT_FORBIDDEN;
  }
  gsl_FailFast();
}

minifi::core::logging::LOG_LEVEL toLogLevel(MinifiLogLevel lvl) {
  switch (lvl) {
    case MINIFI_TRACE: return minifi::core::logging::trace;
    case MINIFI_DEBUG: return minifi::core::logging::debug;
    case MINIFI_INFO: return minifi::core::logging::info;
    case MINIFI_WARNING: return minifi::core::logging::warn;
    case MINIFI_ERROR: return minifi::core::logging::err;
    case MINIFI_CRITICAL: return minifi::core::logging::critical;
    case MINIFI_OFF: return minifi::core::logging::off;
  }
  gsl_FailFast();
}

minifi::state::response::SerializedResponseNode createSerializedResponseNode(const MinifiSerializedResponseNode* node) {
  gsl_Expects(node);
  std::vector<minifi::state::response::SerializedResponseNode> children;
  for (size_t i = 0; i < node->children_count; ++i) {
    children.push_back(createSerializedResponseNode(&node->children_ptr[i]));
  }
  return minifi::state::response::SerializedResponseNode{
    .name = toString(node->name),
    .value = {},
    .array = static_cast<bool>(node->array),
    .collapsible = static_cast<bool>(node->collapsible),
    .keep_empty = static_cast<bool>(node->keep_empty),
    .children = std::move(children)
  };
}

minifi::core::Property createProperty(const MinifiProperty* property_description) {
  gsl_Expects(property_description);
  std::vector<std::string_view> allowed_values;
  for (size_t i = 0; i < property_description->allowed_values_count; ++i) {
    allowed_values.push_back(toStringView(property_description->allowed_values_ptr[i]));
  }
  std::vector<std::string_view> allowed_types;
  for (size_t i = 0; i < property_description->types_count; ++i) {
    allowed_types.push_back(toStringView(property_description->types_ptr[i]));
  }
  std::vector<std::string_view> dependent_properties;
  for (size_t i = 0; i < property_description->dependent_properties_count; ++i) {
    dependent_properties.push_back(toStringView(property_description->dependent_properties_ptr[i]));
  }
  std::vector<std::pair<std::string_view, std::string_view>> exclusive_of_properties;
  for (size_t i = 0; i < property_description->exclusive_of_properties_count; ++i) {
    exclusive_of_properties.push_back({toStringView(property_description->exclusive_of_property_names_ptr[i]), toStringView(property_description->exclusive_of_property_values_ptr[i])});
  }
  std::optional<std::string_view> default_value;
  if (property_description->default_value) {
    default_value = toStringView(*property_description->default_value);
  }
  return minifi::core::Property{minifi::core::PropertyReference{
    toStringView(property_description->name),
    toStringView(property_description->display_name),
    toStringView(property_description->description),
    static_cast<bool>(property_description->is_required),
    static_cast<bool>(property_description->is_sensitive),
    std::span(allowed_values),
    std::span(allowed_types),
    std::span(dependent_properties),
    std::span(exclusive_of_properties),
    default_value,
    gsl::make_not_null((const minifi::core::PropertyValidator*)(void*)property_description->validator),
    static_cast<bool>(property_description->supports_expression_language)
  }};
}

class CProcessor;

class CProcessorMetricsWrapper : public minifi::core::ProcessorMetricsImpl {
 public:
  class CProcessorInfoProvider : public ProcessorMetricsImpl::ProcessorInfoProvider {
   public:
    CProcessorInfoProvider(const CProcessor& source_processor): source_processor_(source_processor) {}

    std::string getProcessorType() const override;
    std::string getName() const override;
    minifi::utils::SmallString<36> getUUIDStr() const override;

    ~CProcessorInfoProvider() override = default;

   private:
    const CProcessor& source_processor_;
  };

  explicit CProcessorMetricsWrapper(const CProcessor& source_processor)
      : minifi::core::ProcessorMetricsImpl(std::make_unique<CProcessorInfoProvider>(source_processor)),
        source_processor_(source_processor) {
  }

  std::vector<minifi::state::response::SerializedResponseNode> serialize() override;

  std::vector<minifi::state::PublishedMetric> calculateMetrics() override;

 private:
  const CProcessor& source_processor_;
};

class CProcessor : public minifi::core::ProcessorApi {
 public:
  CProcessor(MinifiProcessorCallbacks callbacks, minifi::core::ProcessorMetadata metadata)
      : callbacks_(callbacks),
        metrics_(std::make_shared<CProcessorMetricsWrapper>(*this)) {
    metadata_ = metadata;
    MinifiProcessorMetadata c_metadata;
    auto uuid_str = metadata.uuid.to_string();
    c_metadata.uuid = MinifiStringView{.data = uuid_str.data(), .length = gsl::narrow<uint32_t>(uuid_str.length())};
    c_metadata.name = MinifiStringView{.data = metadata.name.data(), .length = gsl::narrow<uint32_t>(metadata.name.length())};
    c_metadata.logger = (MinifiLogger)(void*)&metadata_.logger;
    impl_ = callbacks_.create(c_metadata);
  }
  ~CProcessor() override {
    callbacks_.destroy(impl_);
  }

  bool isWorkAvailable() override {
    return static_cast<bool>(callbacks_.isWorkAvailable(impl_));
  }

  void restore(const std::shared_ptr<minifi::core::FlowFile>& file) override {
    callbacks_.restore(impl_, (MinifiFlowFile)(void*)&file);
  }

  bool supportsDynamicProperties() const override {
    return static_cast<bool>(callbacks_.supportsDynamicProperties(impl_));
  }

  bool supportsDynamicRelationships() const override {
    return static_cast<bool>(callbacks_.supportsDynamicRelationships(impl_));
  }

  void initialize(minifi::core::ProcessorDescriptor& descriptor) override {
    callbacks_.initialize(impl_, (MinifiProcessorDescriptor)(void*)(&descriptor));
  }

  bool isSingleThreaded() const override {
    return static_cast<bool>(callbacks_.isSingleThreaded(impl_));
  }

  std::string getProcessorType() const override {
    std::optional<std::string> type;
    callbacks_.getProcessorType(impl_, (MinifiString)(void*)&type);
    gsl_Expects(type.has_value());
    return type.value();
  }

  bool getTriggerWhenEmpty() const override {
    return static_cast<bool>(callbacks_.getTriggerWhenEmpty(impl_));
  }

  void onTrigger(minifi::core::ProcessContext& process_context, minifi::core::ProcessSession& process_session) override {
    std::optional<std::string> error;
    callbacks_.onTrigger(impl_, (MinifiProcessContext)(void*)(&process_context), (MinifiProcessSession)(void*)(&process_session), (MinifiString)(void*)&error);
    if (error) {
      throw minifi::Exception(minifi::ExceptionType::PROCESSOR_EXCEPTION, error.value());
    }
  }

  void onSchedule(minifi::core::ProcessContext& process_context, minifi::core::ProcessSessionFactory& process_session_factory) override {
    std::optional<std::string> error;
    callbacks_.onSchedule(impl_, (MinifiProcessContext)(void*)(&process_context), (MinifiProcessSessionFactory)(void*)(&process_session_factory), (MinifiString)(void*)&error);
    if (error) {
      throw minifi::Exception(minifi::ExceptionType::PROCESS_SCHEDULE_EXCEPTION, error.value());
    }
  }

  void onUnSchedule() override {
    callbacks_.onUnSchedule(impl_);
  }

  void notifyStop() override {
    callbacks_.notifyStop(impl_);
  }

  minifi::core::annotation::Input getInputRequirement() const override {
    return toInputRequirement(callbacks_.getInputRequirement(impl_));
  }

  gsl::not_null<std::shared_ptr<minifi::core::ProcessorMetrics>> getMetrics() const override {
    return metrics_;
  }

  void forEachLogger(const std::function<void(std::shared_ptr<minifi::core::logging::Logger>)>& callback) override {
    callbacks_.forEachLogger(impl_, (MinifiLoggerCallback)(void*)(&callback));
  }

  std::string getName() const {
    return metadata_.name;
  }

  minifi::utils::Identifier getUUID() const {
    return metadata_.uuid;
  }

  void serializeMetrics(std::vector<minifi::state::response::SerializedResponseNode>& resp) const {
    callbacks_.serializeMetrics(impl_, (MinifiSerializedResponseNodeVec)(void*)&resp);
  }

  void calculateMetrics(std::vector<minifi::state::PublishedMetric>& resp) const {
    callbacks_.calculateMetrics(impl_, (MinifiPublishedMetricVec)(void*)&resp);
  }

 private:
  MinifiProcessorCallbacks callbacks_;
  void* impl_;
  minifi::core::ProcessorMetadata metadata_;
  gsl::not_null<std::shared_ptr<minifi::core::ProcessorMetrics>> metrics_;
};

class CProcessorFactory : public minifi::core::ProcessorFactory {
 public:
  CProcessorFactory(std::string group_name, std::string class_name, MinifiProcessorCallbacks callbacks)
    : group_name_(std::move(group_name)),
      class_name_(std::move(class_name)),
      callbacks_(callbacks) {}
  std::unique_ptr<minifi::core::ProcessorApi> create(minifi::core::ProcessorMetadata metadata) override {
    return std::make_unique<CProcessor>(callbacks_, metadata);
  }

  std::string getGroupName() const override {
    return group_name_;
  }

  std::string getClassName() const override {
    return class_name_;
  }

  ~CProcessorFactory() override = default;

 private:
  std::string group_name_;
  std::string class_name_;
  MinifiProcessorCallbacks callbacks_;
};

std::string CProcessorMetricsWrapper::CProcessorInfoProvider::getProcessorType() const {
  return source_processor_.getProcessorType();
}
std::string CProcessorMetricsWrapper::CProcessorInfoProvider::getName() const {
  return source_processor_.getName();
}
minifi::utils::SmallString<36> CProcessorMetricsWrapper::CProcessorInfoProvider::getUUIDStr() const {
  return source_processor_.getUUID().to_string();
}

std::vector<minifi::state::response::SerializedResponseNode> CProcessorMetricsWrapper::serialize() {
  auto nodes = ProcessorMetricsImpl::serialize();
  source_processor_.serializeMetrics(nodes);
  return nodes;
}

std::vector<minifi::state::PublishedMetric> CProcessorMetricsWrapper::calculateMetrics() {
  auto nodes = ProcessorMetricsImpl::calculateMetrics();
  source_processor_.calculateMetrics(nodes);
  return nodes;
}

class CExtension : public minifi::core::extension::Extension {
 public:
  CExtension(std::string name, MinifiBool(*initializer)(void*, MinifiConfigure), void* user_data): name_(std::move(name)), initializer_(initializer), user_data_(user_data) {}
  bool initialize(const minifi::core::extension::ExtensionConfig& config) override {
    gsl_Assert(initializer_);
    return static_cast<bool>(initializer_(user_data_, (MinifiConfigure)(void*)config.get()));
  }

  const std::string& getName() const override {
    return name_;
  }

 private:
  std::string name_;
  MinifiBool(*initializer_)(void*, MinifiConfigure);
  void* user_data_;
};

}  // namespace

extern "C" {

MinifiExtension* MinifiCreateExtension(const MinifiExtensionCreateInfo* extension_create_info) {
  gsl_Assert(extension_create_info);
  return reinterpret_cast<MinifiExtension*>(new org::apache::nifi::minifi::core::extension::Extension::Info{
    .name = toString(extension_create_info->name),
    .version = toString(extension_create_info->version),
    .deinit = extension_create_info->deinit,
    .user_data = extension_create_info->user_data
  });
}

MinifiPropertyValidator MinifiGetStandardValidator(MinifiStandardPropertyValidator validator) {
  switch (validator) {
    case MINIFI_ALWAYS_VALID_VALIDATOR: return (MinifiPropertyValidator)(void*)(const minifi::core::PropertyValidator*)&minifi::core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR;
    case MINIFI_NON_BLANK_VALIDATOR: return (MinifiPropertyValidator)(void*)(const minifi::core::PropertyValidator*)&minifi::core::StandardPropertyValidators::NON_BLANK_VALIDATOR;
    case MINIFI_TIME_PERIOD_VALIDATOR: return (MinifiPropertyValidator)(void*)(const minifi::core::PropertyValidator*)&minifi::core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR;
    case MINIFI_BOOLEAN_VALIDATOR: return (MinifiPropertyValidator)(void*)(const minifi::core::PropertyValidator*)&minifi::core::StandardPropertyValidators::BOOLEAN_VALIDATOR;
    case MINIFI_INTEGER_VALIDATOR: return (MinifiPropertyValidator)(void*)(const minifi::core::PropertyValidator*)&minifi::core::StandardPropertyValidators::INTEGER_VALIDATOR;
    case MINIFI_UNSIGNED_INTEGER_VALIDATOR: return (MinifiPropertyValidator)(void*)(const minifi::core::PropertyValidator*)&minifi::core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR;
    case MINIFI_DATA_SIZE_VALIDATOR: return (MinifiPropertyValidator)(void*)(const minifi::core::PropertyValidator*)&minifi::core::StandardPropertyValidators::DATA_SIZE_VALIDATOR;
    case MINIFI_PORT_VALIDATOR: return (MinifiPropertyValidator)(void*)(const minifi::core::PropertyValidator*)&minifi::core::StandardPropertyValidators::PORT_VALIDATOR;
    default: gsl_FailFast();
  }
}

MinifiPropertyValidator MinifiCreatePropertyValidator(const MinifiPropertyValidatorCreateInfo*) {
  gsl_FailFast();
}

void MinifiSerializedResponseNodeVecPush(MinifiSerializedResponseNodeVec nodes, const MinifiSerializedResponseNode* node) {
  ((std::vector<minifi::state::response::SerializedResponseNode>*)(void*)(nodes))->push_back(createSerializedResponseNode(node));
}

void MinifiPublishedMetricVecPush(MinifiPublishedMetricVec nodes, const MinifiPublishedMetric* node) {
  gsl_Assert(nodes != MINIFI_NULL);
  gsl_Assert(node);
  minifi::state::PublishedMetric metric{
    .name = toString(node->name),
    .value = node->value
  };
  for (size_t i = 0; i < node->labels_count; ++i) {
    metric.labels[toString(node->label_keys[i])] = toString(node->label_values[i]);
  }
  ((std::vector<minifi::state::PublishedMetric>*)(void*)(nodes))->push_back(std::move(metric));
}

void MinifiRegisterProcessorClass(const MinifiProcessorClassDescription* class_description) {
  gsl_Expects(class_description);
  std::vector<minifi::core::Property> properties;
  for (size_t i = 0; i < class_description->class_properties_count; ++i) {
    properties.push_back(createProperty(&class_description->class_properties_ptr[i]));
  }
  std::vector<minifi::core::DynamicProperty> dynamic_properties;
  for (size_t i = 0; i < class_description->dynamic_properties_count; ++i) {
    dynamic_properties.push_back(minifi::core::DynamicProperty{
      .name = toStringView(class_description->dynamic_properties_ptr[i].name),
      .value = toStringView(class_description->dynamic_properties_ptr[i].value),
      .description = toStringView(class_description->dynamic_properties_ptr[i].description),
      .supports_expression_language = static_cast<bool>(class_description->dynamic_properties_ptr[i].supports_expression_language)
    });
  }
  std::vector<minifi::core::Relationship> relationships;
  for (size_t i = 0; i < class_description->class_relationships_count; ++i) {
    relationships.push_back(minifi::core::Relationship{
      toString(class_description->class_relationships_ptr[i].name),
      toString(class_description->class_relationships_ptr[i].description)
    });
  }
  std::vector<std::vector<minifi::core::RelationshipDefinition>> output_attribute_relationships;
  std::vector<minifi::core::OutputAttributeReference> output_attributes;
  for (size_t i = 0; i < class_description->output_attributes_count; ++i) {
    minifi::core::OutputAttributeReference ref{minifi::core::OutputAttributeDefinition{"", {}, ""}};
    ref.name = toString(class_description->output_attributes_ptr[i].name);
    ref.description = toStringView(class_description->output_attributes_ptr[i].description);
    output_attribute_relationships.push_back({});
    for (size_t j = 0; j < class_description->output_attributes_ptr[i].relationships_count; ++j) {
      output_attribute_relationships.back().push_back(minifi::core::RelationshipDefinition{
        .name = toStringView(class_description->output_attributes_ptr[i].relationships_ptr[j].name),
        .description = toStringView(class_description->output_attributes_ptr[i].relationships_ptr[j].description)
      });
    }
    ref.relationships = std::span(output_attribute_relationships.back());
    output_attributes.push_back(ref);
  }
  auto module_name = toString(class_description->module_name);
  minifi::AgentDocs::getMutableClassDescriptions()[module_name].processors_.push_back(minifi::ClassDescription{
    .type_ = minifi::ResourceType::Processor,
    .short_name_ = toString(class_description->short_name),
    .full_name_ = toString(class_description->full_name),
    .description_ = toString(class_description->description),
    .class_properties_ = properties,
    .dynamic_properties_ = dynamic_properties,
    .class_relationships_ = relationships,
    .output_attributes_ = output_attributes,
    .supports_dynamic_properties_ = static_cast<bool>(class_description->supports_dynamic_properties),
    .supports_dynamic_relationships_ = static_cast<bool>(class_description->supports_dynamic_relationships),
    .inputRequirement_ = minifi::core::annotation::toString(toInputRequirement(class_description->input_requirement)),
    .isSingleThreaded_ = static_cast<bool>(class_description->is_single_threaded),
  });

  minifi::core::ClassLoader::getDefaultClassLoader().getClassLoader(module_name).registerClass(
    toString(class_description->short_name),
    std::make_unique<CProcessorFactory>(module_name, toString(class_description->internal_name), class_description->callbacks)
  );
}

void MinifiMinifiLoggerCallbackCall(MinifiLoggerCallback cb, MinifiLogger logger) {
  gsl_Assert(logger != MINIFI_NULL);
  (*((std::function<void(std::shared_ptr<minifi::core::logging::Logger>)>*)(void*)cb))(*(std::shared_ptr<minifi::core::logging::Logger>*)(void*)logger);
}

void MinifiProcessorDescriptorSetSupportedRelationships(MinifiProcessorDescriptor descriptor, uint32_t relationships_count, const MinifiRelationship* relationships_ptr) {
  gsl_Assert(descriptor != MINIFI_NULL);
  std::vector<minifi::core::RelationshipDefinition> relationships;
  for (size_t i = 0; i < relationships_count; ++i) {
    relationships.push_back(minifi::core::RelationshipDefinition{
      .name = toString(relationships_ptr[i].name),
      .description = toString(relationships_ptr[i].description),
    });
  }
  ((minifi::core::ProcessorDescriptor*)(void*)descriptor)->setSupportedRelationships(relationships);
}

void MinifiProcessorDescriptorSetSupportedProperties(MinifiProcessorDescriptor descriptor, uint32_t properties_count, const MinifiProperty* properties_ptr) {
  gsl_Assert(descriptor != MINIFI_NULL);
  std::vector<minifi::core::Property> properties;
  for (size_t i = 0; i < properties_count; ++i) {
    properties.push_back(createProperty(&properties_ptr[i]));
  }
  std::vector<minifi::core::PropertyReference> property_references;
  for (size_t i = 0; i < properties_count; ++i) {
    property_references.push_back(properties[i].getReference());
  }
  ((minifi::core::ProcessorDescriptor*)(void*)descriptor)->setSupportedProperties(property_references);
}

MinifiStatus MinifiProcessContextGetProperty(MinifiProcessContext context, MinifiStringView property_name, MinifiFlowFile flow_file, void(*result_cb)(void* data, MinifiStringView result), void* data) {
  gsl_Assert(context != MINIFI_NULL);
  auto result = ((minifi::core::ProcessContext*)(void*)context)->getProperty(toStringView(property_name), flow_file != MINIFI_NULL ? ((std::shared_ptr<minifi::core::FlowFile>*)(void*)flow_file)->get() : nullptr);
  if (result) {
    result_cb(data, MinifiStringView{
      .data = result.value().data(),
      .length = gsl::narrow<uint32_t>(result.value().length())
    });
    return MINIFI_SUCCESS;
  }
  switch (static_cast<minifi::core::PropertyErrorCode>(result.error().value())) {
    case minifi::core::PropertyErrorCode::NotSupportedProperty: return MINIFI_NOT_SUPPORTED_PROPERTY;
    case minifi::core::PropertyErrorCode::DynamicPropertiesNotSupported: return MINIFI_DYNAMIC_PROPERTIES_NOT_SUPPORTED;
    case minifi::core::PropertyErrorCode::PropertyNotSet: return MINIFI_PROPERTY_NOT_SET;
    case minifi::core::PropertyErrorCode::ValidationFailed: return MINIFI_VALIDATION_FAILED;
    default: return MINIFI_UNKNOWN_ERROR;
  }
}

OWNED MinifiExtension MinifiCreateExtension(const MinifiExtensionCreateInfo* extension_create_info) {
  gsl_Assert(extension_create_info);
  auto* extension = new CExtension(toString(extension_create_info->name), extension_create_info->initialize, extension_create_info->user_data);
  minifi::core::extension::ExtensionManager::get().registerExtension(*extension);
  return (OWNED MinifiExtension)(void*)extension;
}
void MinifiDestroyExtension(OWNED MinifiExtension extension) {
  gsl_Assert(extension != MINIFI_NULL);
  auto* extension_impl = (CExtension*)(void*)extension;
  minifi::core::extension::ExtensionManager::get().unregisterExtension(*extension_impl);
  delete extension_impl;
}

void MinifiProcessContextYield(MinifiProcessContext context) {
  gsl_Assert(context != MINIFI_NULL);
  ((minifi::core::ProcessContext*)(void*)context)->yield();
}

void MinifiProcessContextGetProcessorName(MinifiProcessContext context, void(*cb)(void* data, MinifiStringView result), void* data) {
  gsl_Assert(context != MINIFI_NULL);
  auto name = ((minifi::core::ProcessContext*)(void*)context)->getProcessorInfo().getName();
  cb(data, MinifiStringView{.data = name.data(), .length = gsl::narrow<uint32_t>(name.length())});
}

MinifiBool MinifiProcessContextHasNonEmptyProperty(MinifiProcessContext context, MinifiStringView property_name) {
  gsl_Assert(context != MINIFI_NULL);
  return ((minifi::core::ProcessContext*)(void*)context)->hasNonEmptyProperty(toString(property_name)) ? MINIFI_TRUE : MINIFI_FALSE;
}

void MinifiStringAssign(MinifiString str, MinifiStringView sv) {
  (*(std::optional<std::string>*)(void*)str) = toString(sv);
}

void MinifiConfigureGet(MinifiConfigure configure, MinifiStringView key, void(*cb)(void*, MinifiStringView), void* data) {
  gsl_Assert(configure != MINIFI_NULL);
  auto value = ((minifi::Configure*)(void*)configure)->get(toString(key));
  if (value) {
    cb(data, MinifiStringView{
      .data = value->data(),
      .length = gsl::narrow<uint32_t>(value->length())
    });
  }
}

void MinifiLoggerSetMaxLogSize(MinifiLogger logger, int32_t max_size) {
  gsl_Assert(logger != MINIFI_NULL);
  (*((std::shared_ptr<minifi::core::logging::Logger>*)(void*)logger))->set_max_log_size(max_size);
}

void MinifiLoggerGetId(MinifiLogger logger, void(*cb)(void*, MinifiStringView), void* data) {
  gsl_Assert(logger != MINIFI_NULL);
  auto id = (*((std::shared_ptr<minifi::core::logging::Logger>*)(void*)logger))->get_id();
  if (id) {
    cb(data, MinifiStringView{
      .data = id->data(),
      .length = gsl::narrow<uint32_t>(id->length())
    });
  }
}

void MinifiLoggerLogString(MinifiLogger logger, MinifiLogLevel level, MinifiStringView msg) {
  gsl_Assert(logger != MINIFI_NULL);
  (*((std::shared_ptr<minifi::core::logging::Logger>*)(void*)logger))->log_string(toLogLevel(level), toString(msg));
}

MinifiBool MinifiLoggerShouldLog(MinifiLogger logger, MinifiLogLevel level) {
  gsl_Assert(logger != MINIFI_NULL);
  return (*((std::shared_ptr<minifi::core::logging::Logger>*)(void*)logger))->should_log(toLogLevel(level));
}

MinifiLogLevel MinifiLoggerLevel(MinifiLogger logger) {
  gsl_Assert(logger != MINIFI_NULL);
  switch ((*((std::shared_ptr<minifi::core::logging::Logger>*)(void*)logger))->level()) {
    case minifi::core::logging::LOG_LEVEL::trace: return MINIFI_TRACE;
    case minifi::core::logging::LOG_LEVEL::debug: return MINIFI_DEBUG;
    case minifi::core::logging::LOG_LEVEL::info: return MINIFI_INFO;
    case minifi::core::logging::LOG_LEVEL::warn: return MINIFI_WARNING;
    case minifi::core::logging::LOG_LEVEL::err: return MINIFI_ERROR;
    case minifi::core::logging::LOG_LEVEL::critical: return MINIFI_CRITICAL;
    case minifi::core::logging::LOG_LEVEL::off: return MINIFI_OFF;
  }
  gsl_FailFast();
}

int32_t MinifiLoggerGetMaxLogSize(MinifiLogger logger) {
  gsl_Assert(logger != MINIFI_NULL);
  return (*((std::shared_ptr<minifi::core::logging::Logger>*)(void*)logger))->getMaxLogSize();
}

OWNED MinifiFlowFile MinifiProcessSessionGet(MinifiProcessSession session) {
  gsl_Assert(session != MINIFI_NULL);
  auto ff = ((minifi::core::ProcessSession*)(void*)session)->get();
  if (ff) {
    return (MinifiFlowFile)(void*)(new std::shared_ptr<minifi::core::FlowFile>(ff));
  }
  return MINIFI_NULL;
}

OWNED MinifiFlowFile MinifiProcessSessionCreate(MinifiProcessSession session, MinifiFlowFile parent) {
  gsl_Assert(session != MINIFI_NULL);
  auto ff = ((minifi::core::ProcessSession*)(void*)session)->create(parent != MINIFI_NULL ? ((std::shared_ptr<minifi::core::FlowFile>*)(void*)parent)->get() : nullptr);
  if (ff) {
    return (MinifiFlowFile)(void*)(new std::shared_ptr<minifi::core::FlowFile>(ff));
  }
  return MINIFI_NULL;
}

void MinifiDestroyFlowFile(OWNED MinifiFlowFile ff) {
  gsl_Assert(ff != MINIFI_NULL);
  delete ((std::shared_ptr<minifi::core::FlowFile>*)(void*)ff);
}

OWNED MinifiFlowFile MinifiCopyFlowFile(MinifiFlowFile ff) {
  gsl_Assert(ff != MINIFI_NULL);
  return (MinifiFlowFile)(void*)new std::shared_ptr<minifi::core::FlowFile>(*(std::shared_ptr<minifi::core::FlowFile>*)(void*)ff);
}

void MinifiProcessSessionTransfer(MinifiProcessSession session, MinifiFlowFile ff, MinifiStringView rel) {
  gsl_Assert(ff != MINIFI_NULL);
  ((minifi::core::ProcessSession*)(void*)session)->transfer(*(std::shared_ptr<minifi::core::FlowFile>*)(void*)ff, minifi::core::Relationship{toString(rel), ""});
}

void MinifiProcessSessionRead(MinifiProcessSession session, MinifiFlowFile ff, int64_t(*cb)(void* data, MinifiInputStream), void* data) {
  gsl_Assert(session != MINIFI_NULL);
  gsl_Assert(ff != MINIFI_NULL);
  ((minifi::core::ProcessSession*)(void*)session)->read(*(std::shared_ptr<minifi::core::FlowFile>*)(void*)ff, [&] (auto& input_stream) -> int64_t {
    return cb(data, (MinifiInputStream)(void*)input_stream.get());
  });
}

void MinifiProcessSessionWrite(MinifiProcessSession session, MinifiFlowFile ff, int64_t(*cb)(void* data, MinifiOutputStream), void* data) {
  gsl_Assert(session != MINIFI_NULL);
  gsl_Assert(ff != MINIFI_NULL);
  ((minifi::core::ProcessSession*)(void*)session)->write(*(std::shared_ptr<minifi::core::FlowFile>*)(void*)ff, [&] (auto& output_stream) -> int64_t {
    return cb(data, (MinifiOutputStream)(void*)output_stream.get());
  });
}

uint64_t MinifiInputStreamSize(MinifiInputStream stream) {
  gsl_Assert(stream != MINIFI_NULL);
  return gsl::narrow<uint64_t>(((minifi::io::InputStream*)(void*)stream)->size());
}

int64_t MinifiInputStreamRead(MinifiInputStream stream, char* data, uint64_t size) {
  gsl_Assert(stream != MINIFI_NULL);
  return gsl::narrow<uint64_t>(((minifi::io::InputStream*)(void*)stream)->read(std::span(reinterpret_cast<std::byte*>(data), size)));
}

int64_t MinifiOutputStreamWrite(MinifiOutputStream stream, const char* data, uint64_t size) {
  gsl_Assert(stream != MINIFI_NULL);
  return gsl::narrow<uint64_t>(((minifi::io::OutputStream*)(void*)stream)->write(std::span(reinterpret_cast<const std::byte*>(data), size)));
}

void MinifiStatusToString(MinifiStatus status, void(*cb)(void* data, MinifiStringView str), void* data) {
  std::string message = [&] () -> std::string {
    switch (status) {
      case MINIFI_SUCCESS: return "Success";
      case MINIFI_UNKNOWN_ERROR: return "Unknown error";
      case MINIFI_NOT_SUPPORTED_PROPERTY: return minifi::core::PropertyErrorCategory{}.message(static_cast<int>(minifi::core::PropertyErrorCode::NotSupportedProperty));
      case MINIFI_DYNAMIC_PROPERTIES_NOT_SUPPORTED: return minifi::core::PropertyErrorCategory{}.message(static_cast<int>(minifi::core::PropertyErrorCode::DynamicPropertiesNotSupported));
      case MINIFI_PROPERTY_NOT_SET: return minifi::core::PropertyErrorCategory{}.message(static_cast<int>(minifi::core::PropertyErrorCode::PropertyNotSet));
      case MINIFI_VALIDATION_FAILED: return minifi::core::PropertyErrorCategory{}.message(static_cast<int>(minifi::core::PropertyErrorCode::ValidationFailed));
      default: return "Unknown error";
    }
  }();
  cb(data, MinifiStringView{.data = message.data(), .length = gsl::narrow<uint32_t>(message.size())});
}

} // extern "C"
