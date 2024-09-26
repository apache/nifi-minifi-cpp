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

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <functional>
#include <mutex>
#include <condition_variable>

#include "core/logging/Logger.h"
#include "concurrentqueue.h"
#include "utils/MinifiConcurrentQueue.h"

namespace org::apache::nifi::minifi::utils {

/*
 * utils::ResourceQueue a threadsafe resource pool that lends out existing resources or creates them if necessary.
 * getResource will return an existing unused resource or use the create_resource function to create one.
 * If the number of existing resources reached the maximum_number_of_creatable_resources_, the call will block until a resource is returned to the queue.
 * The lent out resource is in a ResourceWrapper that returns the resource to the queue on destruction.
 * */

template<class ResourceType>
class ResourceQueue : public utils::EnableSharedFromThis {
 public:
  class ResourceWrapper {
   public:
    ResourceWrapper(std::weak_ptr<ResourceQueue<ResourceType>> queue, std::unique_ptr<ResourceType> resource) : queue_(std::move(queue)), resource_(std::move(resource)) {}
    ResourceWrapper(ResourceWrapper&& src) = default;
    ResourceWrapper(const ResourceWrapper&) = delete;
    ~ResourceWrapper() {
      if (auto queue = queue_.lock())
        queue->returnResource(std::move(resource_));
    }

    ResourceWrapper& operator=(ResourceWrapper&&) = default;
    ResourceWrapper& operator=(const ResourceWrapper&) = delete;

    ResourceType& operator*() const { return *resource_; }
    ResourceType* operator->() const noexcept { return resource_.operator->(); }
    ResourceType* get() const { return resource_.get(); }

   private:
    std::weak_ptr<ResourceQueue<ResourceType>> queue_;
    std::unique_ptr<ResourceType> resource_;
  };

  static auto create(std::function<std::unique_ptr<ResourceType>()> creator,
                     std::optional<size_t> maximum_number_of_creatable_resources = std::nullopt,
                     std::optional<std::function<void(ResourceType&)>> reset_fn = std::nullopt,
                     std::shared_ptr<core::logging::Logger> logger = nullptr);

  [[nodiscard]] ResourceWrapper getResource() {
    std::unique_ptr<ResourceType> resource;
    // Use an existing resource, if one is available
    if (internal_queue_.tryDequeue(resource)) {
      logDebug("Using available [{}] resource instance", static_cast<void*>(resource.get()));
      return ResourceWrapper(sharedFromThis<ResourceQueue<ResourceType>>(), std::move(resource));
    } else {
      const std::lock_guard<std::mutex> lock(counter_mutex_);
      if (!maximum_number_of_creatable_resources_ || resources_created_ < maximum_number_of_creatable_resources_) {
        ++resources_created_;
        resource = create_new_resource_();
        logDebug("Created new [{}] resource instance. Number of instances: {}{}.",
                 static_cast<void*>(resource.get()),
                 resources_created_,
                 maximum_number_of_creatable_resources_ ? " / " + std::to_string(*maximum_number_of_creatable_resources_) : "");
        return ResourceWrapper(sharedFromThis<ResourceQueue<ResourceType>>(), std::move(resource));
      }
    }
    logDebug("Waiting for resource");
    if (!internal_queue_.dequeueWait(resource)) {
      throw std::runtime_error("No resource available");
    }
    return ResourceWrapper(sharedFromThis<ResourceQueue<ResourceType>>(), std::move(resource));
  }

 protected:
  ResourceQueue(std::function<std::unique_ptr<ResourceType>()> create_new_resource,
                std::optional<size_t> maximum_number_of_creatable_resources,
                std::optional<std::function<void(ResourceType&)>> reset_fn,
                std::shared_ptr<core::logging::Logger> logger)
      : create_new_resource_(std::move(create_new_resource)),
        maximum_number_of_creatable_resources_(maximum_number_of_creatable_resources),
        reset_fn_(std::move(reset_fn)),
        logger_(std::move(logger)) {
  }

 private:
  void returnResource(std::unique_ptr<ResourceType> resource) {
    logDebug("Returning [{}] resource", static_cast<void*>(resource.get()));
    if (reset_fn_)
      reset_fn_.value()(*resource);
    internal_queue_.enqueue(std::move(resource));
  }

  template<typename ...Args>
  void logDebug(core::logging::log_format_string<Args...> fmt, Args&& ...args) {
    if (logger_)
      logger_->log_debug(fmt, std::forward<Args>(args)...);
  }

  const std::function<std::unique_ptr<ResourceType>()> create_new_resource_;
  const std::optional<size_t> maximum_number_of_creatable_resources_;
  const std::optional<std::function<void(ResourceType&)>> reset_fn_;
  const std::shared_ptr<core::logging::Logger> logger_;
  ConditionConcurrentQueue<std::unique_ptr<ResourceType>> internal_queue_;
  size_t resources_created_ = 0;
  std::mutex counter_mutex_;
  struct make_shared_enabler;
};

template<class ResourceType>
struct ResourceQueue<ResourceType>::make_shared_enabler : public ResourceQueue<ResourceType> {
  template<typename... Args>
  make_shared_enabler(Args&& ... args) : ResourceQueue<ResourceType>(std::forward<Args>(args)...) {}
};

template<class ResourceType>
auto ResourceQueue<ResourceType>::create(std::function<std::unique_ptr<ResourceType>()> creator,
                                         std::optional<size_t> maximum_number_of_creatable_resources,
                                         std::optional<std::function<void(ResourceType&)>> reset_fn,
                                         std::shared_ptr<core::logging::Logger> logger) {
  return std::make_shared<make_shared_enabler>(std::move(creator), maximum_number_of_creatable_resources, std::move(reset_fn), std::move(logger));
}
}  // namespace org::apache::nifi::minifi::utils
