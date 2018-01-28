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

#include "core/state/StateManager.h"
#include <memory>
#include <utility>
#include <vector>

#include "core/state/nodes/MetricsBase.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {

void StateManager::initialize() {
  metrics_listener_ = std::unique_ptr<state::response::TreeUpdateListener>(new state::response::TreeUpdateListener(shared_from_this(), shared_from_this()));
  // manually add the c2 agent for now
  listener_thread_pool_.setMaxConcurrentTasks(2);
  listener_thread_pool_.start();
  controller_running_ = true;
}
/**
 * State management operations.
 */
int16_t StateManager::stop(bool force, uint64_t timeToWait) {
  controller_running_ = false;
  listener_thread_pool_.shutdown();
  return 1;
}

int16_t StateManager::update(const std::shared_ptr<Update> &updateController) {
  // must be stopped to update.
  if (isStateMonitorRunning()) {
    return -1;
  }
  int16_t ret = applyUpdate("StateManager", updateController);
  switch (ret) {
    case -1:
      return -1;
    default:
      return 1;
  }
}

/**
 * Passes metrics to the update controllers if they are a metrics sink.
 * @param metrics metric to pass through
 */
int16_t StateManager::setResponseNodes(const std::shared_ptr<response::ResponseNode> &metrics) {
  if (IsNullOrEmpty(metrics)) {
    return -1;
  }
  auto now = std::chrono::steady_clock::now();
  if (mutex_.try_lock_until(now + std::chrono::milliseconds(100))) {
    // update controllers can be metric sinks too
    for (auto controller : updateControllers) {
      std::shared_ptr<response::ResponseNodeSink> sink = std::dynamic_pointer_cast<response::ResponseNodeSink>(controller);
      if (sink != nullptr) {
        sink->setResponseNodes(metrics);
      }
    }
    metrics_maps_[metrics->getName()] = metrics;
    mutex_.unlock();
  } else {
    return -1;
  }
  return 0;
}
/**
 * Metrics operations
 */
int16_t StateManager::getResponseNodes(std::vector<std::shared_ptr<response::ResponseNode>> &metric_vector, uint16_t metricsClass) {
  auto now = std::chrono::steady_clock::now();
  const std::chrono::steady_clock::time_point wait_time = now + std::chrono::milliseconds(100);
  if (mutex_.try_lock_until(wait_time)) {
    for (auto metric : metrics_maps_) {
      metric_vector.push_back(metric.second);
    }
    mutex_.unlock();
    return 0;
  }
  return -1;
}

bool StateManager::registerUpdateListener(const std::shared_ptr<UpdateController> &updateController, const int64_t &delay) {
  auto functions = updateController->getFunctions();

  updateControllers.push_back(updateController);
  // run all functions independently

  for (auto function : functions) {
    std::unique_ptr<utils::AfterExecute<Update>> after_execute = std::unique_ptr<utils::AfterExecute<Update>>(new UpdateRunner(isStateMonitorRunning(), delay));
    utils::Worker<Update> functor(function, "listeners", std::move(after_execute));
    std::future<Update> future;
    if (!listener_thread_pool_.execute(std::move(functor), future)) {
      // denote failure
      return false;
    }
  }
  return true;
}

/**
 * Base metrics function will employ the default metrics listener.
 */
bool StateManager::startMetrics(const int64_t &delay) {
  std::unique_ptr<utils::AfterExecute<Update>> after_execute = std::unique_ptr<utils::AfterExecute<Update>>(new UpdateRunner(isStateMonitorRunning(), delay));
  utils::Worker<Update> functor(metrics_listener_->getFunction(), "metrics", std::move(after_execute));
  if (!listener_thread_pool_.execute(std::move(functor), metrics_listener_->getFuture())) {
    // denote failure
    return false;
  }
  return true;
}

} /* namespace state */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
