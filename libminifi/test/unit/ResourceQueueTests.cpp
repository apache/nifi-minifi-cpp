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

#include <chrono>
#include <set>

#include "ResourceQueue.h"
#include "../TestBase.h"
#include "../Catch.h"
#include "logging/LoggerConfiguration.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::utils::testing {

TEST_CASE("Limiting resource queue to a maximum of 2 resources", "[utils::ResourceQueue]") {
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<ResourceQueue<int>>::getLogger()};
  LogTestController::getInstance().setTrace<ResourceQueue<int>>();

  std::mutex resources_created_mutex;
  std::set<int> resources_created;

  auto worker = [&](int value, const std::shared_ptr<ResourceQueue<int>>& resource_queue) {
    auto resource = resource_queue->getResource([value]{return std::make_unique<int>(value);});
    std::this_thread::sleep_for(10ms);
    std::lock_guard<std::mutex> lock(resources_created_mutex);
    resources_created.emplace(*resource);
  };

  auto resource_queue = ResourceQueue<int>::create(2, logger_);
  std::thread thread_one{[&] { worker(1, resource_queue); }};
  std::thread thread_two{[&] { worker(2, resource_queue); }};
  std::thread thread_three{[&] { worker(3, resource_queue); }};

  thread_one.join();
  thread_two.join();
  thread_three.join();

  CHECK(!resources_created.empty());
  CHECK(resources_created.size() <= 2);
}

TEST_CASE("Resource limitation is not set to the resource queue", "[utils::ResourceQueue]") {
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<ResourceQueue<int>>::getLogger()};
  LogTestController::getInstance().setTrace<ResourceQueue<int>>();
  LogTestController::getInstance().clear();
  auto resource_queue = ResourceQueue<int>::create(std::nullopt, logger_);
  std::set<int> resources_created;

  auto resource_wrapper_one = resource_queue->getResource([]{return std::make_unique<int>(1);});
  auto resource_wrapper_two = resource_queue->getResource([]{return std::make_unique<int>(2);});
  auto resource_wrapper_three = resource_queue->getResource([]{return std::make_unique<int>(3);});

  resources_created.emplace(*resource_wrapper_one);
  resources_created.emplace(*resource_wrapper_two);
  resources_created.emplace(*resource_wrapper_three);

  CHECK(!LogTestController::getInstance().contains("Waiting for resource", 0ms));
  CHECK(LogTestController::getInstance().contains("Number of instances: 3", 0ms));
  CHECK(resources_created.size() == 3);
}

TEST_CASE("resource returns when it goes out of scope", "[utils::ResourceQueue]") {
  auto queue = utils::ResourceQueue<int>::create(std::nullopt, nullptr);
  {
    auto resource = queue->getResource([] { return std::make_unique<int>(1); });
    CHECK(*resource == 1);
  }
  {
    auto resource = queue->getResource([] { return std::make_unique<int>(2); });
    CHECK(*resource == 1);
  }
}

TEST_CASE("queue destroyed before resource", "[utils::ResourceQueue]") {
  auto queue = utils::ResourceQueue<int>::create(std::nullopt, nullptr);
  auto resource = queue->getResource([]{ return std::make_unique<int>(1); });
  REQUIRE_NOTHROW(queue.reset());
}
}  // namespace org::apache::nifi::minifi::utils::testing
