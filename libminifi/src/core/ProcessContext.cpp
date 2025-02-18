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

#include "core/ProcessContext.h"
#include "core/Processor.h"

namespace org::apache::nifi::minifi::core {

namespace {

class StandardProcessorInfo : public ProcessorInfo {
 public:
  explicit StandardProcessorInfo(Processor& proc): proc_(proc) {}

  std::string getName() const override {return proc_.getName();}
  utils::Identifier getUUID() const override {return proc_.getUUID();}
  std::shared_ptr<state::FlowIdentifier> getFlowIdentifier() const override {return proc_.getFlowIdentifier();}
  std::map<std::string, core::Property> getSupportedProperties() const override {return proc_.getSupportedProperties();}
  nonstd::expected<PropertyReference, std::error_code> getPropertyReference(std::string_view name) const override {return proc_.getPropertyReference(name);}

 private:
  Processor& proc_;
};

}  // namespace

ProcessContextImpl::ProcessContextImpl(
    Processor& processor, controller::ControllerServiceProvider* controller_service_provider, const std::shared_ptr<core::Repository>& repo,
    const std::shared_ptr<core::Repository>& flow_repo, const std::shared_ptr<core::ContentRepository>& content_repo)
    : VariableRegistryImpl(Configure::create()),
      controller_service_provider_(controller_service_provider),
      flow_repo_(flow_repo),
      content_repo_(content_repo),
      processor_(processor),
      logger_(logging::LoggerFactory<ProcessContext>::getLogger()),
      configure_(minifi::Configure::create()),
      initialized_(false) {
  repo_ = repo;
  state_storage_ = getStateStorage(logger_, controller_service_provider_, nullptr);
}

ProcessContextImpl::ProcessContextImpl(
    Processor& processor, controller::ControllerServiceProvider* controller_service_provider, const std::shared_ptr<core::Repository>& repo,
    const std::shared_ptr<core::Repository>& flow_repo, const std::shared_ptr<minifi::Configure>& configuration,
    const std::shared_ptr<core::ContentRepository>& content_repo)
    : VariableRegistryImpl(configuration),
      controller_service_provider_(controller_service_provider),
      flow_repo_(flow_repo),
      content_repo_(content_repo),
      processor_(processor),
      logger_(logging::LoggerFactory<ProcessContext>::getLogger()),
      configure_(configuration),
      initialized_(false) {
  repo_ = repo;
  state_storage_ = getStateStorage(logger_, controller_service_provider_, configuration);
  if (!configure_) { configure_ = minifi::Configure::create(); }
  info_ = std::make_unique<StandardProcessorInfo>(processor);
}

std::vector<std::string> ProcessContextImpl::getDynamicPropertyKeys() const { return processor_.getDynamicPropertyKeys(); }

std::map<std::string, std::string> ProcessContextImpl::getDynamicProperties(const FlowFile*) const { return processor_.getDynamicProperties(); }

bool ProcessContextImpl::isAutoTerminated(Relationship relationship) const { return processor_.isAutoTerminated(relationship); }

uint8_t ProcessContextImpl::getMaxConcurrentTasks() const { return processor_.getMaxConcurrentTasks(); }

void ProcessContextImpl::yield() { processor_.yield(); }

nonstd::expected<std::string, std::error_code> ProcessContextImpl::getProperty(const std::string_view name, const FlowFile* const) const {
  return getProcessor().getProperty(name);
}

nonstd::expected<void, std::error_code> ProcessContextImpl::setProperty(const std::string_view name, std::string value) {
  return getProcessor().setProperty(name, std::move(value));
}

nonstd::expected<void, std::error_code> ProcessContextImpl::clearProperty(const std::string_view name) {
  return getProcessor().clearProperty(name);
}

nonstd::expected<std::string, std::error_code> ProcessContextImpl::getDynamicProperty(const std::string_view name, const FlowFile* const) const {
  return getProcessor().getDynamicProperty(name);
}

nonstd::expected<void, std::error_code> ProcessContextImpl::setDynamicProperty(std::string name, std::string value) {
  return getProcessor().setDynamicProperty(std::move(name), std::move(value));
}

StateManager* ProcessContextImpl::getStateManager() {
  if (state_storage_ == nullptr) { return nullptr; }
  if (!state_manager_) { state_manager_ = state_storage_->getStateManager(processor_); }
  return state_manager_.get();
}

}  // namespace org::apache::nifi::minifi::core
