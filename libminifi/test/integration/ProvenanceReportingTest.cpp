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
#undef NDEBUG
#include <cassert>
#include <chrono>
#include <fstream>
#include <utility>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#include "utils/file/FileUtils.h"
#include "utils/StringUtils.h"
#include "core/Core.h"
#include "core/logging/Logger.h"
#include "core/ProcessGroup.h"
#include "core/yaml/YamlConfiguration.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "../unit/ProvenanceTestHelper.h"
#include "io/StreamFactory.h"
#include "../TestBase.h"
#include "utils/IntegrationTestUtils.h"

int main(int argc, char **argv) {
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;

  std::string test_file_location;
  if (argc > 1) {
    test_file_location = argv[1];
  }
  // need to change test to not use temp dir
#ifndef WIN32
  mkdir("/tmp/aljs39/", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

  mkdir("content_repository", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

  LogTestController::getInstance().setDebug<core::ProcessGroup>();

  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  std::shared_ptr<core::Repository> test_repo = std::make_shared<TestRepository>();
  std::shared_ptr<core::Repository> test_flow_repo = std::make_shared<TestFlowRepository>();

  configuration->set(minifi::Configure::nifi_flow_configuration_file, test_file_location);
  std::shared_ptr<minifi::io::StreamFactory> stream_factory = minifi::io::StreamFactory::getInstance(configuration);
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  std::unique_ptr<core::FlowConfiguration> yaml_ptr = std::unique_ptr<core::YamlConfiguration>(
      new core::YamlConfiguration(test_repo, test_repo, content_repo, stream_factory, configuration, test_file_location));
  std::shared_ptr<TestRepository> repo = std::static_pointer_cast<TestRepository>(test_repo);

  std::shared_ptr<minifi::FlowController> controller = std::make_shared<minifi::FlowController>(
      test_repo, test_flow_repo, configuration, std::move(yaml_ptr), content_repo, DEFAULT_ROOT_GROUP_NAME);

  core::YamlConfiguration yaml_config(test_repo, test_repo, content_repo, stream_factory, configuration, test_file_location);

  std::unique_ptr<core::ProcessGroup> ptr = yaml_config.getRoot();
  std::shared_ptr<core::ProcessGroup> pg = std::shared_ptr<core::ProcessGroup>(ptr.get());
  ptr.release();
  std::shared_ptr<org::apache::nifi::minifi::io::SocketContext> socket_context = std::make_shared<org::apache::nifi::minifi::io::SocketContext>(std::make_shared<minifi::Configure>());
  org::apache::nifi::minifi::io::ServerSocket server(socket_context, "localhost", 10005, 1);

  controller->load();
  controller->start();

  assert(verifyLogLinePresenceInPollTime(std::chrono::milliseconds(std::chrono::seconds(2)), "Add processor SiteToSiteProvenanceReportingTask into process group MiNiFi Flow"));

  controller->waitUnload(60000);
  LogTestController::getInstance().reset();
  rmdir("./content_repository");
  rmdir("/tmp/aljs39/");
#endif
  return 0;
}
