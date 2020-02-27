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
#ifndef LIBMINIFI_INCLUDE_CORE_STATE_NODES_REPOSITORYMETRICS_H_
#define LIBMINIFI_INCLUDE_CORE_STATE_NODES_REPOSITORYMETRICS_H_

#include <sstream>
#include <map>

#include "../nodes/MetricsBase.h"
#include "Connection.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {
namespace response {

/**
 * Justification and Purpose: Provides repository metrics. Provides critical information to the
 * C2 server.
 *
 */
class RepositoryMetrics : public ResponseNode {
 public:

  RepositoryMetrics(const std::string &name, utils::Identifier &uuid)
      : ResponseNode(name, uuid) {
  }

  RepositoryMetrics(const std::string &name)
      : ResponseNode(name) {
  }

  RepositoryMetrics()
      : ResponseNode("RepositoryMetrics") {
  }

  virtual std::string getName() const {
    return "RepositoryMetrics";
  }

  void addRepository(const std::shared_ptr<core::Repository> &repo) {
    if (nullptr != repo) {
      repositories.insert(std::make_pair(repo->getName(), repo));
    }
  }

  std::vector<SerializedResponseNode> serialize() {
    std::vector<SerializedResponseNode> serialized;
    for (auto conn : repositories) {
      auto repo = conn.second;
      SerializedResponseNode parent;
      parent.name = repo->getName();
      SerializedResponseNode datasize;
      datasize.name = "running";
      datasize.value = repo->isRunning();

      SerializedResponseNode datasizemax;
      datasizemax.name = "full";
      datasizemax.value = repo->isFull();

      SerializedResponseNode queuesize;
      queuesize.name = "size";
      queuesize.value = std::to_string(repo->getRepoSize());

      parent.children.push_back(datasize);
      parent.children.push_back(datasizemax);
      parent.children.push_back(queuesize);

      serialized.push_back(parent);
    }
    return serialized;
  }

 protected:
  std::map<std::string, std::shared_ptr<core::Repository>> repositories;
};

} /* namespace metrics */
} /* namespace state */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_STATE_METRICS_RepositoryMetrics_H_ */
