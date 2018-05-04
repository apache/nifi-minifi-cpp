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

#include "core/Resource.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <functional>
#include <sys/ioctl.h>
#if ( defined(__APPLE__) || defined(__MACH__) || defined(BSD)) 
#include <net/if_dl.h>
#include <net/if_types.h>
#endif
#include <ifaddrs.h>
#include <net/if.h> 
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sstream>
#include <map>
#include "../nodes/MetricsBase.h"
#include "Connection.h"
#include "io/ClientSocket.h"
#include "../nodes/StateMonitor.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {
namespace response {

class FlowVersion : public DeviceInformation {
 public:

  explicit FlowVersion()
      : DeviceInformation("FlowVersion", nullptr) {
  }

  explicit FlowVersion(const std::string &registry_url, const std::string &bucket_id, const std::string &flow_id)
      : DeviceInformation("FlowVersion", nullptr),
        registry_url_(registry_url),
        bucket_id_(bucket_id),
        flow_id_(flow_id) {
    setFlowVersion(registry_url_, bucket_id_, flow_id_);
  }

  explicit FlowVersion(FlowVersion &&fv)
      : DeviceInformation("FlowVersion", nullptr),
        registry_url_(std::move(fv.registry_url_)),
        bucket_id_(std::move(fv.bucket_id_)),
        flow_id_(std::move(fv.flow_id_)) {
    setFlowVersion(registry_url_, bucket_id_, flow_id_);
  }

  std::string getName() const {
    return "FlowVersion";
  }

  std::string getRegistryUrl() const {
    return registry_url_;
  }

  std::string getBucketId() const {
    return bucket_id_;
  }

  std::string getFlowId() const {
    return flow_id_.empty() ? getUUIDStr() : flow_id_;
  }

  void setFlowVersion(const std::string &url, const std::string &bucket_id, const std::string &flow_id) {
    registry_url_ = url;
    bucket_id_ = bucket_id;
    flow_id_ = flow_id;
  }

  std::vector<SerializedResponseNode> serialize() {
    std::lock_guard<std::mutex> lock_guard(guard);

    std::vector<SerializedResponseNode> serialized;
    SerializedResponseNode ru;
    ru.name = "registryUrl";
    ru.value = registry_url_;

    SerializedResponseNode bucketid;
    bucketid.name = "bucketId";
    bucketid.value = bucket_id_;

    SerializedResponseNode flowId;
    flowId.name = "flowId";
    flowId.value = getFlowId();

    serialized.push_back(ru);
    serialized.push_back(bucketid);
    serialized.push_back(flowId);
    return serialized;
  }

  FlowVersion &operator=(const FlowVersion &&fv) {
    registry_url_ = (std::move(fv.registry_url_));
    bucket_id_ = (std::move(fv.bucket_id_));
    flow_id_ = (std::move(fv.flow_id_));
    setFlowVersion(registry_url_, bucket_id_, flow_id_);
    return *this;
  }
 protected:

  std::mutex guard;

  std::string registry_url_;
  std::string bucket_id_;
  std::string flow_id_;
};

class FlowMonitor : public StateMonitorNode {
 public:

  FlowMonitor(std::string name, uuid_t uuid)
      : StateMonitorNode(name, uuid) {
  }

  FlowMonitor(const std::string &name)
      : StateMonitorNode(name, 0) {
  }

  void addConnection(const std::shared_ptr<minifi::Connection> &connection) {
    if (nullptr != connection) {
      connections_.insert(std::make_pair(connection->getName(), connection));
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

  FlowInformation(std::string name, uuid_t uuid)
      : FlowMonitor(name, uuid) {
  }

  FlowInformation(const std::string &name)
      : FlowMonitor(name, 0) {
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
      SerializedResponseNode queues;

      queues.name = "queues";

      for (auto &queue : connections_) {
        SerializedResponseNode repoNode;
        repoNode.name = queue.first;

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

        queues.children.push_back(repoNode);

      }
      serialized.push_back(queues);
    }

    if (nullptr != monitor_) {
      auto components = monitor_->getAllComponents();
      SerializedResponseNode componentsNode;

      componentsNode.name = "components";

      for (auto component : components) {
        SerializedResponseNode componentNode;

        componentNode.name = component->getComponentName();

        SerializedResponseNode componentStatusNode;
        componentStatusNode.name = "running";
        componentStatusNode.value = component->isRunning();

        componentNode.children.push_back(componentStatusNode);

        componentsNode.children.push_back(componentNode);
      }
      serialized.push_back(componentsNode);
    }

    return serialized;
  }

 protected:

};

REGISTER_RESOURCE(FlowInformation);

} /* namespace metrics */
} /* namespace state */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_STATE_NODES_FLOWINFORMATION_H_ */
