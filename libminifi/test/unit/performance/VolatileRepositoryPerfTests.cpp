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

#include <benchmark/benchmark.h>

#include "core/repository/LegacyVolatileContentRepository.h"
#include "core/repository/VolatileContentRepository.h"
#include "core/logging/LoggerFactory.h"
#include "core/logging/LoggerConfiguration.h"

static bool initializeLogger = [] {
  auto log_props = std::make_shared<org::apache::nifi::minifi::core::logging::LoggerProperties>();
  log_props->set("logger.root", "OFF");
  org::apache::nifi::minifi::core::logging::LoggerConfiguration::getConfiguration().initialize(log_props);
  return true;
}();

static constexpr int N = 10000;

static void BM_LegacyVolatileContentRepository_Write(benchmark::State& state) {
  auto repo = std::make_shared<org::apache::nifi::minifi::core::repository::LegacyVolatileContentRepository>();
  repo->initialize(nullptr);
  for (auto _ : state) {
    auto session = repo->createSession();
    auto claim = session->create();
    session->write(claim)->write("Lorem ipsum dolor sit amet, consectetur adipiscing elit");
    session->commit();
  }
}
BENCHMARK(BM_LegacyVolatileContentRepository_Write);

static void BM_LegacyVolatileContentRepository_Write2(benchmark::State& state) {
  auto repo = std::make_shared<org::apache::nifi::minifi::core::repository::LegacyVolatileContentRepository>();
  repo->initialize(nullptr);
  for (auto _ : state) {
    auto session = repo->createSession();
    auto claim1 = session->create();
    session->write(claim1)->write("Lorem ipsum dolor sit amet, consectetur adipiscing elit");
    auto claim2 = session->create();
    session->write(claim2)->write("Lorem ipsum dolor sit amet, consectetur adipiscing elit");
    session->commit();
  }
}
BENCHMARK(BM_LegacyVolatileContentRepository_Write2);

static void BM_LegacyVolatileContentRepository_Write3(benchmark::State& state) {
  auto repo = std::make_shared<org::apache::nifi::minifi::core::repository::LegacyVolatileContentRepository>();
  repo->initialize(nullptr);
  {
    auto session = repo->createSession();
    for (int i = 0; i < N; ++i) {
      auto claim = session->create();
      session->write(claim)->write("Lorem ipsum dolor sit amet, consectetur adipiscing elit");
    }
    session->commit();
  }
  for (auto _ : state) {
    auto session = repo->createSession();
    auto claim1 = session->create();
    session->write(claim1)->write("Lorem ipsum dolor sit amet, consectetur adipiscing elit");
    auto claim2 = session->create();
    session->write(claim2)->write("Lorem ipsum dolor sit amet, consectetur adipiscing elit");
    session->commit();
  }
}
BENCHMARK(BM_LegacyVolatileContentRepository_Write3);

static void BM_LegacyVolatileContentRepository_Read(benchmark::State& state) {
  auto repo = std::make_shared<org::apache::nifi::minifi::core::repository::LegacyVolatileContentRepository>();
  repo->initialize(nullptr);
  std::shared_ptr<org::apache::nifi::minifi::ResourceClaim> claim;
  {
    auto session = repo->createSession();
    claim = session->create();
    session->write(claim)->write("Lorem ipsum dolor sit amet, consectetur adipiscing elit");
    session->commit();
  }
  for (auto _ : state) {
    auto session = repo->createSession();
    std::string data;
    session->read(claim)->read(data);
    session->commit();
  }
}
BENCHMARK(BM_LegacyVolatileContentRepository_Read);

static void BM_LegacyVolatileContentRepository_Read2(benchmark::State& state) {
  auto repo = std::make_shared<org::apache::nifi::minifi::core::repository::LegacyVolatileContentRepository>();
  repo->initialize(nullptr);
  std::shared_ptr<org::apache::nifi::minifi::ResourceClaim> claim;
  {
    auto session = repo->createSession();
    claim = session->create();
    session->write(claim)->write("Lorem ipsum dolor sit amet, consectetur adipiscing elit");
    session->commit();
  }
  for (auto _ : state) {
    auto session = repo->createSession();
    std::string data;
    session->read(claim)->read(data);
    session->read(claim)->read(data);
    session->commit();
  }
}
BENCHMARK(BM_LegacyVolatileContentRepository_Read2);

static void BM_LegacyVolatileContentRepository_Read3(benchmark::State& state) {
  auto repo = std::make_shared<org::apache::nifi::minifi::core::repository::LegacyVolatileContentRepository>();
  repo->initialize(nullptr);
  std::shared_ptr<org::apache::nifi::minifi::ResourceClaim> claim;
  {
    auto session = repo->createSession();
    for (int i = 0; i < N; ++i) {
      claim = session->create();
      session->write(claim)->write("Lorem ipsum dolor sit amet, consectetur adipiscing elit");
    }
    session->commit();
  }
  for (auto _ : state) {
    auto session = repo->createSession();
    std::string data;
    session->read(claim)->read(data);
    session->read(claim)->read(data);
    session->commit();
  }
}
BENCHMARK(BM_LegacyVolatileContentRepository_Read3);

static void BM_VolatileContentRepository_Write(benchmark::State& state) {
  auto repo = std::make_shared<org::apache::nifi::minifi::core::repository::VolatileContentRepository>();
  repo->initialize(nullptr);
  for (auto _ : state) {
    auto session = repo->createSession();
    auto claim = session->create();
    session->write(claim)->write("Lorem ipsum dolor sit amet, consectetur adipiscing elit");
    session->commit();
  }
}
BENCHMARK(BM_VolatileContentRepository_Write);

static void BM_VolatileContentRepository_Write2(benchmark::State& state) {
  auto repo = std::make_shared<org::apache::nifi::minifi::core::repository::VolatileContentRepository>();
  repo->initialize(nullptr);
  for (auto _ : state) {
    auto session = repo->createSession();
    auto claim1 = session->create();
    session->write(claim1)->write("Lorem ipsum dolor sit amet, consectetur adipiscing elit");
    auto claim2 = session->create();
    session->write(claim2)->write("Lorem ipsum dolor sit amet, consectetur adipiscing elit");
    session->commit();
  }
}
BENCHMARK(BM_VolatileContentRepository_Write2);

static void BM_VolatileContentRepository_Write3(benchmark::State& state) {
  auto repo = std::make_shared<org::apache::nifi::minifi::core::repository::VolatileContentRepository>();
  repo->initialize(nullptr);
  {
    auto session = repo->createSession();
    for (int i = 0; i < N; ++i) {
      auto claim = session->create();
      session->write(claim)->write("Lorem ipsum dolor sit amet, consectetur adipiscing elit");
    }
    session->commit();
  }
  for (auto _ : state) {
    auto session = repo->createSession();
    auto claim1 = session->create();
    session->write(claim1)->write("Lorem ipsum dolor sit amet, consectetur adipiscing elit");
    auto claim2 = session->create();
    session->write(claim2)->write("Lorem ipsum dolor sit amet, consectetur adipiscing elit");
    session->commit();
  }
}
BENCHMARK(BM_VolatileContentRepository_Write3);

static void BM_VolatileContentRepository_Read(benchmark::State& state) {
  auto repo = std::make_shared<org::apache::nifi::minifi::core::repository::VolatileContentRepository>();
  repo->initialize(nullptr);
  std::shared_ptr<org::apache::nifi::minifi::ResourceClaim> claim;
  {
    auto session = repo->createSession();
    claim = session->create();
    session->write(claim)->write("Lorem ipsum dolor sit amet, consectetur adipiscing elit");
    session->commit();
  }
  for (auto _ : state) {
    auto session = repo->createSession();
    std::string data;
    session->read(claim)->read(data);
    session->commit();
  }
}
BENCHMARK(BM_VolatileContentRepository_Read);

static void BM_VolatileContentRepository_Read2(benchmark::State& state) {
  auto repo = std::make_shared<org::apache::nifi::minifi::core::repository::VolatileContentRepository>();
  repo->initialize(nullptr);
  std::shared_ptr<org::apache::nifi::minifi::ResourceClaim> claim;
  {
    auto session = repo->createSession();
    claim = session->create();
    session->write(claim)->write("Lorem ipsum dolor sit amet, consectetur adipiscing elit");
    session->commit();
  }
  for (auto _ : state) {
    auto session = repo->createSession();
    std::string data;
    session->read(claim)->read(data);
    session->read(claim)->read(data);
    session->commit();
  }
}
BENCHMARK(BM_VolatileContentRepository_Read2);

static void BM_VolatileContentRepository_Read3(benchmark::State& state) {
  auto repo = std::make_shared<org::apache::nifi::minifi::core::repository::VolatileContentRepository>();
  repo->initialize(nullptr);
  std::shared_ptr<org::apache::nifi::minifi::ResourceClaim> claim;
  {
    auto session = repo->createSession();
    for (int i = 0; i < N; ++i) {
      claim = session->create();
      session->write(claim)->write("Lorem ipsum dolor sit amet, consectetur adipiscing elit");
    }
    session->commit();
  }
  for (auto _ : state) {
    auto session = repo->createSession();
    std::string data;
    session->read(claim)->read(data);
    session->read(claim)->read(data);
    session->commit();
  }
}
BENCHMARK(BM_VolatileContentRepository_Read3);

BENCHMARK_MAIN();