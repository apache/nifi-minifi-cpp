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
#include <vector>
#include <string>
#include <fstream>
#include <iterator>

#include "integration/HTTPIntegrationBase.h"
#include "integration/HTTPHandlers.h"
#include "unit/TestUtils.h"
#include "utils/Environment.h"
#include "utils/file/FileUtils.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::test {

class FileProvider : public ServerAwareHandler {
 public:
  explicit FileProvider(std::string file_content): file_content_(std::move(file_content)) {}

  bool handleGet(CivetServer* /*server*/, struct mg_connection* conn) override {
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: "
                    "text/plain\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
              file_content_.length());
    mg_printf(conn, "%s", file_content_.c_str());
    return true;
  }

 private:
  std::string file_content_;
};

class C2HeartbeatHandler : public HeartbeatHandler {
 public:
  using HeartbeatHandler::HeartbeatHandler;

  bool handlePost(CivetServer* /*server*/, struct mg_connection* conn) override {
    std::lock_guard<std::mutex> guard(op_mtx_);
    sendHeartbeatResponse(operations_, conn);
    operations_.clear();
    return true;
  }

  void addOperation(std::string id, std::unordered_map<std::string, c2::C2Value> args) {
    std::lock_guard<std::mutex> guard(op_mtx_);
    operations_.push_back(C2Operation{
      .operation = "update",
      .operand = "asset",
      .operation_id = std::move(id),
      .args = std::move(args)
    });
  }

 private:
  std::mutex op_mtx_;
  std::vector<C2Operation> operations_;
};

class VerifyC2AssetUpdate : public VerifyC2Base {
 public:
  void configureC2() override {
    configuration->set("nifi.c2.enable", "true");
    configuration->set("nifi.c2.agent.heartbeat.period", "100");
  }

  void runAssertions() override {
    REQUIRE(utils::verifyEventHappenedInPollTime(std::chrono::seconds(10), verify_));
  }

  void setVerifier(std::function<bool()> verify) {
    verify_ = std::move(verify);
  }

 private:
  std::function<bool()> verify_;
};

struct AssetUpdateOperation {
  std::string id;
  std::unordered_map<std::string, c2::C2Value> args;
  std::string state;
  std::optional<std::string> details;
};

TEST_CASE("Test update asset C2 command", "[c2test]") {
  TestController controller;

  // setup minifi home
  const std::filesystem::path home_dir = controller.createTempDirectory();
  const auto asset_dir = home_dir / "asset";

  std::filesystem::current_path(home_dir);
  auto wd_guard = gsl::finally([] {
    std::filesystem::current_path(minifi::utils::file::get_executable_dir());
  });

  C2AcknowledgeHandler ack_handler;
  std::string file_A = "hello from file A";
  FileProvider file_A_provider{file_A};
  std::string file_B = "hello from file B";
  FileProvider file_B_provider{file_B};
  C2HeartbeatHandler hb_handler{std::make_shared<minifi::ConfigureImpl>()};

  VerifyC2AssetUpdate harness;
  harness.setUrl("http://localhost:0/api/file/A.txt", &file_A_provider);
  harness.setUrl("http://localhost:0/api/file/B.txt", &file_B_provider);

  std::string absolute_file_A_url = "http://localhost:" + harness.getWebPort() + "/api/file/A.txt";

  std::vector<AssetUpdateOperation> operations;

  operations.push_back({
    .id = "1",
    .args = {},
    .state = "NOT_APPLIED",
    .details = "Couldn't find 'file' argument"
  });

  operations.push_back({
    .id = "2",
    .args = {
        {"file", minifi::c2::C2Value{"my_file.txt"}}
    },
    .state = "NOT_APPLIED",
    .details = "Couldn't find 'url' argument"
  });

  operations.push_back({
    .id = "3",
    .args = {
        {"file", minifi::c2::C2Value{"my_file.txt"}},
        {"url", minifi::c2::C2Value{"/api/file/A.txt"}}
    },
    .state = "FULLY_APPLIED",
    .details = std::nullopt
  });

  operations.push_back({
    .id = "4",
    .args = {
        {"file", minifi::c2::C2Value{"my_file.txt"}},
        {"url", minifi::c2::C2Value{"/api/file/A.txt"}}
    },
    .state = "NO_OPERATION",
    .details = std::nullopt
  });

  operations.push_back({
    .id = "5",
    .args = {
        {"file", minifi::c2::C2Value{"my_file.txt"}},
        {"url", minifi::c2::C2Value{"/api/file/B.txt"}},
        {"forceDownload", minifi::c2::C2Value{"true"}}
    },
    .state = "FULLY_APPLIED",
    .details = std::nullopt
  });

  operations.push_back({
    .id = "6",
    .args = {
        {"file", minifi::c2::C2Value{"new_dir/inner/my_file.txt"}},
        {"url", minifi::c2::C2Value{"/api/file/A.txt"}}
    },
    .state = "FULLY_APPLIED",
    .details = std::nullopt
  });

  operations.push_back({
    .id = "7",
    .args = {
        {"file", minifi::c2::C2Value{"dummy.txt"}},
        {"url", minifi::c2::C2Value{"/not_existing_api/file.txt"}}
    },
    .state = "NOT_APPLIED",
    .details = "Failed to fetch asset"
  });

  operations.push_back({
    .id = "8",
    .args = {
        {"file", minifi::c2::C2Value{"../../system_lib.dll"}},
        {"url", minifi::c2::C2Value{"/not_existing_api/file.txt"}}
    },
    .state = "NOT_APPLIED",
    .details = "Accessing parent directory is forbidden in file path"
  });

  operations.push_back({
    .id = "9",
    .args = {
        {"file", minifi::c2::C2Value{"other_dir/A.txt"}},
        {"url", minifi::c2::C2Value{absolute_file_A_url}}
    },
    .state = "FULLY_APPLIED",
    .details = std::nullopt
  });

  for (auto& op : operations) {
    hb_handler.addOperation(op.id, op.args);
  }

  harness.setVerifier([&] () -> bool {
    for (auto& op : operations) {
      if (auto res = ack_handler.getState(op.id)) {
        if (res->state != op.state) {
          controller.getLogger()->log_error("Operation '{}' in expected to return '{}', got '{}'", op.id, op.state, res->state);
          REQUIRE(false);
        }
        if (op.details) {
          if (res->details.find(op.details.value()) == std::string::npos) {
            controller.getLogger()->log_error("In operation '{}' failed to find '{}' in ack details '{}'", op.id, op.details.value(), res->details);
            REQUIRE(false);
          }
        }
      } else {
        return false;
      }
    }
    return true;
  });

  harness.setUrl("http://localhost:0/api/heartbeat", &hb_handler);
  harness.setUrl("http://localhost:0/api/acknowledge", &ack_handler);
  harness.setC2Url("/api/heartbeat", "/api/acknowledge");

  harness.run();

  std::unordered_map<std::string, std::string> expected_files;
  // verify directory structure
  for (auto& op : operations) {
    if (op.state != "FULLY_APPLIED") {
      // this op failed no file made on the disk
      continue;
    }
    expected_files[(asset_dir / op.args["file"].to_string()).string()] = minifi::utils::string::endsWith(op.args["url"].to_string(), "A.txt") ? file_A : file_B;
  }

  size_t file_count = minifi::utils::file::list_dir_all(asset_dir.string(), controller.getLogger()).size();
  // + 1 is for the .state file from the AssetManager
  if (file_count != expected_files.size() + 1) {
    controller.getLogger()->log_error("Expected {} files, got {}", expected_files.size(), file_count);
    REQUIRE(false);
  }
  for (auto& [path, content] : expected_files) {
    if (minifi::utils::file::get_content(path) != content) {
      controller.getLogger()->log_error("File content mismatch at '{}'", path);
      REQUIRE(false);
    }
  }
}

}  // namespace org::apache::nifi::minifi::test
