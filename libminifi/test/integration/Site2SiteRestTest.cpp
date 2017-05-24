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
#include <utility>
#include <chrono>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#include <iostream>
#include <sstream>
#include "../TestBase.h"
#include "utils/StringUtils.h"
#include "core/Core.h"
#include "../include/core/logging/Logger.h"
#include "core/ProcessGroup.h"
#include "core/yaml/YamlConfiguration.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "../unit/ProvenanceTestHelper.h"
#include "io/StreamFactory.h"
#include "CivetServer.h"
#include "RemoteProcessorGroupPort.h"

void waitToVerifyProcessor() {
  std::this_thread::sleep_for(std::chrono::seconds(10));
}

class ConfigHandler : public CivetHandler {
 public:
  bool handleGet(CivetServer *server, struct mg_connection *conn) {
    static const std::string site2site_rest_resp = "{"
        "\"revision\": {"
        "\"clientId\": \"483d53eb-53ec-4e93-b4d4-1fc3d23dae6f\""
        "},"
        "\"controller\": {"
        "\"id\": \"fe4a3a42-53b6-4af1-a80d-6fdfe60de97f\","
        "\"name\": \"NiFi Flow\","
        "\"remoteSiteListeningPort\": 10001,"
        "\"siteToSiteSecure\": false"
        "}}";
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
              "text/plain\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
              site2site_rest_resp.length());
    mg_printf(conn, "%s", site2site_rest_resp.c_str());
    return true;
  }
};

int main(int argc, char **argv) {
  LogTestController::getInstance().setInfo<minifi::RemoteProcessorGroupPort>();
  LogTestController::getInstance().setInfo<minifi::FlowController>();

  const char *options[] = { "document_root", ".", "listening_ports", "8082", 0 };
  std::vector<std::string> cpp_options;
  for (int i = 0; i < (sizeof(options) / sizeof(options[0]) - 1); i++) {
    cpp_options.push_back(options[i]);
  }

  mkdir("/tmp/site2siteGetFile/", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  std::fstream file;
  std::stringstream ss;
  ss << "/tmp/site2siteGetFile/" << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << "tempFile";
  file.close();

  CivetServer server(cpp_options);
  ConfigHandler h_ex;
  server.addHandler("/nifi-api/controller/", h_ex);
  LogTestController::getInstance().setDebug<minifi::RemoteProcessorGroupPort>();

  std::string key_dir, test_file_location;
  if (argc > 1) {
    test_file_location = argv[1];
    key_dir = argv[2];
  }

  std::shared_ptr<minifi::Configure> configuration = std::make_shared<
      minifi::Configure>();
  configuration->set(minifi::Configure::nifi_default_directory, key_dir);
  mkdir("content_repository", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

  std::shared_ptr<core::Repository> test_repo =
      std::make_shared<TestRepository>();
  std::shared_ptr<core::Repository> test_flow_repo = std::make_shared<
      TestFlowRepository>();

  configuration->set(minifi::Configure::nifi_flow_configuration_file,
                     test_file_location);

  std::shared_ptr<minifi::io::StreamFactory> stream_factory = std::make_shared
      <minifi::io::StreamFactory>(configuration);
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();

  content_repo->initialize(configuration);

  std::unique_ptr<core::FlowConfiguration> yaml_ptr = std::unique_ptr<core::YamlConfiguration>(
      new core::YamlConfiguration(test_repo, test_repo, content_repo, stream_factory, configuration, test_file_location));
  std::shared_ptr<TestRepository> repo = std::static_pointer_cast<TestRepository>(test_repo);

  std::shared_ptr<minifi::FlowController> controller = std::make_shared<minifi::FlowController>(test_repo, test_flow_repo, configuration, std::move(yaml_ptr),
                                                                                                content_repo,
                                                                                                DEFAULT_ROOT_GROUP_NAME,
                                                                                                true);

  core::YamlConfiguration yaml_config(test_repo, test_repo, content_repo, stream_factory,
                                      configuration,
                                      test_file_location);

  std::unique_ptr<core::ProcessGroup> ptr = yaml_config.getRoot(
                                                                test_file_location);
  std::shared_ptr<core::ProcessGroup> pg = std::shared_ptr<core::ProcessGroup
      >(ptr.get());
  ptr.release();

  controller->load();
  controller->start();
  waitToVerifyProcessor();

  controller->waitUnload(60000);
  std::string logs = LogTestController::getInstance().log_output.str();
  assert(logs.find("process group remote site2site port 10001, is secure 0") != std::string::npos);
  LogTestController::getInstance().reset();
  unlink(ss.str().c_str());
  rmdir("/tmp/site2siteGetFile/");
  rmdir("./content_repository");
  return 0;
}
