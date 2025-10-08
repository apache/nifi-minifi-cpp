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
#include "minifi-cpp/Exception.h"
#include "minifi-cpp/core/extension/ExtensionManager.h"
#include "utils/PropertyErrors.h"
#include "minifi-cpp/agent/build_description.h"
#include "utils/CProcessor.h"

namespace minifi = org::apache::nifi::minifi;

namespace {

std::string toString(MinifiStringView sv) {
  return {sv.data, sv.length};
}

std::string_view toStringView(MinifiStringView sv) {
  return {sv.data, sv.length};
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

minifi::core::Property createProperty(const MinifiProperty* property_description) {
  gsl_Expects(property_description);
  std::vector<std::string_view> allowed_values;
  allowed_values.reserve(property_description->allowed_values_count);
  for (size_t i = 0; i < property_description->allowed_values_count; ++i) {
    allowed_values.push_back(toStringView(property_description->allowed_values_ptr[i]));
  }
  std::vector<std::string_view> allowed_types;
  allowed_types.reserve(property_description->types_count);
  for (size_t i = 0; i < property_description->types_count; ++i) {
    allowed_types.push_back(toStringView(property_description->types_ptr[i]));
  }
  std::vector<std::string_view> dependent_properties;
  dependent_properties.reserve(property_description->dependent_properties_count);
  for (size_t i = 0; i < property_description->dependent_properties_count; ++i) {
    dependent_properties.push_back(toStringView(property_description->dependent_properties_ptr[i]));
  }
  std::vector<std::pair<std::string_view, std::string_view>> exclusive_of_properties;
  exclusive_of_properties.reserve(property_description->exclusive_of_properties_count);
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
    gsl::make_not_null(reinterpret_cast<const minifi::core::PropertyValidator*>(property_description->validator)),
    static_cast<bool>(property_description->supports_expression_language)
  }};
}

class CProcessorFactory : public minifi::core::ProcessorFactory {
 public:
  CProcessorFactory(std::string group_name, std::string class_name, minifi::utils::CProcessorClassDescription class_description)
    : group_name_(std::move(group_name)),
      class_name_(std::move(class_name)),
      class_description_(std::move(class_description)) {}
  std::unique_ptr<minifi::core::ProcessorApi> create(minifi::core::ProcessorMetadata metadata) override {
    return std::make_unique<minifi::utils::CProcessor>(class_description_, metadata);
  }

  [[nodiscard]] std::string getGroupName() const override {
    return group_name_;
  }

  [[nodiscard]] std::string getClassName() const override {
    return class_name_;
  }

  CProcessorFactory() = delete;
  CProcessorFactory(const CProcessorFactory&) = delete;
  CProcessorFactory& operator=(const CProcessorFactory&) = delete;
  CProcessorFactory(CProcessorFactory&&) = delete;
  CProcessorFactory& operator=(CProcessorFactory&&) = delete;
  ~CProcessorFactory() override = default;

 private:
  std::string group_name_;
  std::string class_name_;
  minifi::utils::CProcessorClassDescription class_description_;
};

class CExtension : public minifi::core::extension::Extension {
 public:
  CExtension(std::string name, MinifiBool(*initializer)(void*, MinifiConfigure), void* user_data): name_(std::move(name)), initializer_(initializer), user_data_(user_data) {}
  bool initialize(const minifi::core::extension::ExtensionConfig& config) override {
    gsl_Assert(initializer_);
    return static_cast<bool>(initializer_(user_data_, reinterpret_cast<MinifiConfigure>(config.get())));
  }

  [[nodiscard]] const std::string& getName() const override {
    return name_;
  }

 private:
  std::string name_;
  MinifiBool(*initializer_)(void*, MinifiConfigure);
  void* user_data_;
};

}  // namespace

namespace org::apache::nifi::minifi::utils {

MinifiExtension* MinifiCreateExtension(const MinifiExtensionCreateInfo* extension_create_info) {
  gsl_Assert(extension_create_info);
  return reinterpret_cast<MinifiExtension*>(new org::apache::nifi::minifi::core::extension::Extension::Info{
    .name = toString(extension_create_info->name),
    .version = toString(extension_create_info->version),
    .deinit = extension_create_info->deinit,
    .user_data = extension_create_info->user_data
  });
}

void useCProcessorClassDescription(const MinifiProcessorClassDescription* class_description, const std::function<void(minifi::ClassDescription, minifi::utils::CProcessorClassDescription)>& fn) {
  std::vector<minifi::core::Property> properties;
  properties.reserve(class_description->class_properties_count);
  for (size_t i = 0; i < class_description->class_properties_count; ++i) {
    properties.push_back(createProperty(&class_description->class_properties_ptr[i]));
  }
  std::vector<minifi::core::DynamicProperty> dynamic_properties;
  dynamic_properties.reserve(class_description->dynamic_properties_count);
  for (size_t i = 0; i < class_description->dynamic_properties_count; ++i) {
    dynamic_properties.push_back(minifi::core::DynamicProperty{
      .name = toStringView(class_description->dynamic_properties_ptr[i].name),
      .value = toStringView(class_description->dynamic_properties_ptr[i].value),
      .description = toStringView(class_description->dynamic_properties_ptr[i].description),
      .supports_expression_language = static_cast<bool>(class_description->dynamic_properties_ptr[i].supports_expression_language)
    });
  }
  std::vector<minifi::core::Relationship> relationships;
  relationships.reserve(class_description->class_relationships_count);
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
    ref.name = toStringView(class_description->output_attributes_ptr[i].name);
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

  auto name_segments = minifi::utils::string::split(toStringView(class_description->full_name), "::");
  gsl_Assert(!name_segments.empty());

  minifi::ClassDescription description{
    .type_ = minifi::ResourceType::Processor,
    .short_name_ = name_segments.back(),
    .full_name_ = minifi::utils::string::join(".", name_segments),
    .description_ = toString(class_description->description),
    .class_properties_ = properties,
    .dynamic_properties_ = dynamic_properties,
    .class_relationships_ = relationships,
    .output_attributes_ = output_attributes,
    .supports_dynamic_properties_ = static_cast<bool>(class_description->supports_dynamic_properties),
    .supports_dynamic_relationships_ = static_cast<bool>(class_description->supports_dynamic_relationships),
    .inputRequirement_ = minifi::core::annotation::toString(toInputRequirement(class_description->input_requirement)),
    .isSingleThreaded_ = static_cast<bool>(class_description->is_single_threaded),
  };

  minifi::utils::CProcessorClassDescription c_class_description{
    .name = name_segments.back(),
    .class_properties = properties,
    .class_relationships = relationships,
    .supports_dynamic_properties = description.supports_dynamic_properties_,
    .supports_dynamic_relationships = description.supports_dynamic_relationships_,
    .input_requirement = toInputRequirement(class_description->input_requirement),
    .is_single_threaded = description.isSingleThreaded_,

    .callbacks = class_description->callbacks
  };

  fn(description, c_class_description);
}

}  // namespace org::apache::nifi::minifi::utils

extern "C" {

MinifiPropertyValidator MinifiGetStandardValidator(MinifiStandardPropertyValidator validator) {
  switch (validator) {
    case MINIFI_ALWAYS_VALID_VALIDATOR: return reinterpret_cast<MinifiPropertyValidator>(&minifi::core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR);
    case MINIFI_NON_BLANK_VALIDATOR: return reinterpret_cast<MinifiPropertyValidator>(&minifi::core::StandardPropertyValidators::NON_BLANK_VALIDATOR);
    case MINIFI_TIME_PERIOD_VALIDATOR: return reinterpret_cast<MinifiPropertyValidator>(&minifi::core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR);
    case MINIFI_BOOLEAN_VALIDATOR: return reinterpret_cast<MinifiPropertyValidator>(&minifi::core::StandardPropertyValidators::BOOLEAN_VALIDATOR);
    case MINIFI_INTEGER_VALIDATOR: return reinterpret_cast<MinifiPropertyValidator>(&minifi::core::StandardPropertyValidators::INTEGER_VALIDATOR);
    case MINIFI_UNSIGNED_INTEGER_VALIDATOR: return reinterpret_cast<MinifiPropertyValidator>(&minifi::core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR);
    case MINIFI_DATA_SIZE_VALIDATOR: return reinterpret_cast<MinifiPropertyValidator>(&minifi::core::StandardPropertyValidators::DATA_SIZE_VALIDATOR);
    case MINIFI_PORT_VALIDATOR: return reinterpret_cast<MinifiPropertyValidator>(&minifi::core::StandardPropertyValidators::PORT_VALIDATOR);
    default: gsl_FailFast();
  }
}

void MinifiRegisterProcessorClass(const MinifiProcessorClassDescription* class_description) {
  gsl_Expects(class_description);

  auto module_name = toString(class_description->module_name);
  minifi::BundleDetails bundle{
    .artifact = module_name,
    .group = module_name,
    .version = "1.0.0"
  };

  minifi::utils::useCProcessorClassDescription(class_description, [&] (const auto& description, const auto& c_class_description) {
    minifi::ExternalBuildDescription::addExternalComponent(bundle, description);

    minifi::core::ClassLoader::getDefaultClassLoader().getClassLoader(module_name).registerClass(
      c_class_description.name,
      std::make_unique<CProcessorFactory>(module_name, toString(class_description->full_name), c_class_description));
  });
}

MinifiStatus MinifiProcessContextGetProperty(MinifiProcessContext context, MinifiStringView property_name, MinifiFlowFile flow_file,
    void (*result_cb)(void* user_ctx, MinifiStringView result), void* user_ctx) {
  gsl_Assert(context != MINIFI_NULL);
  auto result = reinterpret_cast<minifi::core::ProcessContext*>(context)->getProperty(toStringView(property_name),
      flow_file != MINIFI_NULL ? reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(flow_file)->get() : nullptr);
  if (result) {
    result_cb(user_ctx, MinifiStringView{.data = result.value().data(), .length = gsl::narrow<uint32_t>(result.value().length())});
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
  return reinterpret_cast<OWNED MinifiExtension>(extension);
}
void MinifiDestroyExtension(OWNED gsl::owner<MinifiExtension> extension) {
  gsl_Assert(extension != MINIFI_NULL);
  auto extension_impl = reinterpret_cast<gsl::owner<CExtension*>>(extension);
  minifi::core::extension::ExtensionManager::get().unregisterExtension(*extension_impl);
  delete extension_impl;
}

void MinifiProcessContextYield(MinifiProcessContext context) {
  gsl_Assert(context != MINIFI_NULL);
  reinterpret_cast<minifi::core::ProcessContext*>(context)->yield();
}

void MinifiProcessContextGetProcessorName(MinifiProcessContext context, void(*cb)(void* user_ctx, MinifiStringView result), void* user_ctx) {
  gsl_Assert(context != MINIFI_NULL);
  auto name = reinterpret_cast<minifi::core::ProcessContext*>(context)->getProcessorInfo().getName();
  cb(user_ctx, MinifiStringView{.data = name.data(), .length = gsl::narrow<uint32_t>(name.length())});
}

MinifiBool MinifiProcessContextHasNonEmptyProperty(MinifiProcessContext context, MinifiStringView property_name) {
  gsl_Assert(context != MINIFI_NULL);
  return reinterpret_cast<minifi::core::ProcessContext*>(context)->hasNonEmptyProperty(toString(property_name)) ? MINIFI_TRUE : MINIFI_FALSE;
}

void MinifiConfigureGet(MinifiConfigure configure, MinifiStringView key, void(*cb)(void* user_ctx, MinifiStringView result), void* user_ctx) {
  gsl_Assert(configure != MINIFI_NULL);
  auto value = reinterpret_cast<minifi::Configure*>(configure)->get(toString(key));
  if (value) {
    cb(user_ctx, MinifiStringView{
      .data = value->data(),
      .length = gsl::narrow<uint32_t>(value->length())
    });
  }
}

void MinifiLoggerSetMaxLogSize(MinifiLogger logger, int32_t max_size) {
  gsl_Assert(logger != MINIFI_NULL);
  (*reinterpret_cast<std::shared_ptr<minifi::core::logging::Logger>*>(logger))->set_max_log_size(max_size);
}

void MinifiLoggerGetId(MinifiLogger logger, void(*cb)(void* user_ctx, MinifiStringView id), void* user_ctx) {
  gsl_Assert(logger != MINIFI_NULL);
  auto id = (*reinterpret_cast<std::shared_ptr<minifi::core::logging::Logger>*>(logger))->get_id();
  if (id) {
    cb(user_ctx, MinifiStringView{
      .data = id->data(),
      .length = gsl::narrow<uint32_t>(id->length())
    });
  }
}

void MinifiLoggerLogString(MinifiLogger logger, MinifiLogLevel level, MinifiStringView msg) {
  gsl_Assert(logger != MINIFI_NULL);
  (*reinterpret_cast<std::shared_ptr<minifi::core::logging::Logger>*>(logger))->log_string(toLogLevel(level), toString(msg));
}

MinifiBool MinifiLoggerShouldLog(MinifiLogger logger, MinifiLogLevel level) {
  gsl_Assert(logger != MINIFI_NULL);
  return (*reinterpret_cast<std::shared_ptr<minifi::core::logging::Logger>*>(logger))->should_log(toLogLevel(level));
}

MinifiLogLevel MinifiLoggerLevel(MinifiLogger logger) {
  gsl_Assert(logger != MINIFI_NULL);
  switch ((*reinterpret_cast<std::shared_ptr<minifi::core::logging::Logger>*>(logger))->level()) {
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

OWNED gsl::owner<MinifiPublishedMetrics> MinifiPublishedMetricsCreate(const uint32_t count, const MinifiStringView* names, const double* values) {
  const gsl::owner<std::vector<minifi::state::PublishedMetric>*> metrics = new std::vector<minifi::state::PublishedMetric>();  // NOLINT(modernize-use-auto)
  metrics->reserve(count);
  for (uint32_t i = 0; i < count; i++) {
    metrics->emplace_back(minifi::state::PublishedMetric{toString(names[i]), values[i], {}});
  }
  return reinterpret_cast<MinifiPublishedMetrics>(metrics);
}

int32_t MinifiLoggerGetMaxLogSize(MinifiLogger logger) {
  gsl_Assert(logger != MINIFI_NULL);
  return (*reinterpret_cast<std::shared_ptr<minifi::core::logging::Logger>*>(logger))->getMaxLogSize();
}

OWNED MinifiFlowFile MinifiProcessSessionGet(MinifiProcessSession session) {
  gsl_Assert(session != MINIFI_NULL);
  if (const auto ff = reinterpret_cast<minifi::core::ProcessSession*>(session)->get()) {
    return reinterpret_cast<MinifiFlowFile>(new std::shared_ptr<minifi::core::FlowFile>(ff));
  }
  return MINIFI_NULL;
}

OWNED MinifiFlowFile MinifiProcessSessionCreate(MinifiProcessSession session, MinifiFlowFile parent) {
  gsl_Assert(session != MINIFI_NULL);
  if (const auto ff = reinterpret_cast<minifi::core::ProcessSession*>(session)->create(parent != MINIFI_NULL
              ? reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(parent)->get()
              : nullptr)) {
    return reinterpret_cast<MinifiFlowFile>(new std::shared_ptr<minifi::core::FlowFile>(ff));
  }
  return MINIFI_NULL;
}

void MinifiDestroyFlowFile(OWNED gsl::owner<MinifiFlowFile> ff) {
  gsl_Assert(ff != MINIFI_NULL);
  delete reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(ff);
}

void MinifiProcessSessionTransfer(MinifiProcessSession session, MinifiFlowFile ff, MinifiStringView rel) {
  gsl_Assert(ff != MINIFI_NULL);
  reinterpret_cast<minifi::core::ProcessSession*>(session)->transfer(*reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(ff), minifi::core::Relationship{toString(rel), ""});
}

MinifiStatus MinifiProcessSessionRead(MinifiProcessSession session, MinifiFlowFile ff, int64_t(*cb)(void* user_ctx, MinifiInputStream), void* user_ctx) {
  gsl_Assert(session != MINIFI_NULL);
  gsl_Assert(ff != MINIFI_NULL);
  try {
    reinterpret_cast<minifi::core::ProcessSession*>(session)->read(*reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(ff), [&] (auto& input_stream) -> int64_t {
      return cb(user_ctx, reinterpret_cast<MinifiInputStream>(input_stream.get()));
    });
    return MINIFI_SUCCESS;
  } catch (...) {
    return MINIFI_UNKNOWN_ERROR;
  }
}

MinifiStatus MinifiProcessSessionWrite(MinifiProcessSession session, MinifiFlowFile ff, int64_t(*cb)(void* user_ctx, MinifiOutputStream), void* user_ctx) {
  gsl_Assert(session != MINIFI_NULL);
  gsl_Assert(ff != MINIFI_NULL);
  try {
    reinterpret_cast<minifi::core::ProcessSession*>(session)->write(*reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(ff), [&] (auto& output_stream) -> int64_t {
      return cb(user_ctx, reinterpret_cast<MinifiOutputStream>(output_stream.get()));
    });
    return MINIFI_SUCCESS;
  } catch (...) {
    return MINIFI_UNKNOWN_ERROR;
  }
}

uint64_t MinifiInputStreamSize(MinifiInputStream stream) {
  gsl_Assert(stream != MINIFI_NULL);
  return gsl::narrow<uint64_t>(reinterpret_cast<minifi::io::InputStream*>(stream)->size());
}

int64_t MinifiInputStreamRead(MinifiInputStream stream, char* data, uint64_t size) {
  gsl_Assert(stream != MINIFI_NULL);
  return gsl::narrow<int64_t>(reinterpret_cast<minifi::io::InputStream*>(stream)->read(std::span(reinterpret_cast<std::byte*>(data), size)));
}

int64_t MinifiOutputStreamWrite(MinifiOutputStream stream, const char* data, uint64_t size) {
  gsl_Assert(stream != MINIFI_NULL);
  return gsl::narrow<int64_t>(reinterpret_cast<minifi::io::OutputStream*>(stream)->write(std::span(reinterpret_cast<const std::byte*>(data), size)));
}

void MinifiStatusToString(MinifiStatus status, void(*cb)(void* user_ctx, MinifiStringView str), void* user_ctx) {
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
  cb(user_ctx, MinifiStringView{.data = message.data(), .length = gsl::narrow<uint32_t>(message.size())});
}

void MinifiFlowFileSetAttribute(MinifiProcessSession session, MinifiFlowFile ff, MinifiStringView key, const MinifiStringView* value) {
  gsl_Assert(session != MINIFI_NULL);
  gsl_Assert(ff != MINIFI_NULL);
  if (value == nullptr) {
    reinterpret_cast<minifi::core::ProcessSession*>(session)->removeAttribute(**reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(ff), toString(key));
  } else {
    reinterpret_cast<minifi::core::ProcessSession*>(session)->putAttribute(**reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(ff), toString(key), toString(*value));
  }
}

MinifiBool MinifiFlowFileGetAttribute(MinifiProcessSession session, MinifiFlowFile ff, MinifiStringView key, void(*cb)(void* user_ctx, MinifiStringView), void* user_ctx) {
  gsl_Assert(session != MINIFI_NULL);
  gsl_Assert(ff != MINIFI_NULL);
  auto value = (*reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(ff))->getAttribute(toString(key));
  if (!value.has_value()) {
    return MINIFI_FALSE;
  }
  cb(user_ctx, MinifiStringView{.data = value->data(), .length = gsl::narrow<uint32_t>(value->size())});
  return MINIFI_TRUE;
}

void MinifiFlowFileGetAttributes(MinifiProcessSession session, MinifiFlowFile ff, void(*cb)(void* user_ctx, MinifiStringView, MinifiStringView), void* user_ctx) {
  gsl_Assert(session != MINIFI_NULL);
  gsl_Assert(ff != MINIFI_NULL);
  for (auto& [key, value] : (*reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(ff))->getAttributes()) {
    cb(user_ctx, MinifiStringView{.data = key.data(), .length = gsl::narrow<uint32_t>(key.size())}, MinifiStringView{.data = value.data(), .length = gsl::narrow<uint32_t>(value.size())});
  }
}

}  // extern "C"
