/**
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

#include <atomic>
#include <memory>
#include <string>
#include <utility>

#include "c2/C2Trigger.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "c2/C2Payload.h"
#include "properties/Configure.h"

namespace org::apache::nifi::minifi::c2 {

/**
 * Purpose: Defines a file update trigger when the last write time of a file has been changed.
 * Design: Extends C2Trigger, and implements a trigger, action, reset state machine. Calling
 * triggered will check the file.
 */
class FileUpdateTrigger final : public C2Trigger {
 public:
  MINIFIAPI static constexpr const char* Description = "Defines a file update trigger when the last write time of a file has been changed.";

  explicit FileUpdateTrigger(const std::string_view name, const utils::Identifier &uuid = {})
      : C2Trigger(name, uuid),
        update_(false) {
  }

  void initialize(const std::shared_ptr<minifi::Configure> &configuration) override {
    if (nullptr != configuration) {
      if (configuration->get(minifi::Configure::nifi_c2_file_watch, "c2.file.watch", file_)) {
        setLastUpdate(utils::file::last_write_time(file_));
      } else {
        logger_->log_trace("Could not configure file");
      }
    }
  }

  bool triggered() override {
    if (!getLastUpdate().has_value()) {
      logger_->log_trace("Last Update is zero");
      return false;
    }
    auto update_time = utils::file::last_write_time(file_);
    auto last_update_l = getLastUpdate().value().time_since_epoch().count();
    logger_->log_trace("Last Update is {} and update time is {}", last_update_l , update_time.value().time_since_epoch().count());
    if (update_time > getLastUpdate()) {
      setLastUpdate(update_time);
      update_ = true;
      return true;
    }
    return false;
  }


  void reset() override {
    setLastUpdate(utils::file::last_write_time(file_));
    update_ = false;
  }

  /**
   * Returns an update payload implementing a C2 action
   */
  C2Payload getAction() override;

  /**
   * Determines if we are connected and operating
   */
  bool isRunning() const override {
    return true;
  }

  /**
   * Block until work is available on any input connection, or the given duration elapses
   * @param timeoutMs timeout in milliseconds
   */

  void yield() override {
  }

  /**
   * Determines if work is available by this connectable
   * @return boolean if work is available.
   */
  bool isWorkAvailable() override {
    return true;
  }

  std::optional<std::chrono::file_clock::time_point> getLastUpdate() const;

  void setLastUpdate(const std::optional<std::chrono::file_clock::time_point> &last_update);

 protected:
  std::string file_;
  std::atomic<bool> update_;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<FileUpdateTrigger>::getLogger();
  mutable std::mutex last_update_lock;
  std::optional<std::chrono::file_clock::time_point> last_update_;
};

}  // namespace org::apache::nifi::minifi::c2
