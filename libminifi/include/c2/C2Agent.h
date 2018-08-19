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
#ifndef LIBMINIFI_INCLUDE_C2_C2AGENT_H_
#define LIBMINIFI_INCLUDE_C2_C2AGENT_H_

#include <utility>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

#include "../core/state/nodes/MetricsBase.h"
#include "../core/state/Value.h"
#include "core/state/UpdateController.h"
#include "controllers/UpdatePolicyControllerService.h"
#include "C2Payload.h"
#include "C2Trigger.h"
#include "C2Protocol.h"
#include "io/validation.h"
#include "HeartBeatReporter.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

#define C2_AGENT_UPDATE_NAME "C2UpdatePolicy"
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
class C2Agent : public state::UpdateController, public state::response::ResponseNodeSink, public std::enable_shared_from_this<C2Agent> {
 public:

  C2Agent(const std::shared_ptr<core::controller::ControllerServiceProvider> &controller, const std::shared_ptr<state::StateMonitor> &updateSink, const std::shared_ptr<Configure> &configure);

  virtual ~C2Agent() {
  }

  /**
   * Sends the heartbeat to ths server. Will include metrics
   * in the payload if they exist.
   */
  void performHeartBeat();

  virtual std::vector<std::function<state::Update()>> getFunctions() {
    return functions_;
  }

  /**
   * Sets the metric within this sink
   * @param metric metric to set
   * @param return 0 on success, -1 on failure.
   */
  virtual int16_t setResponseNodes(const std::shared_ptr<state::response::ResponseNode> &metric);

  /**
    * Sets the metric within this sink
    * @param metric metric to set
    * @param return 0 on success, -1 on failure.
    */
   virtual int16_t setMetricsNodes(const std::shared_ptr<state::response::ResponseNode> &metric);

  int64_t getHeartBeatDelay(){
    std::lock_guard<std::mutex> lock(heartbeat_mutex);
    return heart_beat_period_;
  }

 protected:

  /**
   * Restarts this agent.
   */
  void restart_agent();

  /**
   * Update agent per the provided C2 update from c2 server or triggers
   */
  void update_agent();

  /**
   * Check the collection of triggers for any updates that need to be handled.
   * This is an optional step
   */
  void checkTriggers();

  /**
   * Configure the C2 agent
   */
  void configure(const std::shared_ptr<Configure> &configure, bool reconfigure = true);

  /**
   * Serializes metrics into a payload.
   * @parem parent_paylaod parent payload into which we insert the newly generated payload.
   * @param name name of this metric
   * @param metrics metrics to include.
   */
  void serializeMetrics(C2Payload &parent_payload, const std::string &name, const std::vector<state::response::SerializedResponseNode> &metrics, bool is_container = false, bool is_collapsible = true);

  /**
   * Extract the payload
   * @param resp payload to be moved into the function.
   */
  void extractPayload(const C2Payload &&resp);

  /**
   * Extract the payload
   * @param payload reference.
   */
  void extractPayload(const C2Payload &resp);

  /**
   * Enqueues a C2 server response for us to evaluate and parse.
   */
  void enqueue_c2_server_response(C2Payload &&resp) {
    std::lock_guard<std::timed_mutex> lock(queue_mutex);
    responses.push_back(std::move(resp));
  }

  /**
   * Enqueues a c2 payload for a response to the C2 server.
   */
  void enqueue_c2_response(C2Payload &&resp) {
    std::lock_guard<std::timed_mutex> lock(request_mutex);
    requests.push_back(std::move(resp));
  }

  /**
   * Handles a C2 event requested by the server.
   * @param resp c2 server response.
   */
  virtual void handle_c2_server_response(const C2ContentResponse &resp);

  /**
   * Handles an update request
   * @param C2ContentResponse response
   */
  void handle_update(const C2ContentResponse &resp);

  /**
   * Handles a description request
   */
  void handle_describe(const C2ContentResponse &resp);

  /**
   * Updates a property
   */
  bool update_property(const std::string &property_name, const std::string &property_value,  bool persist = false);

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
  // queue mutex
  std::timed_mutex queue_mutex;

  // queue mutex
  std::timed_mutex request_mutex;

  // responses for the the C2 agent.
  std::vector<C2Payload> responses;

  // requests that originate from the C2 server.
  std::vector<C2Payload> requests;

  // heart beat period.
  int64_t heart_beat_period_;

  // maximum number of queued messages to send to the c2 server
  int16_t max_c2_responses;

  // time point the last time we performed a heartbeat.
  std::chrono::steady_clock::time_point last_run_;

  // function that performs the heartbeat
  std::function<state::Update()> c2_producer_;

  // function that acts upon the
  std::function<state::Update()> c2_consumer_;

  // reference to the update sink, against which we will execute updates.
  std::shared_ptr<state::StateMonitor> update_sink_;

  // functions that will be used for the udpate controller.
  std::vector<std::function<state::Update()>> functions_;

  std::shared_ptr<controllers::UpdatePolicyControllerService> update_service_;

  // controller service provider reference.
  std::shared_ptr<core::controller::ControllerServiceProvider> controller_;

  // shared pointer to the configuration of this agent
  std::shared_ptr<Configure> configuration_;

  // shared pointer to the running c2 configuration.
  std::shared_ptr<Configure> running_c2_configuration;

  std::mutex heartbeat_mutex;

  std::vector<std::shared_ptr<HeartBeatReporter>> heartbeat_protocols_;

  std::vector<std::shared_ptr<C2Trigger>> triggers_;

  std::atomic<C2Protocol*> protocol_;

  bool allow_updates_;

  std::string update_command_;

  std::string update_location_;

  std::string bin_location_;

  std::shared_ptr<logging::Logger> logger_;
};

} /* namesapce c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_C2_C2AGENT_H_ */
