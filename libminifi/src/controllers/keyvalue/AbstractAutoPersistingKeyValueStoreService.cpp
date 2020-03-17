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

#include "controllers/keyvalue/AbstractAutoPersistingKeyValueStoreService.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

core::Property AbstractAutoPersistingKeyValueStoreService::AlwaysPersist(
    core::PropertyBuilder::createProperty("Always Persist")->withDescription("Persist every change instead of persisting it periodically.")
        ->isRequired(false)->withDefaultValue<bool>(false)->build());
core::Property AbstractAutoPersistingKeyValueStoreService::AutoPersistenceInterval(
    core::PropertyBuilder::createProperty("Auto Persistence Interval")->withDescription("The interval of the periodic task persisting all values. "
                                                                                        "Only used if Always Persist is false. "
                                                                                        "If set to 0 seconds, auto persistence will be disabled.")
        ->isRequired(false)->withDefaultValue<core::TimePeriodValue>("1 min")->build());

AbstractAutoPersistingKeyValueStoreService::AbstractAutoPersistingKeyValueStoreService(const std::string& name, const std::string& id)
    : KeyValueStoreService(name, id)
    , PersistableKeyValueStoreService(name, id)
    , always_persist_(false)
    , auto_persistence_interval_(0U)
    , running_(false)
    , logger_(logging::LoggerFactory<AbstractAutoPersistingKeyValueStoreService>::getLogger()) {
}

AbstractAutoPersistingKeyValueStoreService::AbstractAutoPersistingKeyValueStoreService(const std::string& name, utils::Identifier uuid /*= utils::Identifier()*/)
    : KeyValueStoreService(name, uuid)
    , PersistableKeyValueStoreService(name, uuid)
    , always_persist_(false)
    , auto_persistence_interval_(0U)
    , running_(false)
    , logger_(logging::LoggerFactory<AbstractAutoPersistingKeyValueStoreService>::getLogger()) {
}

AbstractAutoPersistingKeyValueStoreService::~AbstractAutoPersistingKeyValueStoreService() {
  stopPersistingThread();
}

void AbstractAutoPersistingKeyValueStoreService::stopPersistingThread() {
  std::unique_lock<std::mutex> lock(persisting_mutex_);
  if (persisting_thread_.joinable()) {
    running_ = false;
    persisting_cv_.notify_one();
    lock.unlock();
    persisting_thread_.join();
  }
}

void AbstractAutoPersistingKeyValueStoreService::initialize() {
  ControllerService::initialize();
  std::set<core::Property> supportedProperties;
  supportedProperties.insert(AlwaysPersist);
  supportedProperties.insert(AutoPersistenceInterval);
  updateSupportedProperties(supportedProperties);
}

void AbstractAutoPersistingKeyValueStoreService::onEnable() {
  std::unique_lock<std::mutex> lock(persisting_mutex_);

  if (configuration_ == nullptr) {
    logger_->log_debug("Cannot enable AbstractAutoPersistingKeyValueStoreService");
    return;
  }

  std::string value;
  if (!getProperty(AlwaysPersist.getName(), value)) {
    logger_->log_error("Always Persist attribute is missing or invalid");
  } else {
    utils::StringUtils::StringToBool(value, always_persist_);
  }
  if (!getProperty(AutoPersistenceInterval.getName(), value)) {
    logger_->log_error("Auto Persistence Interval attribute is missing or invalid");
  } else {
    core::TimeUnit unit;
    if (!core::Property::StringToTime(value, auto_persistence_interval_, unit) || !core::Property::ConvertTimeUnitToMS(auto_persistence_interval_, unit, auto_persistence_interval_)) {
      logger_->log_error("Auto Persistence Interval attribute is invalid");
    }
  }

  if (!always_persist_ && auto_persistence_interval_ != 0U) {
    if (!persisting_thread_.joinable()) {
      logger_->log_trace("Starting auto persistence thread");
      running_ = true;
      persisting_thread_ = std::thread(&AbstractAutoPersistingKeyValueStoreService::persistingThreadFunc, this);
    }
  }

  logger_->log_trace("Enabled AbstractAutoPersistingKeyValueStoreService");
}

void AbstractAutoPersistingKeyValueStoreService::notifyStop() {
  stopPersistingThread();
}

void AbstractAutoPersistingKeyValueStoreService::persistingThreadFunc() {
  std::unique_lock<std::mutex> lock(persisting_mutex_);

  while (true) {
    logger_->log_trace("Persisting thread is going to sleep for %d ms", auto_persistence_interval_);
    persisting_cv_.wait_for(lock, std::chrono::milliseconds(auto_persistence_interval_), [this] {
      return !running_;
    });

    if (!running_) {
      logger_->log_trace("Stopping persistence thread");
      return;
    }

    persist();
  }
}

} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
