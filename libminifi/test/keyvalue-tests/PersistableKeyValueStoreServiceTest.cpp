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

#define CATCH_CONFIG_RUNNER
#include <vector>
#include <memory>
#include <string>

#include "catch.hpp"

#include "../TestBase.h"
#include "core/controller/ControllerService.h"
#include "core/ProcessGroup.h"
#include "core/yaml/YamlConfiguration.h"
#include "controllers/keyvalue/PersistableKeyValueStoreService.h"
#include "controllers/keyvalue/AbstractAutoPersistingKeyValueStoreService.h"

static std::string config_yaml; // NOLINT

int main(int argc, char* argv[]) {
  Catch::Session session;

  auto cli = session.cli()
      | Catch::clara::Opt{config_yaml, "config-yaml"}
          ["--config-yaml"]
          ("path to the config.yaml containing the PersistableKeyValueStoreService controller service configuration");
  session.cli(cli);

  int ret = session.applyCommandLine(argc, argv);
  if (ret != 0) {
    return ret;
  }

  if (config_yaml.empty()) {
    std::cerr << "Missing --config-yaml <path>. It must contain the path to the config.yaml containing the PersistableKeyValueStoreService controller service configuration." << std::endl;
    return -1;
  }

  return session.run();
}

class PersistableKeyValueStoreServiceTestsFixture {
 public:
  PersistableKeyValueStoreServiceTestsFixture() {
    LogTestController::getInstance().setTrace<TestPlan>();
    LogTestController::getInstance().setTrace<minifi::controllers::PersistableKeyValueStoreService>();
    LogTestController::getInstance().setTrace<minifi::controllers::AbstractAutoPersistingKeyValueStoreService>();

    // Create temporary directories
    char format[] = "/var/tmp/state.XXXXXX";
    state_dir = testController.createTempDirectory(format);
    REQUIRE(false == state_dir.empty());
#ifdef WIN32
    REQUIRE(0 == _chdir(state_dir.c_str()));
#else
    REQUIRE(0 == chdir(state_dir.c_str()));
#endif
    loadYaml();
  }

  virtual ~PersistableKeyValueStoreServiceTestsFixture() {
    LogTestController::getInstance().reset();
  }

  void loadYaml() {
    controller.reset();
    persistable_key_value_store_service_node.reset();

    process_group.reset();
    yaml_config.reset();

    stream_factory.reset();
    content_repo.reset();
    test_flow_repo.reset();
    test_repo.reset();
    configuration.reset();

    configuration = std::make_shared<minifi::Configure>();
    test_repo = std::make_shared<TestRepository>();
    test_flow_repo = std::make_shared<TestFlowRepository>();

    configuration->set(minifi::Configure::nifi_flow_configuration_file, config_yaml);

    content_repo = std::make_shared<core::repository::VolatileContentRepository>();
    content_repo->initialize(configuration);
    stream_factory = minifi::io::StreamFactory::getInstance(configuration);

    yaml_config = std::unique_ptr<core::YamlConfiguration>(new core::YamlConfiguration(test_repo, test_repo, content_repo, stream_factory, configuration, config_yaml));

    process_group = yaml_config->getRoot();
    persistable_key_value_store_service_node = process_group->findControllerService("testcontroller");
    REQUIRE(persistable_key_value_store_service_node != nullptr);
    persistable_key_value_store_service_node->enable();

    controller = std::dynamic_pointer_cast<minifi::controllers::PersistableKeyValueStoreService>(
        persistable_key_value_store_service_node->getControllerServiceImplementation());
    REQUIRE(controller != nullptr);
  }

 protected:
  std::string state_dir;
  std::shared_ptr<minifi::Configure> configuration;
  std::shared_ptr<core::Repository> test_repo;
  std::shared_ptr<core::Repository> test_flow_repo;
  std::shared_ptr<core::ContentRepository> content_repo;
  std::shared_ptr<minifi::io::StreamFactory> stream_factory;

  std::unique_ptr<core::YamlConfiguration> yaml_config;
  std::unique_ptr<core::ProcessGroup> process_group;

  std::shared_ptr<core::controller::ControllerServiceNode> persistable_key_value_store_service_node;
  std::shared_ptr<minifi::controllers::PersistableKeyValueStoreService> controller;

  TestController testController;
};


TEST_CASE_METHOD(PersistableKeyValueStoreServiceTestsFixture, "PersistableKeyValueStoreServiceTestsFixture set and get", "[basic]") {
  const char* key = "foobar";
  const char* value = "234";
  REQUIRE(true == controller->set(key, value));

  SECTION("without persistence") {
  }
  SECTION("with persistence") {
    controller->persist();
    loadYaml();
  }

  std::string res;
  REQUIRE(true == controller->get(key, res));
  REQUIRE(value == res);
}

TEST_CASE_METHOD(PersistableKeyValueStoreServiceTestsFixture, "PersistableKeyValueStoreServiceTestsFixture special characters", "[basic]") {
  const char* key = "[]{}()==\\=\n\n";
  const char* value = ":./'\\=!\n=[]{}()";
  REQUIRE(true == controller->set(key, value));

  SECTION("without persistence") {
  }
  SECTION("with persistence") {
    controller->persist();
    loadYaml();
  }

  std::string res;
  REQUIRE(true == controller->get(key, res));
  REQUIRE(value == res);
}

TEST_CASE_METHOD(PersistableKeyValueStoreServiceTestsFixture, "PersistableKeyValueStoreServiceTestsFixture set and get all", "[basic]") {
  const std::unordered_map<std::string, std::string> kvs = {
      {"foobar", "234"},
      {"buzz", "value"},
  };
  for (const auto& kv : kvs) {
    REQUIRE(true == controller->set(kv.first, kv.second));
  }

  SECTION("without persistence") {
  }
  SECTION("with persistence") {
    controller->persist();
    loadYaml();
  }

  std::unordered_map<std::string, std::string> kvs_res;
  REQUIRE(true == controller->get(kvs_res));
  REQUIRE(kvs == kvs_res);
}

TEST_CASE_METHOD(PersistableKeyValueStoreServiceTestsFixture, "PersistableKeyValueStoreServiceTestsFixture set and overwrite", "[basic]") {
  const char* key = "foobar";
  const char* value = "234";
  const char* new_value = "baz";
  REQUIRE(true == controller->set(key, value));
  REQUIRE(true == controller->set(key, new_value));

  SECTION("without persistence") {
  }
  SECTION("with persistence") {
    controller->persist();
    loadYaml();
  }

  std::string res;
  REQUIRE(true == controller->get(key, res));
  REQUIRE(new_value == res);
}

TEST_CASE_METHOD(PersistableKeyValueStoreServiceTestsFixture, "PersistableKeyValueStoreServiceTestsFixture set and remove", "[basic]") {
  const char* key = "foobar";
  const char* value = "234";
  REQUIRE(true == controller->set(key, value));
  REQUIRE(true == controller->remove(key));

  SECTION("without persistence") {
  }
  SECTION("with persistence") {
    controller->persist();
    loadYaml();
  }

  std::string res;
  REQUIRE(false == controller->get(key, res));
}


TEST_CASE_METHOD(PersistableKeyValueStoreServiceTestsFixture, "PersistableKeyValueStoreServiceTestsFixture set and clear", "[basic]") {
  const std::unordered_map<std::string, std::string> kvs = {
      {"foobar", "234"},
      {"buzz", "value"},
  };
  for (const auto& kv : kvs) {
    REQUIRE(true == controller->set(kv.first, kv.second));
  }
  REQUIRE(true == controller->clear());

  SECTION("without persistence") {
  }
  SECTION("with persistence") {
    controller->persist();
    loadYaml();
  }

  std::unordered_map<std::string, std::string> kvs_res;
  REQUIRE(kvs_res.empty());

  /* Make sure we can still insert after we cleared */
  const char* key = "foo";
  const char* value = "bar";
  REQUIRE(true == controller->set(key, value));
  std::string res;
  REQUIRE(true == controller->get(key, res));
  REQUIRE(value == res);
}
