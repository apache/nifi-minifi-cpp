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
#ifndef LIBMINIFI_INCLUDE_C2_TRIGGERS_FILESYSTEMTRIGGER_H_
#define LIBMINIFI_INCLUDE_C2_TRIGGERS_FILESYSTEMTRIGGER_H_
#include <atomic>
#include "c2/C2Trigger.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "core/Resource.h"
#include "c2/C2Payload.h"
#include "properties/Configure.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

/**
 * Defines a file update trigger when the last write time of a file has been changed.
 */
class FileUpdateTrigger : public C2Trigger {
 public:

  FileUpdateTrigger(std::string name, utils::Identifier uuid = utils::Identifier())
      : C2Trigger(name, uuid),
        last_update_(0),
        update_(false) {
  }

  void initialize(const std::shared_ptr<minifi::Configure> &configuration) {
    if (nullptr != configuration) {
      if (configuration->get(minifi::Configure::nifi_c2_file_watch, file_)) {
        last_update_ = utils::file::FileUtils::last_write_time(file_);
      }

    }
  }


  virtual bool triggered() {
    if (last_update_ == -1)
      return false;
    auto update_time = utils::file::FileUtils::last_write_time(file_);
    if (update_time > last_update_) {
      last_update_ = update_time;
      update_ = true;
      return true;
    }
    return false;
  }

  /**
   * Returns an update payload implementing a C2 action
   */
  virtual C2Payload getAction();

  /**
   * Determines if we are connected and operating
   */
  virtual bool isRunning() {
    return true;
  }

  /**
   * Block until work is available on any input connection, or the given duration elapses
   * @param timeoutMs timeout in milliseconds
   */

  virtual void yield() {

  }

  /**
   * Determines if work is available by this connectable
   * @return boolean if work is available.
   */
  virtual bool isWorkAvailable() {
    return true;
  }

 protected:
  std::mutex mutex_;
  std::string file_;
  std::atomic<uint64_t> last_update_;
  std::atomic<bool> update_;
};
REGISTER_RESOURCE(FileUpdateTrigger)

} /* namesapce c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_C2_TRIGGERS_FILESYSTEMTRIGGER_H_ */
