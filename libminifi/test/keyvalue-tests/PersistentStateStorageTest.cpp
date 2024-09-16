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

#include "unit/Catch.h"
#include "unit/TestBase.h"
#include "catch2/catch_session.hpp"
#include "core/controller/ControllerService.h"
#include "core/ProcessGroup.h"
#include "core/yaml/YamlConfiguration.h"
#include "controllers/keyvalue/KeyValueStateStorage.h"
#include "controllers/keyvalue/AutoPersistor.h"
#include "unit/ProvenanceTestHelper.h"
#include "repository/VolatileContentRepository.h"
#include "utils/file/FileUtils.h"

static std::string config_yaml; // NOLINT

int main(int argc, char* argv[]) {
  Catch::Session session;

  auto cli = session.cli()
      | Catch::Clara::Opt{config_yaml, "config-yaml"}
          ["--config-yaml"]
          ("path to the config.yaml containing the StateStorage controller service configuration");
  session.cli(cli);

  int ret = session.applyCommandLine(argc, argv);
  if (ret != 0) {
    return ret;
  }

  if (config_yaml.empty()) {
    std::cerr << "Missing --config-yaml <path>. It must contain the path to the config.yaml containing the StateStorage controller service configuration." << std::endl;
    return -1;
  }

  return session.run();
}

class PersistentStateStorageTestsFixture {
 public:
  PersistentStateStorageTestsFixture() {
    LogTestController::getInstance().setTrace<TestPlan>();
    LogTestController::getInstance().setTrace<minifi::controllers::KeyValueStateStorage>();
    LogTestController::getInstance().setTrace<minifi::controllers::AutoPersistor>();

    std::filesystem::current_path(testController.createTempDirectory());
    loadYaml();
  }

  PersistentStateStorageTestsFixture(PersistentStateStorageTestsFixture&&) = delete;
  PersistentStateStorageTestsFixture(const PersistentStateStorageTestsFixture&) = delete;
  PersistentStateStorageTestsFixture& operator=(PersistentStateStorageTestsFixture&&) = delete;
  PersistentStateStorageTestsFixture& operator=(const PersistentStateStorageTestsFixture&) = delete;

  virtual ~PersistentStateStorageTestsFixture() {
    LogTestController::getInstance().reset();
    std::filesystem::current_path(minifi::utils::file::get_executable_dir());
  }

  void loadYaml() {
    controller.reset();

    process_group.reset();
    yaml_config.reset();

    content_repo.reset();
    test_flow_repo.reset();
    test_repo.reset();
    configuration.reset();

    configuration = std::make_shared<minifi::ConfigureImpl>();
    test_repo = std::make_shared<TestRepository>();
    test_flow_repo = std::make_shared<TestFlowRepository>();

    configuration->set(minifi::Configure::nifi_flow_configuration_file, config_yaml);

    content_repo = std::make_shared<core::repository::VolatileContentRepository>();
    content_repo->initialize(configuration);

    yaml_config = std::make_unique<core::YamlConfiguration>(core::ConfigurationContext{
        .flow_file_repo = test_repo,
        .content_repo = content_repo,
        .configuration = configuration,
        .path = config_yaml,
        .filesystem = std::make_shared<utils::file::FileSystem>(),
        .sensitive_values_encryptor = utils::crypto::EncryptionProvider{utils::crypto::XSalsa20Cipher{utils::crypto::XSalsa20Cipher::generateKey()}}
    });
    process_group = yaml_config->getRoot();
    auto* persistable_key_value_store_service_node = process_group->findControllerService("testcontroller");
    REQUIRE(persistable_key_value_store_service_node != nullptr);
    persistable_key_value_store_service_node->enable();

    controller = std::dynamic_pointer_cast<minifi::controllers::KeyValueStateStorage>(
        persistable_key_value_store_service_node->getControllerServiceImplementation());
    REQUIRE(controller != nullptr);
  }

 protected:
  TestController testController;
  std::shared_ptr<minifi::Configure> configuration;
  std::shared_ptr<core::Repository> test_repo;
  std::shared_ptr<core::Repository> test_flow_repo;
  std::shared_ptr<core::ContentRepository> content_repo;

  std::unique_ptr<core::YamlConfiguration> yaml_config;
  std::unique_ptr<core::ProcessGroup> process_group;

  std::shared_ptr<minifi::controllers::KeyValueStateStorage> controller;
};


TEST_CASE_METHOD(PersistentStateStorageTestsFixture, "PersistentStateStorageTestsFixture set and get", "[basic]") {
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

TEST_CASE_METHOD(PersistentStateStorageTestsFixture, "PersistentStateStorageTestsFixture special characters", "[basic]") {
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

TEST_CASE_METHOD(PersistentStateStorageTestsFixture, "PersistentStateStorageTestsFixture set and get all", "[basic]") {
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

TEST_CASE_METHOD(PersistentStateStorageTestsFixture, "PersistentStateStorageTestsFixture set and overwrite", "[basic]") {
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

TEST_CASE_METHOD(PersistentStateStorageTestsFixture, "PersistentStateStorageTestsFixture set and remove", "[basic]") {
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


TEST_CASE_METHOD(PersistentStateStorageTestsFixture, "PersistentStateStorageTestsFixture set and clear", "[basic]") {
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
