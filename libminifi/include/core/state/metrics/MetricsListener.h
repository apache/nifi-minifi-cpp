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

#include "MetricsBase.h"
#include "core/state/UpdateController.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {
namespace metrics {

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

class MetricsWatcher : public utils::AfterExecute<Update> {
 public:
  explicit MetricsWatcher(std::atomic<bool> *running)
      : running_(running) {
  }

  explicit MetricsWatcher(MetricsWatcher && other)
      : running_(std::move(other.running_)) {

  }

  ~MetricsWatcher() {

  }

  virtual bool isFinished(const Update &result) {
    if (result.getStatus().getState() == UpdateState::READ_COMPLETE && running_) {
      return false;
    } else {
      return true;
    }
  }
  virtual bool isCancelled(const UpdateStatus &result) {
    return false;
  }

 protected:
  std::atomic<bool> *running_;

};

class MetricsListener {
 public:
  MetricsListener(const std::shared_ptr<metrics::MetricsReporter> &source, const std::shared_ptr<metrics::MetricsSink> &sink)
      : running_(true),
        sink_(sink),
        source_(source) {

    function_ = [&]() {
      while(running_) {
        std::vector<std::shared_ptr<metrics::Metrics>> metric_vector;
        // simple pass through for the metrics
        if (nullptr != source_ && nullptr != sink_) {
          source_->getMetrics(metric_vector,0);
          for(auto metric : metric_vector) {
            sink_->setMetrics(metric);
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
  std::shared_ptr<metrics::MetricsReporter> source_;
  std::shared_ptr<metrics::MetricsSink> sink_;
};

} /* namespace metrics */
} /* namespace state */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_C2_METRICS_H_ */
