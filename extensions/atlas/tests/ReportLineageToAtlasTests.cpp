/**
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

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ReportLineageToAtlas.h"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "core/Processor.h"
#include "core/logging/LoggerFactory.h"
#include "unit/Catch.h"
#include "unit/ReportingTaskUtils.h"
#include "utils/PropertyErrors.h"

namespace org::apache::nifi::minifi::extensions::atlas::test {

namespace {

// Minimal in-memory ReportingTaskContext. Only the scheduling-path accessors carry
// state; the trigger-path ones (provenance repository, state manager, topology) are
// stubs, which is enough to exercise onSchedule.
class TestReportingTaskContext : public core::reporting::ReportingTaskContext {
 public:
  std::expected<std::string, std::error_code> getProperty(std::string_view name) const override {
    if (const auto it = properties_.find(std::string{name}); it != properties_.end()) {
      return it->second;
    }
    return std::unexpected(core::PropertyErrorCode::PropertyNotSet);
  }

  std::expected<std::string, std::error_code> getRawDynamicProperty(std::string_view name) const override {
    if (const auto it = dynamic_properties_.find(std::string{name}); it != dynamic_properties_.end()) {
      return it->second;
    }
    return std::unexpected(core::PropertyErrorCode::PropertyNotSet);
  }

  std::vector<std::string> getDynamicPropertyKeys() const override {
    std::vector<std::string> keys;
    for (const auto& [key, _] : dynamic_properties_) {
      keys.push_back(key);
    }
    return keys;
  }

  std::shared_ptr<core::controller::ControllerServiceHandle> getControllerService(const std::string&, const utils::Identifier&) const override { return nullptr; }
  std::shared_ptr<provenance::ProvenanceRepository> getProvenanceRepository() override { return nullptr; }
  core::StateManager* getStateManager() override { return nullptr; }
  uint8_t getMaxConcurrentTasks() const override { return 1; }
  core::reporting::FlowTopology getFlowTopology() const override { return {}; }
  void yield() override {}

  void setProperty(std::string name, std::string value) { properties_.insert_or_assign(std::move(name), std::move(value)); }
  void setDynamicProperty(std::string name, std::string value) { dynamic_properties_.insert_or_assign(std::move(name), std::move(value)); }

 private:
  std::map<std::string, std::string> properties_;
  std::map<std::string, std::string> dynamic_properties_;
};

ReportLineageToAtlas makeTask() {
  const auto uuid = minifi::utils::IdGenerator::getIdGenerator()->generate();
  return ReportLineageToAtlas{core::reporting::ReportingTaskMetadata{
      .uuid = uuid,
      .name = "test-report-lineage-to-atlas",
      .logger = core::logging::LoggerFactory<ReportLineageToAtlas>::getLogger(uuid)}};
}

// The two properties onSchedule refuses to start without.
TestReportingTaskContext makeMinimalContext() {
  TestReportingTaskContext context;
  context.setProperty(std::string{ReportLineageToAtlas::AtlasUrls.name}, "http://atlas.example.com:21000");
  context.setProperty(std::string{ReportLineageToAtlas::DefaultNamespace.name}, "ns1");
  return context;
}

}  // namespace

TEST_CASE("ReportLineageToAtlas registers its properties", "[atlas]") {
  auto task = minifi::test::utils::make_reporting_task<ReportLineageToAtlas>("test-report-lineage-to-atlas");
  REQUIRE(task);
  REQUIRE(task->getName() == "test-report-lineage-to-atlas");
  task->initialize();

  // Sanity-check that each declared property is present in the supported set.
  const auto supported = task->getSupportedProperties();
  REQUIRE(supported.contains(ReportLineageToAtlas::AtlasUrls.name));
  REQUIRE(supported.contains(ReportLineageToAtlas::NiFiUrl.name));
  REQUIRE(supported.contains(ReportLineageToAtlas::DefaultNamespace.name));
  REQUIRE(supported.contains(ReportLineageToAtlas::AwsS3ModelVersion.name));
  REQUIRE(supported.contains(ReportLineageToAtlas::FilesystemPathLevel.name));
  REQUIRE(supported.contains(ReportLineageToAtlas::ProvenanceBatchSize.name));
}

// initialize() is skipped deliberately: it only registers supported properties with a
// ReportingTaskDescriptor, and onSchedule reads properties straight from the context.
TEST_CASE("A malformed hostnamePattern names the offending property", "[atlas]") {
  auto task = makeTask();
  auto context = makeMinimalContext();
  context.setDynamicProperty("hostnamePattern.prod", "(unclosed");

  // Without handling, std::regex_error (a std::logic_error, not a minifi Exception)
  // escapes with only "Unexpected end of regex" and no hint which property is broken.
  REQUIRE_THROWS_WITH(task.onSchedule(context), Catch::Matchers::ContainsSubstring("hostnamePattern.prod"));
}

TEST_CASE("DIRECTORY path level collapses a file dataset to its directory", "[atlas]") {
  NamespaceResolver resolver;
  resolver.setDefaultNamespace("ns1");
  const Dataset dataset{.system = "file", .identifier = "/data/in/a.txt", .host = std::string{"localhost"},
      .attributes = {{"name", "/data/in/a.txt"}, {"path", "/data/in/a.txt"}}};

  const auto entry = ReportLineageToAtlas::datasetToEntity(dataset, ReportLineageToAtlas::FsPathLevel::DIRECTORY,
      ReportLineageToAtlas::S3ModelVersion::V2, resolver);

  REQUIRE(entry.qualified_name == "/data/in/@ns1");
  REQUIRE(entry.string_attributes.at("name") == "/data/in/");
  // The regression: `path` used to keep the full file path while name/qualifiedName were
  // collapsed, so the directory entity's `path` became whichever file was seen last.
  REQUIRE(entry.string_attributes.at("path") == "/data/in/");
}

TEST_CASE("Two files in one directory collapse to the same entity", "[atlas]") {
  NamespaceResolver resolver;
  resolver.setDefaultNamespace("ns1");
  const auto convert = [&resolver](std::string identifier) {
    const Dataset dataset{.system = "file", .identifier = identifier, .host = std::string{"localhost"},
        .attributes = {{"name", identifier}, {"path", identifier}}};
    return ReportLineageToAtlas::datasetToEntity(dataset, ReportLineageToAtlas::FsPathLevel::DIRECTORY,
        ReportLineageToAtlas::S3ModelVersion::V2, resolver);
  };

  const auto first = convert("/data/in/a.txt");
  const auto second = convert("/data/in/b.txt");

  // Atlas upserts by qualifiedName, so these are the same entity. Every attribute must
  // therefore agree, or whichever is published last silently wins.
  REQUIRE(first.qualified_name == second.qualified_name);
  REQUIRE(first.string_attributes.at("path") == second.string_attributes.at("path"));
  REQUIRE(first.string_attributes.at("name") == second.string_attributes.at("name"));
}

TEST_CASE("FILE path level keeps the full file path", "[atlas]") {
  NamespaceResolver resolver;
  resolver.setDefaultNamespace("ns1");
  const Dataset dataset{.system = "file", .identifier = "/data/in/a.txt", .host = std::string{"localhost"},
      .attributes = {{"name", "/data/in/a.txt"}, {"path", "/data/in/a.txt"}}};

  const auto entry = ReportLineageToAtlas::datasetToEntity(dataset, ReportLineageToAtlas::FsPathLevel::FILE,
      ReportLineageToAtlas::S3ModelVersion::V2, resolver);

  REQUIRE(entry.qualified_name == "/data/in/a.txt@ns1");
  REQUIRE(entry.string_attributes.at("path") == "/data/in/a.txt");
}

TEST_CASE("A non-file dataset is unaffected by the path level", "[atlas]") {
  NamespaceResolver resolver;
  resolver.setDefaultNamespace("ns1");
  const Dataset dataset{.system = "kafka", .identifier = "orders", .host = std::string{"broker1"}, .attributes = {}};

  const auto entry = ReportLineageToAtlas::datasetToEntity(dataset, ReportLineageToAtlas::FsPathLevel::DIRECTORY,
      ReportLineageToAtlas::S3ModelVersion::V2, resolver);

  REQUIRE(entry.qualified_name == "orders@ns1");
  // Asset.name is mandatory in Atlas, so it falls back to the identifier.
  REQUIRE(entry.string_attributes.at("name") == "orders");
}

TEST_CASE("A well-formed hostnamePattern schedules cleanly", "[atlas]") {
  auto task = makeTask();
  auto context = makeMinimalContext();
  context.setDynamicProperty("hostnamePattern.prod", "prod-.*\\.example\\.com .*\\.internal");

  REQUIRE_NOTHROW(task.onSchedule(context));
}

}  // namespace org::apache::nifi::minifi::extensions::atlas::test
