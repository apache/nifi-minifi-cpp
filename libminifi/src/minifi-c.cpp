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
#include "core/extension/ExtensionManager.h"
#include "utils/PropertyErrors.h"
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
    case MINIFI_LOG_LEVEL_TRACE: return minifi::core::logging::trace;
    case MINIFI_LOG_LEVEL_DEBUG: return minifi::core::logging::debug;
    case MINIFI_LOG_LEVEL_INFO: return minifi::core::logging::info;
    case MINIFI_LOG_LEVEL_WARNING: return minifi::core::logging::warn;
    case MINIFI_LOG_LEVEL_ERROR: return minifi::core::logging::err;
    case MINIFI_LOG_LEVEL_CRITICAL: return minifi::core::logging::critical;
    case MINIFI_LOG_LEVEL_OFF: return minifi::core::logging::off;
  }
  gsl_FailFast();
}

gsl::not_null<const minifi::core::PropertyValidator*> toPropertyValidator(MinifiValidator validator) {
  switch (validator) {
    case MINIFI_VALIDATOR_ALWAYS_VALID: return gsl::make_not_null(&minifi::core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR);
    case MINIFI_VALIDATOR_NON_BLANK: return gsl::make_not_null(&minifi::core::StandardPropertyValidators::NON_BLANK_VALIDATOR);
    case MINIFI_VALIDATOR_TIME_PERIOD: return gsl::make_not_null(&minifi::core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR);
    case MINIFI_VALIDATOR_BOOLEAN: return gsl::make_not_null(&minifi::core::StandardPropertyValidators::BOOLEAN_VALIDATOR);
    case MINIFI_VALIDATOR_INTEGER: return gsl::make_not_null(&minifi::core::StandardPropertyValidators::INTEGER_VALIDATOR);
    case MINIFI_VALIDATOR_UNSIGNED_INTEGER: return gsl::make_not_null(&minifi::core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR);
    case MINIFI_VALIDATOR_DATA_SIZE: return gsl::make_not_null(&minifi::core::StandardPropertyValidators::DATA_SIZE_VALIDATOR);
    case MINIFI_VALIDATOR_PORT: return gsl::make_not_null(&minifi::core::StandardPropertyValidators::PORT_VALIDATOR);
  }
  gsl_FailFast();
}

minifi::core::Property createProperty(const MinifiPropertyDefinition* property_description) {
  gsl_Expects(property_description);
  std::vector<std::string_view> allowed_values;
  allowed_values.reserve(property_description->allowed_values_count);
  for (size_t i = 0; i < property_description->allowed_values_count; ++i) {
    allowed_values.push_back(toStringView(property_description->allowed_values_ptr[i]));
  }
  std::vector<std::string_view> allowed_types;
  if (property_description->type) {
    allowed_types.push_back(toStringView(*property_description->type));
  }
  std::optional<std::string_view> default_value;
  if (property_description->default_value) {
    default_value = toStringView(*property_description->default_value);
  }
  return minifi::core::Property{minifi::core::PropertyReference{
    toStringView(property_description->name),
    toStringView(property_description->display_name),
    toStringView(property_description->description),
    property_description->is_required,
    property_description->is_sensitive,
    std::span(allowed_values),
    std::span(allowed_types),
    default_value,
    toPropertyValidator(property_description->validator),
    property_description->supports_expression_language
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

}  // namespace

namespace org::apache::nifi::minifi::utils {

void useCProcessorClassDescription(const MinifiProcessorClassDefinition& class_description, const std::function<void(minifi::ClassDescription, minifi::utils::CProcessorClassDescription)>& fn) {
  std::vector<minifi::core::Property> properties;
  properties.reserve(class_description.class_properties_count);
  for (size_t i = 0; i < class_description.class_properties_count; ++i) {
    properties.push_back(createProperty(&class_description.class_properties_ptr[i]));
  }
  std::vector<minifi::core::DynamicProperty> dynamic_properties;
  dynamic_properties.reserve(class_description.dynamic_properties_count);
  for (size_t i = 0; i < class_description.dynamic_properties_count; ++i) {
    dynamic_properties.push_back(minifi::core::DynamicPropertyDefinition{
      .name = toStringView(class_description.dynamic_properties_ptr[i].name),
      .value = toStringView(class_description.dynamic_properties_ptr[i].value),
      .description = toStringView(class_description.dynamic_properties_ptr[i].description),
      .supports_expression_language = class_description.dynamic_properties_ptr[i].supports_expression_language
    });
  }
  std::vector<minifi::core::Relationship> relationships;
  relationships.reserve(class_description.class_relationships_count);
  for (size_t i = 0; i < class_description.class_relationships_count; ++i) {
    relationships.push_back(minifi::core::Relationship{
      toString(class_description.class_relationships_ptr[i].name),
      toString(class_description.class_relationships_ptr[i].description)
    });
  }
  std::vector<minifi::core::OutputAttribute> output_attributes;
  for (size_t attribute_idx = 0; attribute_idx < class_description.output_attributes_count; ++attribute_idx) {
    minifi::core::OutputAttribute output_attribute{};
    output_attribute.name = toString(class_description.output_attributes_ptr[attribute_idx].name);
    output_attribute.description = toString(class_description.output_attributes_ptr[attribute_idx].description);
    for (size_t rel_idx = 0; rel_idx < class_description.output_attributes_ptr[attribute_idx].relationships_count; ++rel_idx) {
      auto output_attribute_rel_name = toStringView(class_description.output_attributes_ptr[attribute_idx].relationships_ptr[rel_idx]);
      auto rel_it = std::find(relationships.begin(), relationships.end(), minifi::core::Relationship{std::string{output_attribute_rel_name}, ""});
      gsl_Assert(rel_it != relationships.end());
      output_attribute.relationships.push_back(*rel_it);
    }
    output_attributes.push_back(output_attribute);
  }

  auto name_segments = minifi::utils::string::split(toStringView(class_description.full_name), "::");
  gsl_Assert(!name_segments.empty());

  minifi::ClassDescription description{
    .type_ = minifi::ResourceType::Processor,
    .short_name_ = name_segments.back(),
    .full_name_ = minifi::utils::string::join(".", name_segments),
    .description_ = toString(class_description.description),
    .class_properties_ = properties,
    .dynamic_properties_ = dynamic_properties,
    .class_relationships_ = relationships,
    .output_attributes_ = output_attributes,
    .supports_dynamic_properties_ = class_description.supports_dynamic_properties,
    .supports_dynamic_relationships_ = class_description.supports_dynamic_relationships,
    .inputRequirement_ = minifi::core::annotation::toString(toInputRequirement(class_description.input_requirement)),
    .isSingleThreaded_ = class_description.is_single_threaded,
  };

  minifi::utils::CProcessorClassDescription c_class_description{
    .name = name_segments.back(),
    .class_properties = properties,
    .class_relationships = relationships,
    .supports_dynamic_properties = description.supports_dynamic_properties_,
    .supports_dynamic_relationships = description.supports_dynamic_relationships_,
    .input_requirement = toInputRequirement(class_description.input_requirement),
    .is_single_threaded = description.isSingleThreaded_,

    .callbacks = class_description.callbacks
  };

  fn(description, c_class_description);
}

}  // namespace org::apache::nifi::minifi::utils

extern "C" {

MinifiExtension* MinifiCreateExtension(MinifiStringView /*api_version*/, const MinifiExtensionCreateInfo* extension_create_info) {
  gsl_Assert(extension_create_info);
  auto extension_name = toString(extension_create_info->name);
  minifi::BundleIdentifier bundle{
    .name = extension_name,
    .version = toString(extension_create_info->version)
  };
  auto& bundle_components = minifi::ClassDescriptionRegistry::getMutableClassDescriptions()[bundle];
  for (size_t proc_idx = 0; proc_idx < extension_create_info->processors_count; ++proc_idx) {
    minifi::utils::useCProcessorClassDescription(extension_create_info->processors_ptr[proc_idx], [&] (const auto& description, const auto& c_class_description) {
      minifi::core::ClassLoader::getDefaultClassLoader().getClassLoader(extension_name).registerClass(
        c_class_description.name,
        std::make_unique<CProcessorFactory>(extension_name, toString(extension_create_info->processors_ptr[proc_idx].full_name), c_class_description));
      bundle_components.processors.emplace_back(description);
    });
  }
  return reinterpret_cast<MinifiExtension*>(new org::apache::nifi::minifi::core::extension::Extension::Info{
    .name = toString(extension_create_info->name),
    .version = toString(extension_create_info->version),
    .deinit = extension_create_info->deinit,
    .user_data = extension_create_info->user_data
  });
}

MinifiStatus MinifiProcessContextGetProperty(MinifiProcessContext* context, MinifiStringView property_name, MinifiFlowFile* flowfile,
    void (*result_cb)(void* user_ctx, MinifiStringView result), void* user_ctx) {
  gsl_Assert(context != MINIFI_NULL);
  auto result = reinterpret_cast<minifi::core::ProcessContext*>(context)->getProperty(toStringView(property_name),
      flowfile != MINIFI_NULL ? reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(flowfile)->get() : nullptr);
  if (result) {
    result_cb(user_ctx, MinifiStringView{.data = result.value().data(), .length = result.value().length()});
    return MINIFI_STATUS_SUCCESS;
  }
  switch (static_cast<minifi::core::PropertyErrorCode>(result.error().value())) {
    case minifi::core::PropertyErrorCode::NotSupportedProperty: return MINIFI_STATUS_NOT_SUPPORTED_PROPERTY;
    case minifi::core::PropertyErrorCode::DynamicPropertiesNotSupported: return MINIFI_STATUS_DYNAMIC_PROPERTIES_NOT_SUPPORTED;
    case minifi::core::PropertyErrorCode::PropertyNotSet: return MINIFI_STATUS_PROPERTY_NOT_SET;
    case minifi::core::PropertyErrorCode::ValidationFailed: return MINIFI_STATUS_VALIDATION_FAILED;
    default: return MINIFI_STATUS_UNKNOWN_ERROR;
  }
}

MinifiBool MinifiProcessContextHasNonEmptyProperty(MinifiProcessContext* context, MinifiStringView property_name) {
  gsl_Assert(context != MINIFI_NULL);
  return reinterpret_cast<minifi::core::ProcessContext*>(context)->hasNonEmptyProperty(toString(property_name));
}

void MinifiConfigGet(MinifiConfig* configure, MinifiStringView key, void(*cb)(void* user_ctx, MinifiStringView result), void* user_ctx) {
  gsl_Assert(configure != MINIFI_NULL);
  auto value = reinterpret_cast<minifi::Configure*>(configure)->get(toString(key));
  if (value) {
    cb(user_ctx, MinifiStringView{
      .data = value->data(),
      .length = value->length()
    });
  }
}

void MinifiLoggerSetMaxLogSize(MinifiLogger* logger, int32_t max_size) {
  gsl_Assert(logger != MINIFI_NULL);
  (*reinterpret_cast<std::shared_ptr<minifi::core::logging::Logger>*>(logger))->set_max_log_size(max_size);
}

void MinifiLoggerLogString(MinifiLogger* logger, MinifiLogLevel level, MinifiStringView msg) {
  gsl_Assert(logger != MINIFI_NULL);
  (*reinterpret_cast<std::shared_ptr<minifi::core::logging::Logger>*>(logger))->log_string(toLogLevel(level), toString(msg));
}

MinifiBool MinifiLoggerShouldLog(MinifiLogger* logger, MinifiLogLevel level) {
  gsl_Assert(logger != MINIFI_NULL);
  return (*reinterpret_cast<std::shared_ptr<minifi::core::logging::Logger>*>(logger))->should_log(toLogLevel(level));
}

MinifiLogLevel MinifiLoggerLevel(MinifiLogger* logger) {
  gsl_Assert(logger != MINIFI_NULL);
  switch ((*reinterpret_cast<std::shared_ptr<minifi::core::logging::Logger>*>(logger))->level()) {
    case minifi::core::logging::LOG_LEVEL::trace: return MINIFI_LOG_LEVEL_TRACE;
    case minifi::core::logging::LOG_LEVEL::debug: return MINIFI_LOG_LEVEL_DEBUG;
    case minifi::core::logging::LOG_LEVEL::info: return MINIFI_LOG_LEVEL_INFO;
    case minifi::core::logging::LOG_LEVEL::warn: return MINIFI_LOG_LEVEL_WARNING;
    case minifi::core::logging::LOG_LEVEL::err: return MINIFI_LOG_LEVEL_ERROR;
    case minifi::core::logging::LOG_LEVEL::critical: return MINIFI_LOG_LEVEL_CRITICAL;
    case minifi::core::logging::LOG_LEVEL::off: return MINIFI_LOG_LEVEL_OFF;
  }
  gsl_FailFast();
}

MINIFI_OWNED gsl::owner<MinifiPublishedMetrics*> MinifiPublishedMetricsCreate(const size_t count, const MinifiStringView* metric_names, const double* metric_values) {
  const gsl::owner<std::vector<minifi::state::PublishedMetric>*> metrics = new std::vector<minifi::state::PublishedMetric>();  // NOLINT(modernize-use-auto)
  metrics->reserve(count);
  for (size_t i = 0; i < count; i++) {
    metrics->emplace_back(minifi::state::PublishedMetric{toString(metric_names[i]), metric_values[i], {}});
  }
  return reinterpret_cast<MinifiPublishedMetrics*>(metrics);
}

MINIFI_OWNED MinifiFlowFile* MinifiProcessSessionGet(MinifiProcessSession* session) {
  gsl_Assert(session != MINIFI_NULL);
  if (const auto ff = reinterpret_cast<minifi::core::ProcessSession*>(session)->get()) {
    return reinterpret_cast<MinifiFlowFile*>(new std::shared_ptr<minifi::core::FlowFile>(ff));
  }
  return MINIFI_NULL;
}

MINIFI_OWNED MinifiFlowFile* MinifiProcessSessionCreate(MinifiProcessSession* session, MinifiFlowFile* parent) {
  gsl_Assert(session != MINIFI_NULL);
  if (const auto ff = reinterpret_cast<minifi::core::ProcessSession*>(session)->create(parent != MINIFI_NULL
              ? reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(parent)->get()
              : nullptr)) {
    return reinterpret_cast<MinifiFlowFile*>(new std::shared_ptr<minifi::core::FlowFile>(ff));
  }
  return MINIFI_NULL;
}

MinifiStatus MinifiProcessSessionTransfer(MinifiProcessSession* session, MINIFI_OWNED MinifiFlowFile* flowfile, MinifiStringView relationship_name) {
  gsl_Assert(session != MINIFI_NULL);
  gsl_Assert(flowfile !=  MINIFI_NULL);
  try {
    reinterpret_cast<minifi::core::ProcessSession*>(session)->transfer(
        *reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(flowfile), minifi::core::Relationship{toString(relationship_name), ""});
    return MINIFI_STATUS_SUCCESS;
  } catch (...) {
    return MINIFI_STATUS_UNKNOWN_ERROR;
  }
}

MinifiStatus MinifiProcessSessionRemove(MinifiProcessSession* session, MINIFI_OWNED MinifiFlowFile* flowfile) {
  gsl_Assert(session != MINIFI_NULL);
  gsl_Assert(flowfile != MINIFI_NULL);
  try {
    reinterpret_cast<minifi::core::ProcessSession*>(session)->remove(*reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(flowfile));
    return MINIFI_STATUS_SUCCESS;
  } catch (...) {
    return MINIFI_STATUS_UNKNOWN_ERROR;
  }
}

MinifiStatus MinifiProcessSessionRead(MinifiProcessSession* session, MinifiFlowFile* flowfile, int64_t(*cb)(void* user_ctx, MinifiInputStream*), void* user_ctx) {
  gsl_Assert(session != MINIFI_NULL);
  gsl_Assert(flowfile != MINIFI_NULL);
  try {
    reinterpret_cast<minifi::core::ProcessSession*>(session)->read(*reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(flowfile), [&] (auto& input_stream) -> int64_t {
      return cb(user_ctx, reinterpret_cast<MinifiInputStream*>(input_stream.get()));
    });
    return MINIFI_STATUS_SUCCESS;
  } catch (...) {
    return MINIFI_STATUS_UNKNOWN_ERROR;
  }
}

MinifiStatus MinifiProcessSessionWrite(MinifiProcessSession* session, MinifiFlowFile* ff, int64_t(*cb)(void* user_ctx, MinifiOutputStream*), void* user_ctx) {
  gsl_Assert(session != MINIFI_NULL);
  gsl_Assert(ff != MINIFI_NULL);
  try {
    reinterpret_cast<minifi::core::ProcessSession*>(session)->write(*reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(ff), [&] (auto& output_stream) -> int64_t {
      return cb(user_ctx, reinterpret_cast<MinifiOutputStream*>(output_stream.get()));
    });
    return MINIFI_STATUS_SUCCESS;
  } catch (...) {
    return MINIFI_STATUS_UNKNOWN_ERROR;
  }
}

size_t MinifiInputStreamSize(MinifiInputStream* stream) {
  gsl_Assert(stream != MINIFI_NULL);
  return reinterpret_cast<minifi::io::InputStream*>(stream)->size();
}

int64_t MinifiInputStreamRead(MinifiInputStream* stream, char* buffer, size_t size) {
  gsl_Assert(stream != MINIFI_NULL);
  return gsl::narrow<int64_t>(reinterpret_cast<minifi::io::InputStream*>(stream)->read(std::span(reinterpret_cast<std::byte*>(buffer), size)));
}

int64_t MinifiOutputStreamWrite(MinifiOutputStream* stream, const char* data, size_t size) {
  gsl_Assert(stream != MINIFI_NULL);
  return gsl::narrow<int64_t>(reinterpret_cast<minifi::io::OutputStream*>(stream)->write(as_bytes(std::span(data, size))));
}

void MinifiStatusToString(MinifiStatus status, void(*cb)(void* user_ctx, MinifiStringView str), void* user_ctx) {
  std::string message = [&] () -> std::string {
    switch (status) {
      case MINIFI_STATUS_SUCCESS: return "Success";
      case MINIFI_STATUS_UNKNOWN_ERROR: return "Unknown error";
      case MINIFI_STATUS_NOT_SUPPORTED_PROPERTY: return minifi::core::PropertyErrorCategory{}.message(static_cast<int>(minifi::core::PropertyErrorCode::NotSupportedProperty));
      case MINIFI_STATUS_DYNAMIC_PROPERTIES_NOT_SUPPORTED: return minifi::core::PropertyErrorCategory{}.message(static_cast<int>(minifi::core::PropertyErrorCode::DynamicPropertiesNotSupported));
      case MINIFI_STATUS_PROPERTY_NOT_SET: return minifi::core::PropertyErrorCategory{}.message(static_cast<int>(minifi::core::PropertyErrorCode::PropertyNotSet));
      case MINIFI_STATUS_VALIDATION_FAILED: return minifi::core::PropertyErrorCategory{}.message(static_cast<int>(minifi::core::PropertyErrorCode::ValidationFailed));
      default: return "Unknown error";
    }
  }();
  cb(user_ctx, MinifiStringView{.data = message.data(), .length = message.size()});
}

MinifiStatus MinifiFlowFileSetAttribute(MinifiProcessSession* session, MinifiFlowFile* flowfile, MinifiStringView attribute_name, const MinifiStringView* attribute_value) {
  gsl_Assert(session != MINIFI_NULL);
  gsl_Assert(flowfile != MINIFI_NULL);
  if (attribute_value == nullptr) {
    reinterpret_cast<minifi::core::ProcessSession*>(session)->removeAttribute(**reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(flowfile), toString(attribute_name));
  } else {
    reinterpret_cast<minifi::core::ProcessSession*>(session)->putAttribute(
        **reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(flowfile), toString(attribute_name), toString(*attribute_value));
  }
  return MINIFI_STATUS_SUCCESS;
}

MinifiBool MinifiFlowFileGetAttribute(MinifiProcessSession* session, MinifiFlowFile* flowfile, MinifiStringView attribute_name,
                                      void(*cb)(void* user_ctx, MinifiStringView attribute_value), void* user_ctx) {
  gsl_Assert(session != MINIFI_NULL);
  gsl_Assert(flowfile != MINIFI_NULL);
  auto value = (*reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(flowfile))->getAttribute(toString(attribute_name));
  if (!value.has_value()) {
    return false;
  }
  cb(user_ctx, MinifiStringView{.data = value->data(), .length = value->size()});
  return true;
}

void MinifiFlowFileGetAttributes(MinifiProcessSession* session, MinifiFlowFile* flowfile,
                                 void(*cb)(void* user_ctx, MinifiStringView attribute_name, MinifiStringView attribute_value), void* user_ctx) {
  gsl_Assert(session != MINIFI_NULL);
  gsl_Assert(flowfile != MINIFI_NULL);
  for (auto& [key, value] : (*reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(flowfile))->getAttributes()) {
    cb(user_ctx, MinifiStringView{.data = key.data(), .length = key.size()}, MinifiStringView{.data = value.data(), .length = value.size()});
  }
}

}  // extern "C"
