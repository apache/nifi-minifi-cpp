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

#include "utils/ResourceQueue.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "core/logging/LoggerFactory.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::utils::testing {

TEST_CASE("Limiting resource queue to a maximum of 2 resources", "[utils::ResourceQueue]") {
  using std::chrono::steady_clock;

  std::shared_ptr<core::logging::Logger> logger{core::logging::LoggerFactory<ResourceQueue<steady_clock::time_point>>::getLogger()};

  LogTestController::getInstance().setTrace<ResourceQueue<steady_clock::time_point>>();

  std::mutex resources_created_mutex;
  std::set<steady_clock::time_point> resources_created;

  auto worker = [&](const std::shared_ptr<ResourceQueue<steady_clock::time_point>>& resource_queue) {
    auto resource = resource_queue->getResource();
    std::this_thread::sleep_for(10ms);
    std::lock_guard<std::mutex> lock(resources_created_mutex);
    resources_created.emplace(*resource);
  };

  auto resource_queue = utils::ResourceQueue<steady_clock::time_point>::create([]{ return std::make_unique<steady_clock::time_point>(steady_clock::now()); }, 2, std::nullopt, logger);
  std::thread thread_one{[&] { worker(resource_queue); }};
  std::thread thread_two{[&] { worker(resource_queue); }};
  std::thread thread_three{[&] { worker(resource_queue); }};

  thread_one.join();
  thread_two.join();
  thread_three.join();

  CHECK(!resources_created.empty());
  CHECK(resources_created.size() <= 2);
}

TEST_CASE("Resource limitation is not set to the resource queue", "[utils::ResourceQueue]") {
  using std::chrono::steady_clock;

  std::shared_ptr<core::logging::Logger> logger{core::logging::LoggerFactory<ResourceQueue<steady_clock::time_point>>::getLogger()};
  LogTestController::getInstance().setTrace<ResourceQueue<steady_clock::time_point>>();
  LogTestController::getInstance().clear();
  auto resource_queue = utils::ResourceQueue<steady_clock::time_point>::create([]{ return std::make_unique<steady_clock::time_point>(steady_clock::now()); }, std::nullopt, std::nullopt, logger);
  std::set<steady_clock::time_point> resources_created;

  auto resource_wrapper_one = resource_queue->getResource();
  auto resource_wrapper_two = resource_queue->getResource();
  auto resource_wrapper_three = resource_queue->getResource();

  resources_created.emplace(*resource_wrapper_one);
  resources_created.emplace(*resource_wrapper_two);
  resources_created.emplace(*resource_wrapper_three);

  CHECK(!LogTestController::getInstance().contains("Waiting for resource", 0ms));
  CHECK(LogTestController::getInstance().contains("Number of instances: 3", 0ms));
  CHECK(resources_created.size() == 3);
}

TEST_CASE("resource returns when it goes out of scope", "[utils::ResourceQueue]") {
  using std::chrono::steady_clock;
  auto queue = utils::ResourceQueue<steady_clock::time_point>::create([]{ return std::make_unique<steady_clock::time_point>(steady_clock::time_point::min()); });
  {
    auto resource = queue->getResource();
    CHECK(*resource == steady_clock::time_point::min());
    *resource = steady_clock::now();
  }
  {
    auto resource = queue->getResource();
    CHECK(*resource != steady_clock::time_point::min());
  }
}

TEST_CASE("resource resets when it goes out of scope", "[utils::ResourceQueue]") {
  using std::chrono::steady_clock;
  std::shared_ptr<core::logging::Logger> logger{core::logging::LoggerFactory<ResourceQueue<steady_clock::time_point>>::getLogger()};
  LogTestController::getInstance().setTrace<ResourceQueue<steady_clock::time_point>>();
  LogTestController::getInstance().clear();
  auto queue = utils::ResourceQueue<steady_clock::time_point>::create([]{ return std::make_unique<steady_clock::time_point>(steady_clock::time_point::min()); },
                                                                      std::nullopt,
                                                                      [](steady_clock::time_point& resource){ resource = steady_clock::time_point::min();},
                                                                      logger);
  {
    auto resource = queue->getResource();
    CHECK(*resource == steady_clock::time_point::min());
    *resource = steady_clock::now();
  }
  {
    CHECK(LogTestController::getInstance().matchesRegex("Returning .* resource", 0ms));
    auto resource = queue->getResource();
    CHECK(*resource == steady_clock::time_point::min());
    CHECK(LogTestController::getInstance().matchesRegex("Using available .* resource instance", 0ms));
  }
}

TEST_CASE("queue destroyed before resource", "[utils::ResourceQueue]") {
  using std::chrono::steady_clock;
  auto queue = utils::ResourceQueue<steady_clock::time_point>::create([]{ return std::make_unique<steady_clock::time_point>(steady_clock::time_point::min()); });
  auto resource = queue->getResource();
  REQUIRE_NOTHROW(queue.reset());
  REQUIRE_NOTHROW(*resource);
}
}  // namespace org::apache::nifi::minifi::utils::testing
