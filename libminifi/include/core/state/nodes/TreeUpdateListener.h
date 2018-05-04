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
#ifndef LIBMINIFI_INCLUDE_C2_METRICS_H_
#define LIBMINIFI_INCLUDE_C2_METRICS_H_

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
  MetricsUpdate(UpdateStatus status)
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

  ~OperationWatcher() {

  }

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

class TreeUpdateListener {
 public:
  TreeUpdateListener(const std::shared_ptr<response::NodeReporter> &source, const std::shared_ptr<response::ResponseNodeSink> &sink)
      : running_(true),
        source_(source),
        sink_(sink){

    function_ = [&]() {
      while(running_) {
        std::vector<std::shared_ptr<response::ResponseNode>> metric_vector;
        // simple pass through for the metrics
        if (nullptr != source_ && nullptr != sink_) {
          source_->getResponseNodes(metric_vector,0);
          for(auto metric : metric_vector) {
            sink_->setResponseNodes(metric);
          }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      }
      return MetricsUpdate(UpdateState::READ_COMPLETE);
    };
  }

  void stop() {
    running_ = false;
  }

  std::function<Update()> &getFunction() {
    return function_;
  }

  std::future<Update> &getFuture() {
    return future_;
  }

 private:
  std::function<Update()> function_;
  std::future<Update> future_;
  std::atomic<bool> running_;
  std::shared_ptr<response::NodeReporter> source_;
  std::shared_ptr<response::ResponseNodeSink> sink_;
};

} /* namespace metrics */
} /* namespace state */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_C2_METRICS_H_ */
