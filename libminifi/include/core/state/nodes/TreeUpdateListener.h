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
#ifndef LIBMINIFI_INCLUDE_CORE_STATE_NODES_TREEUPDATELISTENER_H_
#define LIBMINIFI_INCLUDE_CORE_STATE_NODES_TREEUPDATELISTENER_H_

#include <memory>
#include <utility>
#include <vector>

#include "../nodes/MetricsBase.h"
#include "core/state/UpdateController.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {
namespace response {

/**
 * Purpose: Class that will represent the metrics updates, which can be performed asynchronously.
 */
class MetricsUpdate : public Update {
 public:
  MetricsUpdate(UpdateStatus status) // NOLINT
      : Update(status) {
  }
  virtual bool validate() {
    return true;
  }
};

class OperationWatcher : public utils::AfterExecute<Update> {
 public:
  explicit OperationWatcher(std::atomic<bool> *running)
      : running_(running) {
  }

  explicit OperationWatcher(OperationWatcher && other)
      : running_(std::move(other.running_)) {
  }

  ~OperationWatcher() = default;

  virtual bool isFinished(const Update &result) {
    if (result.getStatus().getState() == UpdateState::READ_COMPLETE && running_) {
      return false;
    } else {
      return true;
    }
  }
  virtual bool isCancelled(const Update &result) {
    return false;
  }

 protected:
  std::atomic<bool> *running_;
};

}  // namespace response
}  // namespace state
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_STATE_NODES_TREEUPDATELISTENER_H_
