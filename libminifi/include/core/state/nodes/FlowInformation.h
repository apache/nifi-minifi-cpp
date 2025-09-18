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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/state/FlowIdentifier.h"
#include "minifi-cpp/core/state/nodes/MetricsBase.h"
#include "core/state/nodes/StateMonitor.h"
#include "minifi-cpp/Connection.h"
#include "core/state/ConnectionStore.h"
#include "core/Processor.h"
#include "core/BulletinStore.h"

namespace org::apache::nifi::minifi::state::response {

class FlowVersion : public DeviceInformation {
 public:
  FlowVersion()
      : DeviceInformation("FlowVersion") {
    setFlowVersion("", "", getUUIDStr());
  }

  explicit FlowVersion(const std::string &registry_url, const std::string &bucket_id, const std::string &flow_id)
      : DeviceInformation("FlowVersion") {
    setFlowVersion(registry_url, bucket_id, flow_id.empty() ? getUUIDStr() : flow_id);
  }

  FlowVersion(FlowVersion &&fv) noexcept
      : DeviceInformation("FlowVersion"),
        identifier(std::move(fv.identifier)) {
  }

  std::string getName() const override {
    return "FlowVersion";
  }

  std::shared_ptr<state::FlowIdentifier> getFlowIdentifier() const {
    std::lock_guard<std::mutex> lock(guard);
    return identifier;
  }
  /**
   * In most cases the lock guard isn't necessary for these getters; however,
   * we don't want to cause issues if the FlowVersion object is ever used in a way
   * that breaks the current paradigm.
   */
  std::string getRegistryUrl() {
    std::lock_guard<std::mutex> lock(guard);
    return identifier->getRegistryUrl();
  }

  std::string getBucketId() {
    std::lock_guard<std::mutex> lock(guard);
    return identifier->getBucketId();
  }

  std::string getFlowId() {
    std::lock_guard<std::mutex> lock(guard);
    return identifier->getFlowId();
  }

  void setFlowVersion(const std::string &url, const std::string &bucket_id, const std::string &flow_id) {
    std::lock_guard<std::mutex> lock(guard);
    identifier = std::make_shared<FlowIdentifierImpl>(url, bucket_id, flow_id.empty() ? utils::IdGenerator::getIdGenerator()->generate().to_string() : flow_id);
  }

  std::vector<SerializedResponseNode> serialize() override;

  FlowVersion &operator=(FlowVersion &&fv) noexcept {
    identifier = std::move(fv.identifier);
    return *this;
  }

 protected:
  mutable std::mutex guard;

  std::shared_ptr<FlowIdentifier> identifier;
};

class FlowInformation : public StateMonitorNode {
 public:
  FlowInformation(std::string_view name, const utils::Identifier &uuid)
      : StateMonitorNode(name, uuid) {
  }

  explicit FlowInformation(std::string_view name)
      : StateMonitorNode(name) {
  }

  MINIFIAPI static constexpr const char* Description = "Metric node that defines the flow ID and flow URL deployed to this agent";

  std::string getName() const override {
    return "flowInfo";
  }

  void setFlowVersion(std::shared_ptr<state::response::FlowVersion> flow_version) {
    flow_version_ = std::move(flow_version);
  }

  void updateConnection(minifi::Connection* connection) {
    connection_store_.updateConnection(connection);
  }

  void setProcessors(std::vector<core::Processor*> processors) {
    processors_ = std::move(processors);
  }

  void setBulletinStore(core::BulletinStore* bulletin_store) {
    bulletin_store_ = bulletin_store;
  }

  std::vector<SerializedResponseNode> serialize() override;
  std::vector<PublishedMetric> calculateMetrics() override;

 private:
  std::shared_ptr<state::response::FlowVersion> flow_version_;
  ConnectionStore connection_store_;
  std::vector<core::Processor*> processors_;
  core::BulletinStore* bulletin_store_ = nullptr;
};

}  // namespace org::apache::nifi::minifi::state::response
