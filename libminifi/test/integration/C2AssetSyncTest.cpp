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
#include <vector>
#include <string>
#include <fstream>
#include <iterator>

#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"
#include "utils/IntegrationTestUtils.h"
#include "utils/file/FileUtils.h"
#include "utils/file/AssetManager.h"

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
  using AssetDescription = org::apache::nifi::minifi::utils::file::AssetDescription;

  void handleHeartbeat(const rapidjson::Document& root, struct mg_connection* conn) override {
    std::string hb_str = [&] {
      rapidjson::StringBuffer buffer;
      rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
      root.Accept(writer);
      return std::string{buffer.GetString(), buffer.GetSize()};
    }();
    auto& asset_info_node = root["assetInfo"];
    auto& asset_hash_node = asset_info_node["hash"];
    std::string asset_hash{asset_hash_node.GetString(), asset_hash_node.GetStringLength()};

    std::vector<C2Operation> operations;
    {
      std::lock_guard guard(asset_mtx_);
      agent_asset_hash_ = asset_hash;
      if (asset_hash != calculateAssetHash()) {
        std::unordered_map<std::string, std::string> args;
        for (auto& asset : expected_assets_) {
          args[asset.id + ".path"] = asset.path.string();
          args[asset.id + ".url"] = asset.url;
        }
        operations.push_back(C2Operation{
          .operation = "sync",
          .operand = "asset",
          .operation_id = std::to_string(next_op_id_++),
          .args = std::move(args)
        });
      }
    }
    sendHeartbeatResponse(operations, conn);
  }

  void addAsset(std::string id, std::string path, std::string url) {
    std::lock_guard guard(asset_mtx_);
    expected_assets_.insert(AssetDescription{
      .id = std::move(id),
      .path = std::move(path),
      .url = std::move(url)
    });
  }

  void removeAsset(std::string id) {
    std::lock_guard guard{asset_mtx_};
    expected_assets_.erase(AssetDescription{.id = std::move(id), .path = {}, .url = {}});
  }

  std::optional<std::string> getAgentAssetHash() const {
    std::lock_guard lock(asset_mtx_);
    return agent_asset_hash_;
  }

  std::string calculateAssetHash() const {
    std::lock_guard guard{asset_mtx_};
    size_t hash_value{0};
    for (auto& asset : expected_assets_) {
      hash_value = utils::hash_combine(hash_value, std::hash<std::string>{}(asset.id));
    }
    return std::to_string(hash_value);
  }

  std::string assetState() const {
    std::lock_guard guard{asset_mtx_};
    rapidjson::Document doc;
    doc.SetObject();
    for (auto& asset : expected_assets_) {
      auto path_str = asset.path.string();
      doc.AddMember(rapidjson::StringRef(asset.id), rapidjson::kObjectType, doc.GetAllocator());
      doc[asset.id].AddMember(rapidjson::StringRef("path"), rapidjson::Value(path_str, doc.GetAllocator()), doc.GetAllocator());
      doc[asset.id].AddMember(rapidjson::StringRef("url"), rapidjson::StringRef(asset.url), doc.GetAllocator());
    }
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    return {buffer.GetString(), buffer.GetSize()};
  }

 private:
  mutable std::recursive_mutex asset_mtx_;
  std::set<AssetDescription> expected_assets_;

  std::optional<std::string> agent_asset_hash_;

  std::atomic<size_t> next_op_id_{1};
};

class VerifyC2AssetSync : public VerifyC2Base {
 public:
  void configureC2() override {
    configuration->set("nifi.c2.agent.protocol.class", "RESTSender");
    configuration->set("nifi.c2.enable", "true");
    configuration->set("nifi.c2.agent.heartbeat.period", "100");
    configuration->set("nifi.c2.root.classes", "DeviceInfoNode,AgentInformation,FlowInformation,AssetInformation");
  }

  void runAssertions() override {
    verify_();
  }

  void setVerifier(std::function<void()> verify) {
    verify_ = std::move(verify);
  }

 private:
  std::function<void()> verify_;
};

int main() {
  TestController controller;

  // setup minifi home
  const std::filesystem::path home_dir = controller.createTempDirectory();
  const auto asset_dir = home_dir / "asset";
  std::filesystem::current_path(home_dir);

  C2AcknowledgeHandler ack_handler;
  std::string file_A = "hello from file A";
  FileProvider file_A_provider{file_A};
  std::string file_B = "hello from file B";
  FileProvider file_B_provider{file_B};
  std::string file_C = "hello from file C";
  FileProvider file_C_provider{file_C};
  std::string file_A_v2 = "hello from file A version 2";
  FileProvider file_Av2_provider{file_A_v2};
  C2HeartbeatHandler hb_handler{std::make_shared<minifi::Configure>()};

  VerifyC2AssetSync harness;
  harness.setUrl("http://localhost:0/api/file/A.txt", &file_A_provider);
  harness.setUrl("http://localhost:0/api/file/Av2.txt", &file_Av2_provider);
  harness.setUrl("http://localhost:0/api/file/B.txt", &file_B_provider);
  harness.setUrl("http://localhost:0/api/file/C.txt", &file_C_provider);

  std::string absolute_file_A_url = "http://localhost:" + harness.getWebPort() + "/api/file/A.txt";

  hb_handler.addAsset("Av1", "A.txt", "/api/file/A.txt");
  hb_handler.addAsset("Bv1", "nested/dir/B.txt", "/api/file/B.txt");
  hb_handler.addAsset("Cv1", "nested/C.txt", "/api/file/C.txt");

  harness.setUrl("http://localhost:0/api/heartbeat", &hb_handler);
  harness.setUrl("http://localhost:0/api/acknowledge", &ack_handler);
  harness.setC2Url("/api/heartbeat", "/api/acknowledge");

  auto get_asset_structure = [&] () {
    std::unordered_map<std::string, std::string> contents;
    for (auto& [dir, file] : utils::file::list_dir_all(asset_dir.string(), controller.getLogger())) {
      contents[(dir / file).string()] = utils::file::get_content(dir / file);
    }
    return contents;
  };

  harness.setVerifier([&] () {
    assert(utils::verifyEventHappenedInPollTime(10s, [&] {return hb_handler.calculateAssetHash() == hb_handler.getAgentAssetHash();}));

    {
      std::unordered_map<std::string, std::string> expected_assets{
          {(asset_dir / "A.txt").string(), file_A},
          {(asset_dir / "nested" / "dir" / "B.txt").string(), file_B},
          {(asset_dir / "nested" / "C.txt").string(), file_C},
          {(asset_dir / ".state").string(), hb_handler.assetState()}
      };
      auto actual_assets = get_asset_structure();
      if (actual_assets != expected_assets) {
        controller.getLogger()->log_error("Mismatch between expected and actual assets");
        for (auto& [path, content] : expected_assets) {
          controller.getLogger()->log_error("Expected asset at {}: {}", path, content);
        }
        for (auto& [path, content] : actual_assets) {
          controller.getLogger()->log_error("Actual asset at {}: {}", path, content);
        }
        assert(false);
      }
    }

    hb_handler.removeAsset("Av1");
    hb_handler.removeAsset("Cv1");
    hb_handler.addAsset("Av2", "A.txt", "/api/file/Av2.txt");


    assert(utils::verifyEventHappenedInPollTime(10s, [&] {return hb_handler.calculateAssetHash() == hb_handler.getAgentAssetHash();}));

    {
      std::unordered_map<std::string, std::string> expected_assets{
          {(asset_dir / "A.txt").string(), file_A_v2},
          {(asset_dir / "nested" / "dir" / "B.txt").string(), file_B},
          {(asset_dir / ".state").string(), hb_handler.assetState()}
      };

      auto actual_assets = get_asset_structure();
      if (actual_assets != expected_assets) {
        controller.getLogger()->log_error("Mismatch between expected and actual assets");
        for (auto& [path, content] : expected_assets) {
          controller.getLogger()->log_error("Expected asset at {}: {}", path, content);
        }
        for (auto& [path, content] : actual_assets) {
          controller.getLogger()->log_error("Actual asset at {}: {}", path, content);
        }
        assert(false);
      }
    }
  });

  harness.run();
}
