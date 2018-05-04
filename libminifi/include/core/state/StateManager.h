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
#ifndef LIBMINIFI_INCLUDE_STATE_MANAGER_H
#define LIBMINIFI_INCLUDE_STATE_MANAGER_H

#include <map>
#include <atomic>
#include <algorithm>

#include "UpdateController.h"
#include "io/validation.h"
#include "utils/ThreadPool.h"
#include "core/Core.h"
#include "nodes/MetricsBase.h"
#include "nodes/TreeUpdateListener.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {

/**
 * State manager is meant to be used by implementing agents of this library. It represents the source and sink for metrics,
 * the sink for external updates, and encapsulates the thread pool that runs the listeners for various update operations
 * that can be performed.
 */
class StateManager : public response::NodeReporter, public response::ResponseNodeSink, public StateMonitor, public std::enable_shared_from_this<StateManager> {
 public:

  StateManager()
      : metrics_listener_(nullptr) {

  }

  virtual ~StateManager() {

  }

  /**
   * Initializes the thread pools.
   */
  void initialize();

  /**
   * State management operations.
   */
  /**
   * Stop this controllable.
   * @param force force stopping
   * @param timeToWait time to wait before giving up.
   * @return status of stopping this controller.
   */
  virtual int16_t stop(bool force, uint64_t timeToWait = 0);

  /**
   * Updates the given flow controller.
   */
  int16_t update(const std::shared_ptr<Update> &updateController);

  /**
   * Passes metrics to the update controllers if they are a metrics sink.
   * @param metrics metric to pass through
   */
  int16_t setResponseNodes(const std::shared_ptr<response::ResponseNode> &metrics);

  /**
   * Metrics operations
   */
  virtual int16_t getResponseNodes(std::vector<std::shared_ptr<response::ResponseNode>> &metric_vector, uint16_t metricsClass);

  virtual std::string getVersion(){
    return "";
  }

 protected:

  void shutdownState(){
    listener_thread_pool_.shutdown();
    metrics_maps_.clear();
    updateControllers.clear();
  }

  /**
   * Function to apply updates for a given  update controller.
   * @param source  source identifier
   * @param updateController update controller mechanism.
   */
  virtual int16_t applyUpdate(const std::string &source, const std::shared_ptr<Update> &updateController) = 0;

  /**
   * Registers and update controller
   * @param updateController update controller to add.
   */
  bool registerUpdateListener(const std::shared_ptr<UpdateController> &updateController, const int64_t &delay);


  /**
   * Base metrics function will employ the default metrics listener.
   */
  virtual bool startMetrics(const int64_t &delay);

 private:

  std::timed_mutex mutex_;

  std::map<std::string, std::shared_ptr<response::ResponseNode>> metrics_maps_;

  std::vector<std::shared_ptr<UpdateController> > updateControllers;

  std::unique_ptr<state::response::TreeUpdateListener> metrics_listener_;

  utils::ThreadPool<Update> listener_thread_pool_;

};



} /* namespace state */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_C2_CONTROLLABLE_H_ */
