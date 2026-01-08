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

#include "core/ProcessContextImpl.h"
#include "core/Processor.h"
#include "utils/PropertyErrors.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::core {

namespace {

class StandardProcessorInfo : public ProcessorInfo {
 public:
  explicit StandardProcessorInfo(Processor& proc): proc_(proc) {}

  [[nodiscard]] std::string getName() const override {return proc_.getName();}
  [[nodiscard]] utils::Identifier getUUID() const override {return proc_.getUUID();}
  [[nodiscard]] std::shared_ptr<state::FlowIdentifier> getFlowIdentifier() const override {return proc_.getFlowIdentifier();}
  [[nodiscard]] std::map<std::string, core::Property, std::less<>> getSupportedProperties() const override {return proc_.getSupportedProperties();}
  [[nodiscard]] nonstd::expected<Property, std::error_code> getSupportedProperty(std::string_view name) const override {return proc_.getSupportedProperty(name);}

 private:
  Processor& proc_;
};

}  // namespace

ProcessContextImpl::ProcessContextImpl(
    Processor& processor, controller::ControllerServiceProvider* controller_service_provider, const std::shared_ptr<core::Repository>& repo,
    const std::shared_ptr<core::Repository>& flow_repo, const std::shared_ptr<core::ContentRepository>& content_repo)
    : VariableRegistryImpl(static_cast<std::shared_ptr<Configure>>(minifi::Configure::create())),
      logger_(logging::LoggerFactory<ProcessContext>::getLogger()),
      controller_service_provider_(controller_service_provider),
      state_storage_(getStateStorage(logger_, controller_service_provider_, nullptr)),
      repo_(repo),
      flow_repo_(flow_repo),
      content_repo_(content_repo),
      processor_(processor),
      configure_(minifi::Configure::create()),
      info_(std::make_unique<StandardProcessorInfo>(processor)) {}

ProcessContextImpl::ProcessContextImpl(
    Processor& processor, controller::ControllerServiceProvider* controller_service_provider, const std::shared_ptr<core::Repository>& repo,
    const std::shared_ptr<core::Repository>& flow_repo, const std::shared_ptr<minifi::Configure>& configuration,
    const std::shared_ptr<core::ContentRepository>& content_repo)
    : VariableRegistryImpl(configuration),
      logger_(logging::LoggerFactory<ProcessContext>::getLogger()),
      controller_service_provider_(controller_service_provider),
      state_storage_(getStateStorage(logger_, controller_service_provider_, configuration)),
      repo_(repo),
      flow_repo_(flow_repo),
      content_repo_(content_repo),
      processor_(processor),
      configure_(configuration ? gsl::make_not_null(configuration) : minifi::Configure::create()),
      info_(std::make_unique<StandardProcessorInfo>(processor)) {}

bool ProcessContextImpl::hasNonEmptyProperty(std::string_view name) const {
  auto val = getProcessor().getProperty(name);
  return val && !val->empty();
}

std::vector<std::string> ProcessContextImpl::getDynamicPropertyKeys() const { return processor_.getDynamicPropertyKeys(); }

std::map<std::string, std::string> ProcessContextImpl::getDynamicProperties(const FlowFile* flow_file) const {
  auto dynamic_props = processor_.getDynamicProperties();
  const expression::Parameters params{this, flow_file};
  for (auto& [dynamic_property_name, dynamic_property_value]: dynamic_props) {
    auto cached_dyn_expr_it = cached_dynamic_expressions_.find(dynamic_property_name);
    if (cached_dyn_expr_it == cached_dynamic_expressions_.end()) {
      auto expression = expression::compile(dynamic_property_value);
      const auto [it, success] = cached_dynamic_expressions_.emplace(dynamic_property_name, expression);
      gsl_Assert(success && "getDynamicProperties: no element with the key existed, yet insertion failed");
      cached_dyn_expr_it = it;
    }
    auto& expression = cached_dyn_expr_it->second;
    dynamic_property_value = expression(params).asString();
  }
  return dynamic_props;
}

bool ProcessContextImpl::isAutoTerminated(Relationship relationship) const { return processor_.isAutoTerminated(relationship); }

uint8_t ProcessContextImpl::getMaxConcurrentTasks() const { return processor_.getMaxConcurrentTasks(); }

void ProcessContextImpl::yield() { processor_.yield(); }

nonstd::expected<std::string, std::error_code> ProcessContextImpl::getProperty(const std::string_view name, const FlowFile* flow_file) const {
  const auto property = getProcessorInfo().getSupportedProperty(name);
  if (!property) {
    return nonstd::make_unexpected(PropertyErrorCode::NotSupportedProperty);
  }

  if (!property->supportsExpressionLanguage()) {
    return getProcessor().getProperty(name);
  }
  if (!cached_expressions_.contains(name)) {
    auto expression_str = getProcessor().getProperty(name);
    if (!expression_str) { return expression_str; }
    cached_expressions_.emplace(std::string{name}, expression::compile(*expression_str));
  }
  expression::Parameters p(this, flow_file);
  auto result = cached_expressions_[std::string{name}](p).asString();
  if (!property->getValidator().validate(result)) {
    return nonstd::make_unexpected(PropertyErrorCode::ValidationFailed);
  }
  return result;
}

nonstd::expected<void, std::error_code> ProcessContextImpl::setProperty(const std::string_view name, std::string value) {
  cached_expressions_.erase(std::string{name});
  return getProcessor().setProperty(name, std::move(value));
}

nonstd::expected<void, std::error_code> ProcessContextImpl::clearProperty(const std::string_view name) {
  cached_expressions_.erase(std::string{name});
  return getProcessor().clearProperty(name);
}

nonstd::expected<std::string, std::error_code> ProcessContextImpl::getDynamicProperty(const std::string_view name, const FlowFile* flow_file) const {
  if (!cached_dynamic_expressions_.contains(name)) {
    auto expression_str = getProcessor().getDynamicProperty(name);
    if (!expression_str) { return expression_str; }
    cached_dynamic_expressions_.emplace(std::string{name}, expression::compile(*expression_str));
  }
  const expression::Parameters p(this, flow_file);
  return cached_dynamic_expressions_[std::string{name}](p).asString();
}

nonstd::expected<std::string, std::error_code> ProcessContextImpl::getRawProperty(const std::string_view name) const {
  return getProcessor().getProperty(name);
}

nonstd::expected<std::string, std::error_code> ProcessContextImpl::getRawDynamicProperty(const std::string_view name) const {
  return getProcessor().getDynamicProperty(name);
}

nonstd::expected<void, std::error_code> ProcessContextImpl::setDynamicProperty(std::string name, std::string value) {
  cached_dynamic_expressions_.erase(name);
  return getProcessor().setDynamicProperty(std::move(name), std::move(value));
}

[[nodiscard]] nonstd::expected<std::vector<std::string>, std::error_code> ProcessContextImpl::getAllPropertyValues(std::string_view name) const {
  return getProcessor().getAllPropertyValues(name);
}

void ProcessContextImpl::addAutoTerminatedRelationship(const core::Relationship& relationship) {
  getProcessor().addAutoTerminatedRelationship(relationship);
}

bool ProcessContextImpl::isRunning() const {
  return getProcessor().isRunning();
}

StateManager* ProcessContextImpl::getStateManager() {
  if (state_storage_ == nullptr) { return nullptr; }
  if (!state_manager_) { state_manager_ = state_storage_->getStateManager(processor_); }
  return state_manager_.get();
}

bool ProcessContextImpl::hasIncomingConnections() const {
  return getProcessor().hasIncomingConnections();
}

}  // namespace org::apache::nifi::minifi::core
