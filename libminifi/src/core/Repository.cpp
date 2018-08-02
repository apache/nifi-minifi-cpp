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
#include "core/Repository.h"
#include <cstdint>
#include <vector>

#include "io/DataStream.h"
#include "io/Serializable.h"
#include "core/Relationship.h"
#include "core/logging/Logger.h"
#include "FlowController.h"
#include "provenance/Provenance.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

void Repository::start() {
  if (this->purge_period_ <= 0)
    return;
  if (running_)
    return;
  running_ = true;
  thread_ = std::thread(&Repository::threadExecutor, this);
  logger_->log_debug("%s Repository Monitor Thread Start", name_);
}

void Repository::stop() {
  if (!running_)
    return;
  running_ = false;
  if (thread_.joinable())
    thread_.join();
  logger_->log_debug("%s Repository Monitor Thread Stop", name_);
}

// repoSize
uint64_t Repository::getRepoSize() {
  return repo_size_;
}

void Repository::flush() {
}

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
