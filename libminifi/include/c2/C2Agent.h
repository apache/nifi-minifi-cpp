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

#pragma once

#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "../core/state/nodes/MetricsBase.h"
#include "../core/state/Value.h"
#include "core/state/UpdateController.h"
#include "controllers/UpdatePolicyControllerService.h"
#include "C2Payload.h"
#include "C2Trigger.h"
#include "C2Protocol.h"
#include "io/validation.h"
#include "HeartbeatReporter.h"
#include "utils/Id.h"
#include "utils/MinifiConcurrentQueue.h"
#include "utils/ThreadPool.h"
#include "utils/file/FileSystem.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

namespace c2 {

/**
 * Purpose and Justification: C2 agent will be the mechanism that will abstract the protocol to do the work.
 *
 * The protocol represents a transformation layer into the objects seen in C2Payload. That transformation may
 * be minimal or extreme, depending on the protocol itself.
 *
 * Metrics Classes defined here:
 *
 *   0 HeartBeat --  RESERVED
 *   1-255 Defined by the configuration file.
 */
class C2Agent : public state::UpdateController {
 public:
  static constexpr const char* UPDATE_NAME = "C2UpdatePolicy";

  C2Agent(core::controller::ControllerServiceProvider *controller,
          state::Pausable *pause_handler,
          state::StateMonitor* updateSink,
          std::shared_ptr<Configure> configuration,
          std::shared_ptr<utils::file::FileSystem> filesystem,
          std::function<void()> request_restart);

  ~C2Agent() noexcept override {
    delete protocol_.load();
  }

  void start() override;

  void stop() override;

  /**
   * Sends the heartbeat to ths server. Will include metrics
   * in the payload if they exist.
   */
  void performHeartBeat();

  std::optional<std::string> fetchFlow(const std::string& uri) const;

 protected:
  /**
   * Check the collection of triggers for any updates that need to be handled.
   * This is an optional step
   */
  void checkTriggers();

  void configure(const std::shared_ptr<Configure> &configure, bool reconfigure = true);

  /**
   * Serializes metrics into a payload.
   * @parem parent_paylaod parent payload into which we insert the newly generated payload.
   * @param name name of this metric
   * @param metrics metrics to include.
   */
  void serializeMetrics(C2Payload &metric_payload, const std::string &name, const std::vector<state::response::SerializedResponseNode> &metrics, bool is_container = false, bool is_collapsible = true);

  /**
   * Extract the payload
   * @param resp payload to be moved into the function.
   */
  void extractPayload(const C2Payload &resp);

  /**
   * Enqueues a C2 server response for us to evaluate and parse.
   */
  void enqueue_c2_server_response(C2Payload &&resp);

  /**
   * Enqueues a c2 payload for a response to the C2 server.
   */
  void enqueue_c2_response(C2Payload &&resp) {
    requests.enqueue(std::move(resp));
  }

  /**
   * Handles a C2 event requested by the server.
   * @param resp c2 server response.
   */
  virtual void handle_c2_server_response(const C2ContentResponse &resp);

  void handle_clear(const C2ContentResponse &resp);

  /**
   * Handles an update request
   * @param C2ContentResponse response
   */
  void handle_update(const C2ContentResponse &resp);

  /**
   * Handles a description request
   */
  void handle_describe(const C2ContentResponse &resp);


  enum class UpdateResult {
    NO_UPDATE,
    UPDATE_SUCCESSFUL,
    UPDATE_FAILED
  };
  /**
   * Updates a property
   */
  UpdateResult update_property(const std::string &property_name, const std::string &property_value,  PropertyChangeLifetime lifetime);

  void handle_transfer(const C2ContentResponse &resp);

  C2Payload bundleDebugInfo(std::map<std::string, std::unique_ptr<io::InputStream>>& files);

  /**
   * Creates configuration options C2 payload for response
   */
  C2Payload prepareConfigurationOptions(const C2ContentResponse &resp) const;

 private:
  utils::TaskRescheduleInfo produce();
  utils::TaskRescheduleInfo consume();

  bool handleConfigurationUpdate(const C2ContentResponse &resp);
  void handlePropertyUpdate(const C2ContentResponse &resp);
  void handleAssetUpdate(const C2ContentResponse &resp);

  std::optional<std::string> resolveFlowUrl(const std::string& url) const;
  std::optional<std::string> resolveUrl(const std::string& url) const;
  static std::optional<std::string> getFlowIdFromConfigUpdate(const C2ContentResponse &resp);

 protected:
  std::timed_mutex metrics_mutex_;
  std::map<std::string, std::shared_ptr<state::response::ResponseNode>> metrics_map_;

  /**
   * Device information stored in the metrics format
   */
  std::map<std::string, std::shared_ptr<state::response::ResponseNode>> root_response_nodes_;

  /**
   * Device information stored in the metrics format
   */
  std::map<std::string, std::shared_ptr<state::response::ResponseNode>> device_information_;

  // responses for the the C2 agent.
  utils::ConcurrentQueue<C2Payload> responses;

  // requests that originate from the C2 server.
  utils::ConcurrentQueue<C2Payload> requests;

  // heart beat period.
  std::chrono::milliseconds heart_beat_period_;

  // maximum number of queued messages to send to the c2 server
  size_t max_c2_responses;

  // time point the last time we performed a heartbeat.
  std::chrono::steady_clock::time_point last_run_;

  // function that performs the heartbeat
  std::function<utils::TaskRescheduleInfo()> c2_producer_;

  // reference to the update sink, against which we will execute updates.
  state::StateMonitor* update_sink_;

  // functions that will be used for the udpate controller.
  std::vector<std::function<utils::TaskRescheduleInfo()>> functions_;

  std::shared_ptr<controllers::UpdatePolicyControllerService> update_service_;

  // controller service provider reference.
  core::controller::ControllerServiceProvider* controller_;

  state::Pausable* pause_handler_;

  // shared pointer to the configuration of this agent
  std::shared_ptr<Configure> configuration_;

  std::shared_ptr<utils::file::FileSystem> filesystem_;

  std::mutex heartbeat_mutex;

  std::vector<std::unique_ptr<HeartbeatReporter>> heartbeat_protocols_;

  std::vector<std::unique_ptr<C2Trigger>> triggers_;

  std::atomic<C2Protocol*> protocol_;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<C2Agent>::getLogger();

  utils::ThreadPool<utils::TaskRescheduleInfo> thread_pool_;

  std::vector<utils::Identifier> task_ids_;

  bool manifest_sent_;

  const uint64_t C2RESPONSE_POLL_MS = 100;

  std::atomic<bool> restart_needed_ = false;
  std::function<void()> request_restart_;
};

}  // namespace c2
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

