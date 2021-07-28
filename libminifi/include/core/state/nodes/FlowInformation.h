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
#ifndef LIBMINIFI_INCLUDE_CORE_STATE_NODES_FLOWINFORMATION_H_
#define LIBMINIFI_INCLUDE_CORE_STATE_NODES_FLOWINFORMATION_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#if ( defined(__APPLE__) || defined(__MACH__) || defined(BSD))
#include <net/if_dl.h>
#include <net/if_types.h>

#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <sstream>

#include "../FlowIdentifier.h"
#include "../nodes/MetricsBase.h"
#include "../nodes/StateMonitor.h"
#include "Connection.h"
#include "io/ClientSocket.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {
namespace response {

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

  explicit FlowVersion(FlowVersion &&fv)
      : DeviceInformation("FlowVersion"),
        identifier(std::move(fv.identifier)) {
  }

  std::string getName() const {
    return "FlowVersion";
  }

  virtual std::shared_ptr<state::FlowIdentifier> getFlowIdentifier() const {
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
    identifier = std::make_shared<FlowIdentifier>(url, bucket_id, flow_id);
  }

  std::vector<SerializedResponseNode> serialize() {
    std::lock_guard<std::mutex> lock(guard);
    std::vector<SerializedResponseNode> serialized;
    SerializedResponseNode ru;
    ru.name = "registryUrl";
    ru.value = identifier->getRegistryUrl();

    SerializedResponseNode bucketid;
    bucketid.name = "bucketId";
    bucketid.value = identifier->getBucketId();

    SerializedResponseNode flowId;
    flowId.name = "flowId";
    flowId.value = identifier->getFlowId();

    serialized.push_back(ru);
    serialized.push_back(bucketid);
    serialized.push_back(flowId);
    return serialized;
  }

  FlowVersion &operator=(const FlowVersion &&fv) {
    identifier = std::move(fv.identifier);
    return *this;
  }

 protected:
  mutable std::mutex guard;

  std::shared_ptr<FlowIdentifier> identifier;
};

class FlowMonitor : public StateMonitorNode {
 public:
  FlowMonitor(const std::string &name, const utils::Identifier &uuid)
      : StateMonitorNode(name, uuid) {
  }

  FlowMonitor(const std::string &name) // NOLINT
      : StateMonitorNode(name) {
  }

  void addConnection(const std::shared_ptr<minifi::Connection> &connection) {
    if (nullptr != connection) {
      connections_.insert(std::make_pair(connection->getUUIDStr(), connection));
    }
  }

  void setFlowVersion(std::shared_ptr<state::response::FlowVersion> flow_version) {
    flow_version_ = flow_version;
  }
 protected:
  std::shared_ptr<state::response::FlowVersion> flow_version_;
  std::map<std::string, std::shared_ptr<minifi::Connection>> connections_;
};

/**
 * Justification and Purpose: Provides flow version Information
 */
class FlowInformation : public FlowMonitor {
 public:
  FlowInformation(const std::string &name, const utils::Identifier &uuid)
      : FlowMonitor(name, uuid) {
  }

  FlowInformation(const std::string &name) // NOLINT
      : FlowMonitor(name) {
  }

  std::string getName() const {
    return "flowInfo";
  }

  std::vector<SerializedResponseNode> serialize() {
    std::vector<SerializedResponseNode> serialized;

    SerializedResponseNode fv;
    fv.name = "flowId";
    fv.value = flow_version_->getFlowId();

    SerializedResponseNode uri;
    uri.name = "versionedFlowSnapshotURI";
    for (auto &entry : flow_version_->serialize()) {
      uri.children.push_back(entry);
    }

    serialized.push_back(fv);
    serialized.push_back(uri);

    if (!connections_.empty()) {
      SerializedResponseNode queues(false);
      queues.name = "queues";

      for (auto &queue : connections_) {
        SerializedResponseNode repoNode(false);
        repoNode.name = queue.second->getName();

        SerializedResponseNode queueUUIDNode;
        queueUUIDNode.name = "uuid";
        queueUUIDNode.value = std::string{queue.second->getUUIDStr()};

        SerializedResponseNode queuesize;
        queuesize.name = "size";
        queuesize.value = queue.second->getQueueSize();

        SerializedResponseNode queuesizemax;
        queuesizemax.name = "sizeMax";
        queuesizemax.value = queue.second->getMaxQueueSize();

        SerializedResponseNode datasize;
        datasize.name = "dataSize";
        datasize.value = queue.second->getQueueDataSize();
        SerializedResponseNode datasizemax;

        datasizemax.name = "dataSizeMax";
        datasizemax.value = queue.second->getMaxQueueDataSize();

        repoNode.children.push_back(queuesize);
        repoNode.children.push_back(queuesizemax);
        repoNode.children.push_back(datasize);
        repoNode.children.push_back(datasizemax);
        repoNode.children.push_back(queueUUIDNode);

        queues.children.push_back(repoNode);
      }
      serialized.push_back(queues);
    }

    if (nullptr != monitor_) {
      auto components = monitor_->getAllComponents();
      SerializedResponseNode componentsNode(false);
      componentsNode.name = "components";

      for (auto component : components) {
        SerializedResponseNode componentNode(false);
        componentNode.name = component->getComponentName();

        SerializedResponseNode uuidNode;
        uuidNode.name = "uuid";
        uuidNode.value = std::string{component->getComponentUUID().to_string()};

        SerializedResponseNode componentStatusNode;
        componentStatusNode.name = "running";
        componentStatusNode.value = component->isRunning();

        componentNode.children.push_back(componentStatusNode);
        componentNode.children.push_back(uuidNode);
        componentsNode.children.push_back(componentNode);
      }
      serialized.push_back(componentsNode);
    }

    return serialized;
  }

 protected:
};

}  // namespace response
}  // namespace state
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_STATE_NODES_FLOWINFORMATION_H_
