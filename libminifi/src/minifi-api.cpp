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

#include <memory>
#include <vector>

#include "agent/agent_docs.h"
#include "core/Processor.h"
#include "core/ProcessorMetrics.h"
#include "core/extension/ExtensionManager.h"
#include "minifi-c/minifi-api.h"
#include "minifi-cpp/controllers/ProxyConfigurationServiceInterface.h"
#include "minifi-cpp/controllers/SSLContextServiceInterface.h"
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
#include "utils/CControllerService.h"
#include "utils/CProcessor.h"
#include "utils/PropertyErrors.h"

namespace minifi = org::apache::nifi::minifi;

namespace {

minifi_string_view minifiStringView(const std::string_view s) {
  return minifi_string_view{.data = s.data(), .length = s.size()};
}

std::string toString(minifi_string_view sv) {
  return {sv.data, sv.length};
}

std::string_view toStringView(minifi_string_view sv) {
  return {sv.data, sv.length};
}

minifi::core::annotation::Input toInputRequirement(minifi_input_requirement req) {
  switch (req) {
    case MINIFI_INPUT_REQUIRED: return minifi::core::annotation::Input::INPUT_REQUIRED;
    case MINIFI_INPUT_ALLOWED: return minifi::core::annotation::Input::INPUT_ALLOWED;
    case MINIFI_INPUT_FORBIDDEN: return minifi::core::annotation::Input::INPUT_FORBIDDEN;
  }
  gsl_FailFast();
}

minifi::core::logging::LOG_LEVEL toLogLevel(minifi_log_level lvl) {
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

gsl::not_null<const minifi::core::PropertyValidator*> toPropertyValidator(minifi_validator validator) {
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

minifi::core::Property createProperty(const minifi_property_definition* property_description) {
  gsl_Expects(property_description);
  std::vector<std::string_view> allowed_values;
  allowed_values.reserve(property_description->allowed_values_count);
  for (size_t i = 0; i < property_description->allowed_values_count; ++i) {
    allowed_values.push_back(toStringView(property_description->allowed_values_ptr[i]));
  }
  std::vector<std::string_view> allowed_types;
  if (property_description->allowed_type) {
    allowed_types.push_back(toStringView(*property_description->allowed_type));
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

class CControllerServiceFactory : public minifi::core::controller::ControllerServiceFactory {
 public:
  CControllerServiceFactory(std::string module_name, std::string class_name, minifi::utils::CControllerServiceClassDescription class_description)
      : module_name_(std::move(module_name)),
        class_name_(std::move(class_name)),
        class_description_(std::move(class_description)) {}

  std::unique_ptr<minifi::core::controller::ControllerServiceApi> create(minifi::core::ControllerServiceMetadata metadata) override {
    return std::make_unique<minifi::utils::CControllerService>(class_description_, std::move(metadata));
  }

  [[nodiscard]] std::string getModuleName() const override { return module_name_; }

  [[nodiscard]] std::string getClassName() const override { return class_name_; }

  CControllerServiceFactory() = delete;
  CControllerServiceFactory(const CControllerServiceFactory&) = delete;
  CControllerServiceFactory& operator=(const CControllerServiceFactory&) = delete;
  CControllerServiceFactory(CControllerServiceFactory&&) = delete;
  CControllerServiceFactory& operator=(CControllerServiceFactory&&) = delete;
  ~CControllerServiceFactory() override = default;

 private:
  std::string module_name_;
  std::string class_name_;
  minifi::utils::CControllerServiceClassDescription class_description_;
};

minifi_proxy_type minifiProxyType(const minifi::controllers::ProxyType& proxy_type) {
  switch (proxy_type) {
    case minifi::controllers::ProxyType::DIRECT:
      return minifi_proxy_type::MINIFI_PROXY_TYPE_DIRECT;
    case minifi::controllers::ProxyType::HTTP:
      return minifi_proxy_type::MINIFI_PROXY_TYPE_HTTP;
  }
  std::unreachable();
}

}  // namespace

namespace org::apache::nifi::minifi::utils {

void useCProcessorClassDescription(const minifi_processor_class_definition& class_description, const std::function<void(minifi::ClassDescription, minifi::utils::CProcessorClassDescription)>& fn) {
  std::vector<minifi::core::Property> properties;
  properties.reserve(class_description.properties_count);
  for (size_t i = 0; i < class_description.properties_count; ++i) {
    properties.push_back(createProperty(&class_description.properties_ptr[i]));
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
  relationships.reserve(class_description.relationships_count);
  for (size_t i = 0; i < class_description.relationships_count; ++i) {
    relationships.push_back(minifi::core::Relationship{
      toString(class_description.relationships_ptr[i].name),
      toString(class_description.relationships_ptr[i].description)
    });
  }
  std::vector<minifi::core::OutputAttribute> output_attributes;
  for (size_t attribute_idx = 0; attribute_idx < class_description.output_attributes_count; ++attribute_idx) {
    minifi::core::OutputAttribute output_attribute{};
    output_attribute.name = toString(class_description.output_attributes_ptr[attribute_idx].name);
    output_attribute.description = toString(class_description.output_attributes_ptr[attribute_idx].description);
    for (size_t rel_idx = 0; rel_idx < class_description.output_attributes_ptr[attribute_idx].relationships_count; ++rel_idx) {
      auto output_attribute_rel_name = toStringView(class_description.output_attributes_ptr[attribute_idx].relationships_ptr[rel_idx]);
      auto rel_it = std::ranges::find(relationships, minifi::core::Relationship{std::string{output_attribute_rel_name}, ""});
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

void useCControllerServiceClassDescription(const std::string_view bundle_name, const minifi_controller_service_class_definition& class_description,
    const std::function<void(const ClassDescription&, CControllerServiceClassDescription)>& fn) {
  std::vector<minifi::core::Property> properties;
  properties.reserve(class_description.properties_count);
  for (size_t i = 0; i < class_description.properties_count; ++i) {
    properties.push_back(createProperty(&class_description.properties_ptr[i]));
  }

  auto name_segments = minifi::utils::string::split(toStringView(class_description.full_name), "::");
  gsl_Assert(!name_segments.empty());

  std::vector<core::ControllerServiceType> implements_apis;
  implements_apis.reserve(class_description.provided_interfaces_count);
  for (size_t i = 0; i < class_description.provided_interfaces_count; ++i) {
    auto api_segments = string::split(toStringView(class_description.provided_interfaces_ptr[i]), "::");
    std::string type_name = string::join(".", api_segments);
    api_segments.pop_back();
    std::string group_name = string::join(".", api_segments);
    implements_apis.emplace_back(std::string{bundle_name}, std::move(group_name), std::move(type_name));
  }

  ClassDescription description{
      .type_ = ResourceType::ControllerService,
      .short_name_ = name_segments.back(),
      .full_name_ = string::join(".", name_segments),
      .description_ = toString(class_description.description),
      .class_properties_ = properties,
      .api_implementations = implements_apis,
  };

  CControllerServiceClassDescription c_class_description{
    .full_name = toString(class_description.full_name),
    .class_properties = properties,
    .callbacks = class_description.callbacks
  };
  fn(description, c_class_description);
}
}  // namespace org::apache::nifi::minifi::utils

extern "C" {

minifi_extension* minifi_register_extension(minifi_extension_context* extension_context, const minifi_extension_definition* extension_definition) {
  gsl_Assert(extension_context);
  gsl_Assert(extension_definition);
  auto* extension = reinterpret_cast<org::apache::nifi::minifi::core::extension::Extension::Context*>(extension_context)->create(org::apache::nifi::minifi::core::extension::Extension::Info{
    .name = toString(extension_definition->name),
    .version = toString(extension_definition->version),
    .deinit = extension_definition->deinit,
    .user_data = extension_definition->user_data
  });
  if (extension) {
    return reinterpret_cast<minifi_extension*>(extension);
  }
  return nullptr;
}

minifi_status minifi_register_processor(minifi_extension* extension, const minifi_processor_class_definition* processor) {
  gsl_Assert(extension);
  gsl_Assert(processor);
  auto* extension_ = reinterpret_cast<minifi::core::extension::Extension*>(extension);
  auto extension_info = extension_->getInfo();
  if (!extension_info) {
    return MINIFI_STATUS_UNKNOWN_ERROR;
  }
  const minifi::BundleIdentifier bundle{
    .name = extension_info->name,
    .version = extension_info->version
  };
  auto& bundle_components = minifi::ClassDescriptionRegistry::getMutableClassDescriptions()[bundle];
  minifi::utils::useCProcessorClassDescription(*processor, [&] (const auto& description, const auto& c_class_description) {
    minifi::core::ClassLoader::getDefaultClassLoader().getClassLoader(extension_info->name).registerClass(
      c_class_description.name,
      std::make_unique<CProcessorFactory>(extension_info->name, toString(processor->full_name), c_class_description));
    bundle_components.processors.emplace_back(description);
    extension_->addClass(c_class_description.name);
  });
  return MINIFI_STATUS_SUCCESS;
}

minifi_status minifi_register_controller_service(minifi_extension* extension, const minifi_controller_service_class_definition* controller_service) {
  gsl_Assert(extension);
  gsl_Assert(controller_service);
  auto extension_info = reinterpret_cast<minifi::core::extension::Extension*>(extension)->getInfo();
  if (!extension_info) {
    return MINIFI_STATUS_UNKNOWN_ERROR;
  }
  const minifi::BundleIdentifier bundle{
    .name = extension_info->name,
    .version = extension_info->version
  };
  auto& bundle_components = minifi::ClassDescriptionRegistry::getMutableClassDescriptions()[bundle];
  minifi::utils::useCControllerServiceClassDescription(bundle.name, *controller_service, [&] (const auto& description, const auto& c_class_description) {
    minifi::core::ClassLoader::getDefaultClassLoader().getClassLoader(extension_info->name).registerClass(
      description.short_name_,
      std::make_unique<CControllerServiceFactory>(extension_info->name, toString(controller_service->full_name), c_class_description));
    bundle_components.controller_services.emplace_back(description);
  });
  return MINIFI_STATUS_SUCCESS;
}


minifi_extension* MINIFI_REGISTER_EXTENSION_FN(minifi_extension_context* extension_context, const minifi_extension_definition* extension_definition) {
  return minifi_register_extension(extension_context, extension_definition);
}

minifi_status minifi_process_context_set_trigger_when_empty(minifi_process_context* context, bool trigger_when_empty) {
  gsl_Assert(context);
  reinterpret_cast<minifi::core::ProcessContext*>(context)->getProcessor().setTriggerWhenEmpty(trigger_when_empty);
  return MINIFI_STATUS_SUCCESS;
}

minifi_status minifi_process_context_report_metrics(minifi_process_context* context, const size_t count, const minifi_string_view* metric_names, const double* metric_values) {
  gsl_Assert(context);
  std::vector<minifi::state::PublishedMetric> metrics;
  metrics.reserve(count);
  for (size_t i = 0; i < count; i++) {
    metrics.emplace_back(minifi::state::PublishedMetric{toString(metric_names[i]), metric_values[i], {}});
  }
  auto& processor = reinterpret_cast<minifi::core::ProcessContext*>(context)->getProcessor();
  auto& impl = processor.getImpl();
  if (const auto c_processor = dynamic_cast<minifi::utils::CProcessor*>(&impl)) {
    c_processor->reportMetrics(std::move(metrics));
    return MINIFI_STATUS_SUCCESS;
  }
  return MINIFI_STATUS_UNKNOWN_ERROR;
}

minifi_status minifi_process_context_get_property(minifi_process_context* context, minifi_string_view property_name, minifi_flow_file* flowfile,
    void (*result_cb)(void* user_ctx, minifi_string_view result), void* user_ctx) {
  gsl_Assert(context);
  auto result = reinterpret_cast<minifi::core::ProcessContext*>(context)->getProperty(toStringView(property_name),
      flowfile ? reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(flowfile)->get() : nullptr);
  if (result) {
    result_cb(user_ctx, minifiStringView(result.value()));
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

void minifi_config_get(minifi_extension_context* extension_context, minifi_string_view key, void(*cb)(void* user_ctx, minifi_string_view result), void* user_ctx) {
  gsl_Assert(extension_context);
  auto value = reinterpret_cast<org::apache::nifi::minifi::core::extension::Extension::Context*>(extension_context)->config->get(toString(key));
  if (value) {
    cb(user_ctx, minifiStringView(*value));
  }
}

void minifi_logger_log_string(minifi_logger* logger, minifi_log_level level, minifi_string_view msg) {
  gsl_Assert(logger);
  (*reinterpret_cast<std::shared_ptr<minifi::core::logging::Logger>*>(logger))->log_string(toLogLevel(level), toString(msg));
}

bool minifi_logger_should_log(minifi_logger* logger, minifi_log_level level) {
  gsl_Assert(logger);
  return (*reinterpret_cast<std::shared_ptr<minifi::core::logging::Logger>*>(logger))->should_log(toLogLevel(level));
}

MINIFI_OWNED minifi_flow_file* minifi_process_session_get(minifi_process_session* session) {
  gsl_Assert(session);
  if (const auto ff = reinterpret_cast<minifi::core::ProcessSession*>(session)->get()) {
    return reinterpret_cast<minifi_flow_file*>(new std::shared_ptr<minifi::core::FlowFile>(ff));
  }
  return nullptr;
}

MINIFI_OWNED minifi_flow_file* minifi_process_session_create(minifi_process_session* session, minifi_flow_file* parent) {
  gsl_Assert(session);
  if (const auto ff = reinterpret_cast<minifi::core::ProcessSession*>(session)->create(parent != nullptr
              ? reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(parent)->get()
              : nullptr)) {
    return reinterpret_cast<minifi_flow_file*>(new std::shared_ptr<minifi::core::FlowFile>(ff));
  }
  return nullptr;
}

minifi_status minifi_process_session_penalize(minifi_process_session* session, minifi_flow_file* flowfile) {
  gsl_Assert(session);
  gsl_Assert(flowfile);
  try {
    reinterpret_cast<minifi::core::ProcessSession*>(session)->penalize(
        *reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(flowfile));
    return MINIFI_STATUS_SUCCESS;
  } catch (...) {
    return MINIFI_STATUS_UNKNOWN_ERROR;
  }
}

minifi_status minifi_process_session_transfer(minifi_process_session* session, MINIFI_OWNED minifi_flow_file* flowfile, minifi_string_view relationship_name) {
  gsl_Assert(session);
  gsl_Assert(flowfile);
  try {
    reinterpret_cast<minifi::core::ProcessSession*>(session)->transfer(
        *reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(flowfile), minifi::core::Relationship{toString(relationship_name), ""});
    return MINIFI_STATUS_SUCCESS;
  } catch (...) {
    return MINIFI_STATUS_UNKNOWN_ERROR;
  }
}

minifi_status minifi_process_session_remove(minifi_process_session* session, MINIFI_OWNED minifi_flow_file* flowfile) {
  gsl_Assert(session);
  gsl_Assert(flowfile);
  try {
    reinterpret_cast<minifi::core::ProcessSession*>(session)->remove(*reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(flowfile));
    return MINIFI_STATUS_SUCCESS;
  } catch (...) {
    return MINIFI_STATUS_UNKNOWN_ERROR;
  }
}

minifi_status minifi_process_session_read(minifi_process_session* session, minifi_flow_file* flowfile, int64_t(*cb)(void* user_ctx, minifi_input_stream*), void* user_ctx) {
  gsl_Assert(session);
  gsl_Assert(flowfile);
  try {
    reinterpret_cast<minifi::core::ProcessSession*>(session)->read(*reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(flowfile), [&] (auto& input_stream) -> minifi::io::IoResult {
      const int64_t cb_result = cb(user_ctx, reinterpret_cast<minifi_input_stream*>(input_stream.get()));
      return minifi::io::IoResult::from(cb_result);
    });
    return MINIFI_STATUS_SUCCESS;
  } catch (...) {
    return MINIFI_STATUS_UNKNOWN_ERROR;
  }
}

minifi_status minifi_process_session_write(minifi_process_session* session, minifi_flow_file* ff, int64_t(*cb)(void* user_ctx, minifi_output_stream*), void* user_ctx) {
  gsl_Assert(session);
  gsl_Assert(ff);
  try {
    reinterpret_cast<minifi::core::ProcessSession*>(session)->write(*reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(ff), [&] (auto& output_stream) -> minifi::io::IoResult {
      const int64_t cb_result = cb(user_ctx, reinterpret_cast<minifi_output_stream*>(output_stream.get()));
      return minifi::io::IoResult::from(cb_result);
    });
    return MINIFI_STATUS_SUCCESS;
  } catch (...) {
    return MINIFI_STATUS_UNKNOWN_ERROR;
  }
}

size_t minifi_input_stream_size(minifi_input_stream* stream) {
  gsl_Assert(stream);
  return reinterpret_cast<minifi::io::InputStream*>(stream)->size();
}

int64_t minifi_input_stream_read(minifi_input_stream* stream, char* buffer, size_t size) {
  gsl_Assert(stream);
  return gsl::narrow<int64_t>(reinterpret_cast<minifi::io::InputStream*>(stream)->read(std::span(reinterpret_cast<std::byte*>(buffer), size)));
}

int64_t minifi_output_stream_write(minifi_output_stream* stream, const char* data, size_t size) {
  gsl_Assert(stream);
  return gsl::narrow<int64_t>(reinterpret_cast<minifi::io::OutputStream*>(stream)->write(as_bytes(std::span(data, size))));
}

minifi_status minifi_process_session_set_flow_file_attribute(minifi_process_session* session, minifi_flow_file* flowfile,
    minifi_string_view attribute_name, const minifi_string_view* attribute_value) {
  gsl_Assert(session);
  gsl_Assert(flowfile);
  if (attribute_value == nullptr) {
    reinterpret_cast<minifi::core::ProcessSession*>(session)->removeAttribute(**reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(flowfile),
        toString(attribute_name));
  } else {
    reinterpret_cast<minifi::core::ProcessSession*>(session)->putAttribute(**reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(flowfile),
        toString(attribute_name),
        toString(*attribute_value));
  }
  return MINIFI_STATUS_SUCCESS;
}

bool minifi_process_session_get_flow_file_attribute(minifi_process_session* session, minifi_flow_file* flowfile, minifi_string_view attribute_name,
                                      void(*cb)(void* user_ctx, minifi_string_view attribute_value), void* user_ctx) {
  gsl_Assert(session);
  gsl_Assert(flowfile);
  auto value = (*reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(flowfile))->getAttribute(toString(attribute_name));
  if (!value.has_value()) {
    return false;
  }
  cb(user_ctx, minifiStringView(*value));
  return true;
}

void minifi_process_session_get_flow_file_attributes(minifi_process_session* session, minifi_flow_file* flowfile,
                                 void(*cb)(void* user_ctx, minifi_string_view attribute_name, minifi_string_view attribute_value), void* user_ctx) {
  gsl_Assert(session);
  gsl_Assert(flowfile);
  for (auto& [key, value] : (*reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(flowfile))->getAttributes()) {
    cb(user_ctx, minifiStringView(key), minifiStringView(value));
  }
}

uint64_t minifi_process_session_get_flow_file_size(minifi_process_session* session, minifi_flow_file* flowfile) {
  gsl_Assert(session);
  gsl_Assert(flowfile);
  return (*reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(flowfile))->getSize();
}

minifi_status minifi_process_session_get_flow_file_id(minifi_process_session* session, minifi_flow_file* flowfile, void(*cb)(void* user_ctx, minifi_string_view flow_file_id), void* user_ctx) {
  gsl_Assert(session);
  gsl_Assert(flowfile);
  const auto uuid_small_str = (*reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(flowfile))->getUUIDStr();
  cb(user_ctx, minifiStringView(uuid_small_str.view()));
  return MINIFI_STATUS_SUCCESS;
}

minifi_status minifi_controller_service_context_get_property(minifi_controller_service_context* context, minifi_string_view property_name,
    void (*result_cb)(void* user_ctx, minifi_string_view result), void* user_ctx) {
  gsl_Assert(context);
  auto result = reinterpret_cast<minifi::core::controller::ControllerServiceContext*>(context)->getProperty(toStringView(property_name));
  if (result) {
    result_cb(user_ctx, minifiStringView(result.value()));
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

minifi_status minifi_process_context_get_controller_service_from_property(
    minifi_process_context* process_context,
    const minifi_string_view property_name,
    const minifi_string_view controller_service_type,
    minifi_controller_service** controller_service_out) {
  if (!controller_service_out) {
    return MINIFI_STATUS_UNKNOWN_ERROR;
  }

  gsl_Assert(process_context);
  const auto context = reinterpret_cast<minifi::core::ProcessContext*>(process_context);
  const auto property_name_str = std::string{toStringView(property_name)};
  const auto name_str = context->getProperty(property_name_str, nullptr);
  if (!name_str) { return MINIFI_STATUS_PROPERTY_NOT_SET; }
  const auto service_shared_ptr = context->getControllerService(*name_str, context->getProcessorInfo().getUUID());
  if (!service_shared_ptr) {
    return MINIFI_STATUS_VALIDATION_FAILED;
  }
  if (const minifi::utils::CControllerService* c_controller_service = dynamic_cast<minifi::utils::CControllerService*>(&*service_shared_ptr)) {
    const auto class_description = c_controller_service->getClassDescription();
    if (class_description.full_name == toStringView(controller_service_type)) {
      *controller_service_out = static_cast<minifi_controller_service*>(c_controller_service->getImpl());
      return MINIFI_STATUS_SUCCESS;
    }
    if (class_description.callbacks.get_interface) {
      const auto interface_res = class_description.callbacks.get_interface(c_controller_service->getImpl(), controller_service_type);
      if (!interface_res) {
        return MINIFI_STATUS_VALIDATION_FAILED;
      }
      *controller_service_out = static_cast<minifi_controller_service*>(interface_res);
      return MINIFI_STATUS_SUCCESS;
    }
  }
  return MINIFI_STATUS_VALIDATION_FAILED;
}

void minifi_process_context_get_dynamic_properties(minifi_process_context* context, minifi_flow_file* minifi_flow_file,
    void (*cb)(void* user_ctx, minifi_string_view dynamic_property_name, minifi_string_view dynamic_property_value), void* user_ctx) {
  gsl_Assert(context);
  auto flow_file = minifi_flow_file ? reinterpret_cast<std::shared_ptr<minifi::core::FlowFile>*>(minifi_flow_file)->get() : nullptr;
  for (auto& [key, value] : reinterpret_cast<minifi::core::ProcessContext*>(context)->getDynamicProperties(flow_file)) {
    cb(user_ctx, minifiStringView(key), minifiStringView(value));
  }
}

enum minifi_status minifi_process_context_get_ssl_data_from_property(struct minifi_process_context* process_context, struct minifi_string_view property_name,
    void (*cb)(void* user_ctx, const struct minifi_ssl_data* ssl_data), void* user_ctx) {
  gsl_Assert(process_context);
  try {
    const auto context = reinterpret_cast<minifi::core::ProcessContext*>(process_context);
    const auto property_name_str = std::string{toStringView(property_name)};
    const auto name_str = context->getProperty(property_name_str, nullptr);
    if (!name_str) { return MINIFI_STATUS_PROPERTY_NOT_SET; }
    const auto service_shared_ptr = context->getControllerService(*name_str, context->getProcessorInfo().getUUID());
    if (!service_shared_ptr) { return MINIFI_STATUS_VALIDATION_FAILED; }
    if (const auto ssl_context_service = dynamic_cast<minifi::controllers::SSLContextServiceInterface*>(service_shared_ptr.get())) {
      const std::string ca_cert_file = ssl_context_service->getCACertificate().string();
      const std::string passphrase = ssl_context_service->getPassphrase();
      const std::string cert_file = ssl_context_service->getCertificateFile().string();
      const std::string private_key_file = ssl_context_service->getPrivateKeyFile().string();

      minifi_ssl_data ssl_data{
        .ca_certificate_file = minifiStringView(ca_cert_file),
        .certificate_file = minifiStringView(cert_file),
        .private_key_file = minifiStringView(private_key_file),
        .passphrase = minifiStringView(passphrase),
      };
      cb(user_ctx, &ssl_data);
      return MINIFI_STATUS_SUCCESS;
    }
    return MINIFI_STATUS_VALIDATION_FAILED;
  } catch (...) {
    return MINIFI_STATUS_UNKNOWN_ERROR;
  }
}


minifi_status minifi_process_context_get_proxy_data_from_property(minifi_process_context* process_context, minifi_string_view property_name,
    void (*cb)(void* user_ctx, const minifi_proxy_data* proxy_data), void* user_ctx) {
  gsl_Assert(process_context);
  const auto context = reinterpret_cast<minifi::core::ProcessContext*>(process_context);
  const auto property_name_str = std::string{toStringView(property_name)};
  const auto name_str = context->getProperty(property_name_str, nullptr);
  if (!name_str) { return MINIFI_STATUS_PROPERTY_NOT_SET; }
  const auto service_shared_ptr = context->getControllerService(*name_str, context->getProcessorInfo().getUUID());
  if (!service_shared_ptr) { return MINIFI_STATUS_VALIDATION_FAILED; }
  if (const auto proxy_service = dynamic_cast<minifi::controllers::ProxyConfigurationServiceInterface*>(service_shared_ptr.get())) {
    const std::string hostname = proxy_service->getHost();
    const auto basic_auth_data = proxy_service->getProxyCredentials();
    minifi_string_view username_holder = basic_auth_data ? minifiStringView(basic_auth_data->username) : minifi_string_view{};
    minifi_string_view password_holder = basic_auth_data ? minifiStringView(basic_auth_data->password) : minifi_string_view{};

    minifi_proxy_data proxy_data{
        .proxy_type = minifiProxyType(proxy_service->getProxyType()),
        .hostname = minifiStringView(hostname),
        .port = proxy_service->getPort(),
        .username = basic_auth_data ? &username_holder : nullptr,
        .password = basic_auth_data ? &password_holder : nullptr,
    };
    cb(user_ctx, &proxy_data);
    return MINIFI_STATUS_SUCCESS;
  }
  return MINIFI_STATUS_VALIDATION_FAILED;
}

}  // extern "C"
