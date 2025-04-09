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
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "OpcUaTestServer.h"
#include "unit/SingleProcessorTestController.h"
#include "include/fetchopc.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("Test fetching using path node id", "[fetchopcprocessor]") {
  OpcUaTestServer server(4841);
  server.start();
  SingleProcessorTestController controller{std::make_unique<processors::FetchOPCProcessor>("FetchOPCProcessor")};
  auto fetch_opc_processor = controller.getProcessor();
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4841/");
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::NodeIDType.name, "Path");
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::NodeID.name, "Simulator/Default/Device1");
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::NameSpaceIndex.name, std::to_string(server.getNamespaceIndex()));

  const auto results = controller.trigger();
  REQUIRE(results.at(processors::FetchOPCProcessor::Failure).empty());
  REQUIRE(results.at(processors::FetchOPCProcessor::Success).size() == 4);
  for (size_t i = 0; i < 3; i++) {
    auto flow_file = results.at(processors::FetchOPCProcessor::Success)[i];
    CHECK(flow_file->getAttribute("Browsename") == "INT" + std::to_string(i + 1));
    CHECK(flow_file->getAttribute("Datasize") == "4");
    CHECK(flow_file->getAttribute("Full path") == "Simulator/Default/Device1/INT" + std::to_string(i + 1));
    CHECK(flow_file->getAttribute("NodeID"));
    CHECK(flow_file->getAttribute("NodeID type") == "numeric");
    CHECK(flow_file->getAttribute("Typename") == "Int32");
    CHECK(flow_file->getAttribute("Sourcetimestamp"));
    CHECK(controller.plan->getContent(flow_file) == std::to_string(i + 1));
  }

  auto flow_file = results.at(processors::FetchOPCProcessor::Success)[3];
  CHECK(flow_file->getAttribute("Browsename") == "INT4");
  CHECK(flow_file->getAttribute("Datasize") == "4");
  CHECK(flow_file->getAttribute("Full path") == "Simulator/Default/Device1/INT3/INT4");
  CHECK(flow_file->getAttribute("NodeID"));
  CHECK(flow_file->getAttribute("NodeID type") == "numeric");
  CHECK(flow_file->getAttribute("Typename") == "Int32");
  CHECK(flow_file->getAttribute("Sourcetimestamp"));
  CHECK(controller.plan->getContent(flow_file) == "4");
}

TEST_CASE("Test fetching using custom reference type id path", "[fetchopcprocessor]") {
  OpcUaTestServer server(4841);
  server.start();
  SingleProcessorTestController controller{std::make_unique<processors::FetchOPCProcessor>("FetchOPCProcessor")};
  auto fetch_opc_processor = controller.getProcessor();
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4841/");
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::NodeIDType.name, "Path");
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::NodeID.name, "Simulator/Default/Device1/INT3");
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::NameSpaceIndex.name, std::to_string(server.getNamespaceIndex()));
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::PathReferenceTypes.name, "Organizes/Organizes/HasComponent");

  const auto results = controller.trigger();
  REQUIRE(results.at(processors::FetchOPCProcessor::Failure).empty());
  REQUIRE(results.at(processors::FetchOPCProcessor::Success).size() == 2);
  auto flow_file = results.at(processors::FetchOPCProcessor::Success)[0];
  CHECK(flow_file->getAttribute("Browsename") == "INT3");
  CHECK(flow_file->getAttribute("Datasize") == "4");
  CHECK(flow_file->getAttribute("Full path") == "Simulator/Default/Device1/INT3");
  CHECK(flow_file->getAttribute("NodeID"));
  CHECK(flow_file->getAttribute("NodeID type") == "numeric");
  CHECK(flow_file->getAttribute("Typename") == "Int32");
  CHECK(flow_file->getAttribute("Sourcetimestamp"));
  CHECK(controller.plan->getContent(flow_file) == "3");
  flow_file = results.at(processors::FetchOPCProcessor::Success)[1];
  CHECK(flow_file->getAttribute("Browsename") == "INT4");
  CHECK(flow_file->getAttribute("Datasize") == "4");
  CHECK(flow_file->getAttribute("Full path") == "Simulator/Default/Device1/INT3/INT4");
  CHECK(flow_file->getAttribute("NodeID"));
  CHECK(flow_file->getAttribute("NodeID type") == "numeric");
  CHECK(flow_file->getAttribute("Typename") == "Int32");
  CHECK(flow_file->getAttribute("Sourcetimestamp"));
  CHECK(controller.plan->getContent(flow_file) == "4");
}

TEST_CASE("Test missing path reference types", "[fetchopcprocessor]") {
  SingleProcessorTestController controller{std::make_unique<processors::FetchOPCProcessor>("FetchOPCProcessor")};
  auto fetch_opc_processor = controller.getProcessor();
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4841/");
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::NodeIDType.name, "Path");
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::NodeID.name, "Simulator/Default/Device1/INT3");
  fetch_opc_processor->setProperty(processors::FetchOPCProcessor::PathReferenceTypes.name, "Organizes/Organizes");
  REQUIRE_THROWS_WITH(controller.trigger(), "Process Schedule Operation: Path reference types must be provided for each node pair in the path!");
}

TEST_CASE("Test username and password should both be provided", "[fetchopcprocessor]") {
  SingleProcessorTestController controller{std::make_unique<processors::FetchOPCProcessor>("FetchOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::FetchOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4841/");
  put_opc_processor->setProperty(processors::FetchOPCProcessor::Username.name, "user");
  put_opc_processor->setProperty(processors::FetchOPCProcessor::Password.name, "");

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Both or neither of Username and Password should be provided!");
}

TEST_CASE("Test certificate path and key path should both be provided", "[fetchopcprocessor]") {
  SingleProcessorTestController controller{std::make_unique<processors::FetchOPCProcessor>("FetchOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::FetchOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4841/");
  put_opc_processor->setProperty(processors::FetchOPCProcessor::CertificatePath.name, "cert");
  put_opc_processor->setProperty(processors::FetchOPCProcessor::KeyPath.name, "");

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: All or none of Certificate path and Key path should be provided!");
}

TEST_CASE("Test application uri should be provided if certificate is provided", "[fetchopcprocessor]") {
  SingleProcessorTestController controller{std::make_unique<processors::FetchOPCProcessor>("FetchOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::FetchOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4841/");
  put_opc_processor->setProperty(processors::FetchOPCProcessor::CertificatePath.name, "cert");
  put_opc_processor->setProperty(processors::FetchOPCProcessor::KeyPath.name, "key");

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Application URI must be provided if Certificate path is provided!");
}

TEST_CASE("Test certificate path must be valid", "[fetchopcprocessor]") {
  SingleProcessorTestController controller{std::make_unique<processors::FetchOPCProcessor>("FetchOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::FetchOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4841/");
  put_opc_processor->setProperty(processors::FetchOPCProcessor::CertificatePath.name, "/invalid/cert/path");
  put_opc_processor->setProperty(processors::FetchOPCProcessor::KeyPath.name, "key");
  put_opc_processor->setProperty(processors::FetchOPCProcessor::ApplicationURI.name, "appuri");

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Failed to load cert from path: /invalid/cert/path");
}

TEST_CASE("Test key path must be valid", "[fetchopcprocessor]") {
  SingleProcessorTestController controller{std::make_unique<processors::FetchOPCProcessor>("FetchOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  auto test_cert_path = controller.createTempDirectory() /  "test_cert.pem";
  {
    std::ofstream cert_file(test_cert_path);
    cert_file << "test";
    cert_file.close();
  }
  put_opc_processor->setProperty(processors::FetchOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4841/");
  put_opc_processor->setProperty(processors::FetchOPCProcessor::CertificatePath.name, test_cert_path.string());
  put_opc_processor->setProperty(processors::FetchOPCProcessor::KeyPath.name, "/invalid/key");
  put_opc_processor->setProperty(processors::FetchOPCProcessor::ApplicationURI.name, "appuri");

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Failed to load key from path: /invalid/key");
}

TEST_CASE("Test trusted certs path must be valid", "[fetchopcprocessor]") {
  SingleProcessorTestController controller{std::make_unique<processors::FetchOPCProcessor>("FetchOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  auto test_cert_path = controller.createTempDirectory() /  "test_cert.pem";
  {
    std::ofstream cert_file(test_cert_path);
    cert_file << "test";
    cert_file.close();
  }
  put_opc_processor->setProperty(processors::FetchOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4841/");
  put_opc_processor->setProperty(processors::FetchOPCProcessor::CertificatePath.name, test_cert_path.string());
  put_opc_processor->setProperty(processors::FetchOPCProcessor::KeyPath.name, test_cert_path.string());
  put_opc_processor->setProperty(processors::FetchOPCProcessor::TrustedPath.name, "/invalid/trusted");
  put_opc_processor->setProperty(processors::FetchOPCProcessor::ApplicationURI.name, "appuri");

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Failed to load trusted server certs from path: /invalid/trusted");
}

}  // namespace org::apache::nifi::minifi::test
