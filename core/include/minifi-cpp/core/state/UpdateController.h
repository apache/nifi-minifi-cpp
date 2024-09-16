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
#ifndef LIBMINIFI_INCLUDE_CORE_STATE_UPDATECONTROLLER_H_
#define LIBMINIFI_INCLUDE_CORE_STATE_UPDATECONTROLLER_H_

#include <memory>
#include <utility>
#include <vector>
#include <string>
#include <map>
#include "utils/ThreadPool.h"
#include "utils/BackTrace.h"
#include "io/InputStream.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {

enum class UpdateState {
  INITIATE,
  FULLY_APPLIED,
  READ_COMPLETE,
  PARTIALLY_APPLIED,
  NOT_APPLIED,
  SET_ERROR,
  READ_ERROR,
  NESTED,  // multiple updates embedded into one
  NO_OPERATION
};

/**
 * Represents the status of an update operation.
 *
 */
class UpdateStatus {
 public:
  UpdateStatus(UpdateState state, int16_t reason = 0); // NOLINT

  UpdateStatus(const UpdateStatus &other) = default;

  UpdateStatus(UpdateStatus &&other) = default;

  UpdateState getState() const;

  std::string getError() const;

  int16_t getReadonCode() const;

  UpdateStatus &operator=(UpdateStatus &&other) = default;

  UpdateStatus &operator=(const UpdateStatus &other) = default;
 private:
  UpdateState state_;
  std::string error_;
  int16_t reason_;
};

class Update {
 public:
  Update()
      : status_(UpdateState::INITIATE, 0) {
  }

  Update(UpdateStatus status) // NOLINT
      : status_(std::move(status)) {
  }

  Update(const Update &other) = default;

  Update(Update &&other) = default;

  virtual ~Update() = default;

  virtual bool validate() {
    return true;
  }

  const UpdateStatus &getStatus() const {
    return status_;
  }

  Update &operator=(Update &&other) = default;

  Update &operator=(const Update &other) = default;

 protected:
  UpdateStatus status_;
};

class Pausable {
 public:
  virtual ~Pausable() = default;

  virtual int16_t pause() = 0;

  virtual int16_t resume() = 0;
};

class StateController : public Pausable {
 public:
  ~StateController() override = default;

  [[nodiscard]] virtual std::string getComponentName() const = 0;

  [[nodiscard]] virtual utils::Identifier getComponentUUID() const = 0;
  /**
   * Start the client
   */
  virtual int16_t start() = 0;
  /**
   * Stop the client
   */
  virtual int16_t stop() = 0;

  virtual bool isRunning() const = 0;
};

/**
 * Justification and Purpose: Update sink is an abstract class with most functions being purely virtual.
 * This is meant to reflect the operational sink for the state of the client. Functions, below, represent
 * a small and tight interface for operations that can be performed from external controllers.
 *
 */
class StateMonitor : public StateController {
 public:
  ~StateMonitor() override = default;

  // Execute callback func on the component. Thread safe, locking mutex_, preventing concurrent flow update
  virtual void executeOnComponent(const std::string &id_or_name, std::function<void(state::StateController&)> func) = 0;

  // Execute callback func on the all components. Thread safe, locking mutex_, preventing concurrent flow update
  virtual void executeOnAllComponents(std::function<void(state::StateController&)> func) = 0;

  /**
   * Drain repositories
   */
  virtual int16_t drainRepositories() = 0;

  /**
   * Clear connection for the agent.
   */
  virtual int16_t clearConnection(const std::string &connection) = 0;

  virtual std::vector<std::string> getSupportedConfigurationFormats() const = 0;

  /**
   * Apply an update with the provided string.
   *
   * < 0 is an error code
   * 0 is success
   */
  virtual int16_t applyUpdate(const std::string & source, const std::string &configuration, bool persist = false, const std::optional<std::string>& flow_id = std::nullopt) = 0;

  /**
   * Apply an update that the agent must decode. This is useful for certain operations
   * that can't be encapsulated within these definitions.
   */
  virtual int16_t applyUpdate(const std::string &source, const std::shared_ptr<Update> &updateController) = 0;

  /**
   * Returns uptime for this module.
   * @return uptime for the current state monitor.
   */
  virtual uint64_t getUptime() = 0;

  /**
   * Returns a vector of backtraces
   * @return backtraces from the state monitor.
   */
  virtual std::vector<BackTrace> getTraces() = 0;

  virtual std::map<std::string, std::unique_ptr<io::InputStream>> getDebugInfo() = 0;

 protected:
  std::atomic<bool> controller_running_;
};

/**
 * Asks: what is being updated, what can be updated without a restart
 * what requires a restart, etc.
 */
class UpdateController {
 public:
  UpdateController()
      : controller_running_(false) {
  }

  virtual ~UpdateController() = default;

  virtual std::vector<std::function<utils::TaskRescheduleInfo()>> getFunctions() {
    return {};
  }

  virtual void start() = 0;

  virtual void stop() = 0;

  std::atomic<bool>& isControllerRunning() {
    return controller_running_;
  }
 protected:
  std::atomic<bool> controller_running_;
};

}  // namespace state
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_STATE_UPDATECONTROLLER_H_
