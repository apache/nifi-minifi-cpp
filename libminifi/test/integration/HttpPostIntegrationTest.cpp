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

#include <sys/stat.h>
#include <cassert>
#include <chrono>
#include <fstream>
#include <utility>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#include "utils/StringUtils.h"
#include "core/Core.h"
#include "../include/core/logging/Logger.h"
#include "core/ProcessGroup.h"
#include "core/yaml/YamlConfiguration.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "../unit/ProvenanceTestHelper.h"
#include "io/StreamFactory.h"
#include "../TestBase.h"


void waitToVerifyProcessor() {
  std::this_thread::sleep_for(std::chrono::seconds(2));
}

int main(int argc, char **argv) {
  LogTestController::getInstance().setDebug<processors::InvokeHTTP>();
  LogTestController::getInstance().setDebug<minifi::core::ProcessSession>();
  std::string test_file_location;
  if (argc > 1) {
    test_file_location = argv[1];
  }
  mkdir("/tmp/aljr39/", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  std::ofstream myfile;
  myfile.open("/tmp/aljr39/example.txt");
  myfile << "Hello world" << std::endl;
  myfile.close();
  mkdir("content_repository", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

  std::shared_ptr<minifi::Configure> configuration = std::make_shared<
      minifi::Configure>();

  std::shared_ptr<core::Repository> test_repo =
      std::make_shared<TestRepository>();
  std::shared_ptr<core::Repository> test_flow_repo = std::make_shared<
      TestFlowRepository>();

  configuration->set(minifi::Configure::nifi_flow_configuration_file,
                     test_file_location);
  std::shared_ptr<minifi::io::StreamFactory> stream_factory = std::make_shared<
      minifi::io::StreamFactory>(configuration);

  std::unique_ptr<core::FlowConfiguration> yaml_ptr = std::unique_ptr<
      core::YamlConfiguration>(
      new core::YamlConfiguration(test_repo, test_repo, stream_factory,
                                  configuration, test_file_location));
  std::shared_ptr<TestRepository> repo =
      std::static_pointer_cast<TestRepository>(test_repo);

  std::shared_ptr<minifi::FlowController> controller = std::make_shared<
      minifi::FlowController>(test_repo, test_flow_repo, configuration,
                              std::move(yaml_ptr),
                              DEFAULT_ROOT_GROUP_NAME,
                              true);

  core::YamlConfiguration yaml_config(test_repo, test_repo, stream_factory,
                                      configuration, test_file_location);

  std::unique_ptr<core::ProcessGroup> ptr = yaml_config.getRoot(
      test_file_location);
  std::shared_ptr<core::ProcessGroup> pg = std::shared_ptr<core::ProcessGroup>(
      ptr.get());
  ptr.release();

  controller->load();
  controller->start();
  waitToVerifyProcessor();

  controller->waitUnload(60000);
  assert(LogTestController::getInstance().contains("curl performed") == true);
  assert(LogTestController::getInstance().contains("Import offset 0 length 12") == true);

  std::string stringtofind = "Resource Claim created ./content_repository/";

  std::string logs = LogTestController::getInstance().log_output.str();
  size_t loc = logs.find(stringtofind);
  while (loc > 0 && loc != std::string::npos) {
    std::string id = logs.substr(loc + stringtofind.size(), 36);
    loc = logs.find(stringtofind, loc + 1);
    std::string path = "content_repository/" + id;
    unlink(path.c_str());
    if (loc == std::string::npos)
      break;
  }

  rmdir("./content_repository");
  LogTestController::getInstance().reset();
  return 0;
}
