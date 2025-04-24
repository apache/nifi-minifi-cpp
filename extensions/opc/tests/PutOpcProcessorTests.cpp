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
#include "include/putopc.h"
#include "utils/StringUtils.h"
#include "unit/TestUtils.h"

namespace org::apache::nifi::minifi::test {

struct NodeData {
  uint8_t data;
  uint16_t namespace_index;
  uint32_t node_id;
  std::string browse_name;
  std::string path;
  std::string path_reference_types;
  std::string target_reference_type = "HasComponent";
};

void verifyCreatedNode(const NodeData& expected_node, SingleProcessorTestController& controller) {
  auto client = minifi::opc::Client::createClient(controller.getLogger(), "", {}, {}, {});
  REQUIRE(client->connect("opc.tcp://127.0.0.1:4840/") == UA_STATUSCODE_GOOD);
  std::vector<UA_NodeId> found_node_ids;
  std::vector<UA_UInt32> reference_types;

  if (!expected_node.path_reference_types.empty()) {
    auto ref_types = minifi::utils::string::split(expected_node.path_reference_types, "/");
    for (const auto& ref_type : ref_types) {
      reference_types.push_back(opc::mapOpcReferenceType(ref_type).value());
    }
  } else {
    for (size_t i = 0; i < minifi::utils::string::split(expected_node.path, "/").size() - 1; ++i) {
      reference_types.push_back(UA_NS0ID_ORGANIZES);
    }
  }
  reference_types.push_back(opc::mapOpcReferenceType(expected_node.target_reference_type).value());

  REQUIRE(utils::verifyEventHappenedInPollTime(5s, [&] {
    client->translateBrowsePathsToNodeIdsRequest(expected_node.path + "/" + expected_node.browse_name, found_node_ids, expected_node.namespace_index, reference_types, controller.getLogger());
    return !found_node_ids.empty();
  }, 100ms));

  REQUIRE(found_node_ids.size() == 1);
  REQUIRE(found_node_ids[0].namespaceIndex == expected_node.namespace_index);
  REQUIRE(found_node_ids[0].identifierType == UA_NODEIDTYPE_NUMERIC);
  REQUIRE(found_node_ids[0].identifier.numeric == expected_node.node_id);  // NOLINT(cppcoreguidelines-pro-type-union-access)

  UA_ReferenceDescription ref_desc;
  ref_desc.isForward = true;
  ref_desc.referenceTypeId = UA_NODEID_NUMERIC(0, UA_NODEIDTYPE_NUMERIC);
  ref_desc.nodeId.nodeId = found_node_ids[0];
  ref_desc.browseName = UA_QUALIFIEDNAME_ALLOC(expected_node.namespace_index, expected_node.browse_name.c_str());
  ref_desc.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", expected_node.browse_name.c_str());
  ref_desc.nodeClass = UA_NODECLASS_VARIABLE;
  ref_desc.typeDefinition.nodeId = UA_NODEID_NUMERIC(0, UA_NODEIDTYPE_NUMERIC);
  auto data = client->getNodeData(&ref_desc, expected_node.path);
  CHECK(data.data[0] == expected_node.data);
}

TEST_CASE("Test creating a new node with path node id", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();

  NodeData expected_node{42, server.getNamespaceIndex(), 9999, "everything", "Simulator/Default/Device1", {}};
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Path");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, expected_node.path);
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex.name, std::to_string(expected_node.namespace_index));
  put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "Int");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, std::to_string(expected_node.node_id));
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, std::to_string(expected_node.namespace_index));
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, expected_node.browse_name);

  const auto results = controller.trigger(std::to_string(expected_node.data));
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Success).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Success)[0];
  CHECK(controller.plan->getContent(flow_file) == std::to_string(expected_node.data));
  verifyCreatedNode(expected_node, controller);
}

TEST_CASE("Test fetching using custom reference type id path", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();

  NodeData expected_node{42, server.getNamespaceIndex(), 9999, "everything", "Simulator/Default/Device1/INT3/INT4", "Organizes/Organizes/HasComponent/HasComponent"};
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Path");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, expected_node.path);
  put_opc_processor->setProperty(processors::PutOPCProcessor::PathReferenceTypes.name, expected_node.path_reference_types);
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex.name, std::to_string(expected_node.namespace_index));
  put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "Int");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, std::to_string(expected_node.node_id));
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, std::to_string(expected_node.namespace_index));
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, expected_node.browse_name);

  const auto results = controller.trigger(std::to_string(expected_node.data));
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Success).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Success)[0];
  CHECK(controller.plan->getContent(flow_file) == std::to_string(expected_node.data));
  verifyCreatedNode(expected_node, controller);
}

TEST_CASE("Test fetching using custom target reference type id", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();

  NodeData expected_node{42, server.getNamespaceIndex(), 9999, "everything", "Simulator/Default/Device1/INT3", "Organizes/Organizes/HasComponent", "Organizes"};
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Path");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, expected_node.path);
  put_opc_processor->setProperty(processors::PutOPCProcessor::PathReferenceTypes.name, expected_node.path_reference_types);
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex.name, std::to_string(expected_node.namespace_index));
  put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "Int");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, std::to_string(expected_node.node_id));
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, std::to_string(expected_node.namespace_index));
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, expected_node.browse_name);
  put_opc_processor->setProperty(processors::PutOPCProcessor::CreateNodeReferenceType.name, expected_node.target_reference_type);

  const auto results = controller.trigger(std::to_string(expected_node.data));
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Success).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Success)[0];
  CHECK(controller.plan->getContent(flow_file) == std::to_string(expected_node.data));
  verifyCreatedNode(expected_node, controller);
}

TEST_CASE("Test missing path reference types", "[putopcprocessor]") {
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Path");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, "Simulator/Default/Device1/INT3/INT4");
  put_opc_processor->setProperty(processors::PutOPCProcessor::PathReferenceTypes.name, "Organizes/Organizes/HasComponent");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "Int");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, "9999");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, "2");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, "everything");

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Path reference types must be provided for each node pair in the path!");
}

TEST_CASE("Test namespace cannot be empty", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  LogTestController::getInstance().setTrace<processors::PutOPCProcessor>();
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Path");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, "Simulator/Default/Device1/INT3/INT4");
  put_opc_processor->setProperty(processors::PutOPCProcessor::PathReferenceTypes.name, "Organizes/Organizes/HasComponent/HasComponent");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex.name, "2");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, "everything");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, "${missing}");

  const auto results = controller.trigger("42");
  REQUIRE(results.at(processors::PutOPCProcessor::Success).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Failure)[0];
  CHECK(controller.plan->getContent(flow_file) == "42");
  REQUIRE(LogTestController::getInstance().contains("had no target namespace index specified, routing to failure"));
}

TEST_CASE("Test valid namespace being required", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  LogTestController::getInstance().setTrace<processors::PutOPCProcessor>();
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Path");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, "Simulator/Default/Device1/INT3/INT4");
  put_opc_processor->setProperty(processors::PutOPCProcessor::PathReferenceTypes.name, "Organizes/Organizes/HasComponent/HasComponent");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex.name, std::to_string(server.getNamespaceIndex()));
  put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, "everything");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, "invalid_index");

  const auto results = controller.trigger("42");
  REQUIRE(results.at(processors::PutOPCProcessor::Success).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Failure)[0];
  CHECK(controller.plan->getContent(flow_file) == "42");
  REQUIRE(LogTestController::getInstance().contains("has invalid namespace index (invalid_index), routing to failure"));
}

TEST_CASE("Test username and password should both be provided", "[putopcprocessor]") {
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::Username.name, "user");
  put_opc_processor->setProperty(processors::PutOPCProcessor::Password.name, "");

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Both or neither of Username and Password should be provided!");
}

TEST_CASE("Test certificate path and key path should both be provided", "[putopcprocessor]") {
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::CertificatePath.name, "cert");
  put_opc_processor->setProperty(processors::PutOPCProcessor::KeyPath.name, "");

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: All or none of Certificate path and Key path should be provided!");
}

TEST_CASE("Test application uri should be provided if certificate is provided", "[putopcprocessor]") {
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::CertificatePath.name, "cert");
  put_opc_processor->setProperty(processors::PutOPCProcessor::KeyPath.name, "key");

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Application URI must be provided if Certificate path is provided!");
}

TEST_CASE("Test certificate path must be valid", "[putopcprocessor]") {
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::CertificatePath.name, "/invalid/cert/path");
  put_opc_processor->setProperty(processors::PutOPCProcessor::KeyPath.name, "key");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ApplicationURI.name, "appuri");

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Failed to load cert from path: /invalid/cert/path");
}

TEST_CASE("Test key path must be valid", "[putopcprocessor]") {
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  auto test_cert_path = controller.createTempDirectory() / "test_cert.pem";
  {
    std::ofstream cert_file(test_cert_path);
    cert_file << "test";
    cert_file.close();
  }
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::CertificatePath.name, test_cert_path.string());
  put_opc_processor->setProperty(processors::PutOPCProcessor::KeyPath.name, "/invalid/key");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ApplicationURI.name, "appuri");

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Failed to load key from path: /invalid/key");
}

TEST_CASE("Test trusted certs path must be valid", "[putopcprocessor]") {
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  auto test_cert_path = controller.createTempDirectory() / "test_cert.pem";
  {
    std::ofstream cert_file(test_cert_path);
    cert_file << "test";
    cert_file.close();
  }
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::CertificatePath.name, test_cert_path.string());
  put_opc_processor->setProperty(processors::PutOPCProcessor::KeyPath.name, test_cert_path.string());
  put_opc_processor->setProperty(processors::PutOPCProcessor::TrustedPath.name, "/invalid/trusted");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ApplicationURI.name, "appuri");

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Failed to load trusted server certs from path: /invalid/trusted");
}

TEST_CASE("Test invalid int node id", "[putopcprocessor]") {
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Int");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, "Simulator/Default/Device1");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32");

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Simulator/Default/Device1 cannot be used as an int type node ID");
}

TEST_CASE("Test invalid parent node id path", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Path");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, "Simulator/Default/Device1/INT99");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex.name, std::to_string(server.getNamespaceIndex()));
  put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "Int");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, "9999");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, std::to_string(server.getNamespaceIndex()));
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, "everything");

  const auto results = controller.trigger("42");
  REQUIRE(results.at(processors::PutOPCProcessor::Success).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).empty());
  REQUIRE(LogTestController::getInstance().contains("to node id, no flow files will be put"));
}

TEST_CASE("Test missing target node id", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Path");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, "Simulator/Default/Device1");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex.name, std::to_string(server.getNamespaceIndex()));
  put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "Int");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, "${missing}");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, std::to_string(server.getNamespaceIndex()));
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, "everything");

  const auto results = controller.trigger("42");
  REQUIRE(results.at(processors::PutOPCProcessor::Success).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Failure)[0];
  CHECK(controller.plan->getContent(flow_file) == "42");
  REQUIRE(LogTestController::getInstance().contains("had target node ID type specified (Int) without ID, routing to failure"));
}

TEST_CASE("Test invalid target node id", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Path");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, "Simulator/Default/Device1");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex.name, std::to_string(server.getNamespaceIndex()));
  put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "Int");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, "invalid_int");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, std::to_string(server.getNamespaceIndex()));
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, "everything");

  const auto results = controller.trigger("42");
  REQUIRE(results.at(processors::PutOPCProcessor::Success).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Failure)[0];
  CHECK(controller.plan->getContent(flow_file) == "42");
  REQUIRE(LogTestController::getInstance().contains("target node ID is not a valid integer: invalid_int. Routing to failure"));
}

TEST_CASE("Test missing target node type", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Path");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, "Simulator/Default/Device1");
  put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex.name, std::to_string(server.getNamespaceIndex()));
  put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "${missing}");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, "9999");
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, std::to_string(server.getNamespaceIndex()));
  put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, "everything");

  const auto results = controller.trigger("42", {{"invalid_type", "invalid"}});
  REQUIRE(results.at(processors::PutOPCProcessor::Success).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Failure)[0];
  CHECK(controller.plan->getContent(flow_file) == "42");
  REQUIRE(LogTestController::getInstance().contains("has invalid target node id type, routing to failure"));
}

TEST_CASE("Test value type mismatch", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{std::make_unique<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  CHECK(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  CHECK(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Path"));
  CHECK(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, "Simulator/Default/Device1"));
  CHECK(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));
  CHECK(put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Boolean"));
  CHECK(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "Int"));
  CHECK(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, "9999"));
  CHECK(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));
  CHECK(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, "everything"));

  const auto results = controller.trigger("42");
  REQUIRE(results.at(processors::PutOPCProcessor::Success).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Failure)[0];
  CHECK(controller.plan->getContent(flow_file) == "42");
  REQUIRE(LogTestController::getInstance().contains("Failed to convert 42 to data type Boolean"));
}

}  // namespace org::apache::nifi::minifi::test
