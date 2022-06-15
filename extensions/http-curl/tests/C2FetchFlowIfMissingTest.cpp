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

#undef NDEBUG
#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"
#include "utils/IntegrationTestUtils.h"
#include "utils/file/PathUtils.h"

using namespace std::literals::chrono_literals;

int main(int argc, char **argv) {
  TestController controller;
  std::string minifi_home = controller.createTempDirectory();
  const cmd_args args = parse_cmdline_args(argc, argv);
  C2FlowProvider handler(args.test_file);
  VerifyFlowFetched harness(10s);
  harness.setKeyDir(args.key_dir);
  harness.setUrl(args.url, &handler);
  harness.setFlowUrl(harness.getC2RestUrl());

  std::string config_path = utils::file::PathUtils::concat_path(minifi_home, "config.yml");

  harness.run(config_path);

  // check existence of the config file
  assert(std::ifstream{config_path});

  return 0;
}

