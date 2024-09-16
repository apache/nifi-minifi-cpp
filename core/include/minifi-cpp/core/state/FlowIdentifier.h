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
#ifndef LIBMINIFI_INCLUDE_CORE_STATE_FLOWIDENTIFIER_H_
#define LIBMINIFI_INCLUDE_CORE_STATE_FLOWIDENTIFIER_H_

#include <string>


namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {

/**
 * Purpose: Represents a flow identifier for a given flow update or instance.
 *
 * Design: Immutable collection of strings for the component parts.
 */
class FlowIdentifier {
 public:
  FlowIdentifier() = delete;

  /**
   * Constructor accepts the url, bucket id, and flow id.
   */
  explicit FlowIdentifier(const std::string &url, const std::string &bucket_id, const std::string &flow_id) {
    registry_url_ = url;
    bucket_id_ = bucket_id;
    flow_id_ = flow_id;
  }

  /**
   * In most cases the lock guard isn't necessary for these getters; however,
   * we don't want to cause issues if the FlowVersion object is ever used in a way
   * that breaks the current paradigm.
   */
  std::string getRegistryUrl() const {
    return registry_url_;
  }

  std::string getBucketId() const {
    return bucket_id_;
  }

  std::string getFlowId() const {
    return flow_id_;
  }

 protected:
  FlowIdentifier(const FlowIdentifier &other) = default;
  FlowIdentifier &operator=(const FlowIdentifier &other) = default;

 private:
  std::string registry_url_;
  std::string bucket_id_;
  std::string flow_id_;
  friend class FlowVersion;
};


}  // namespace state
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_STATE_FLOWIDENTIFIER_H_
