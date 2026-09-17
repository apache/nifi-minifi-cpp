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
#include <optional>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "OpcUaTestServer.h"
#include "unit/SingleProcessorTestController.h"
#include "include/PutOPCProcessor.h"
#include "utils/StringUtils.h"
#include "unit/TestUtils.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::test {

struct NodeData {
  uint8_t data = 0;
  uint16_t namespace_index = 0;
  uint32_t node_id = 0;
  std::string browse_name = {};
  std::string path = {};
  std::string path_reference_types = {};
  std::string target_reference_type = "HasComponent";
  std::optional<std::string> guid_node_id = std::nullopt;
};

void verifyCreatedNode(const NodeData& expected_node, SingleProcessorTestController& controller) {
  auto client = minifi::opc::Client::createClient(controller.getLogger(), "", {}, {}, {});
  REQUIRE(client->connect("opc.tcp://127.0.0.1:4840/") == UA_STATUSCODE_GOOD);
  std::vector<opc::NodeId> found_node_ids;
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
  REQUIRE(found_node_ids[0].get().namespaceIndex == expected_node.namespace_index);
  if (expected_node.guid_node_id) {
    REQUIRE(found_node_ids[0].get().identifierType == UA_NODEIDTYPE_GUID);
    UA_Guid expected_guid;
    REQUIRE(UA_Guid_parse(&expected_guid, UA_STRING(const_cast<char*>(expected_node.guid_node_id->c_str()))) == UA_STATUSCODE_GOOD);
    CHECK(UA_Guid_equal(&found_node_ids[0].get().identifier.guid, &expected_guid));  // NOLINT(cppcoreguidelines-pro-type-union-access)
  } else {
    REQUIRE(found_node_ids[0].get().identifierType == UA_NODEIDTYPE_NUMERIC);
    REQUIRE(found_node_ids[0].get().identifier.numeric == expected_node.node_id);  // NOLINT(cppcoreguidelines-pro-type-union-access)
  }

  UA_ReferenceDescription ref_desc;
  ref_desc.isForward = true;
  ref_desc.referenceTypeId = UA_NODEID_NUMERIC(0, UA_NODEIDTYPE_NUMERIC);
  ref_desc.nodeId.nodeId = found_node_ids[0].get();
  ref_desc.browseName = UA_QUALIFIEDNAME_ALLOC(expected_node.namespace_index, expected_node.browse_name.c_str());
  ref_desc.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", expected_node.browse_name.c_str());
  const auto ref_desc_guard = gsl::finally([&ref_desc] {
    UA_LocalizedText_clear(&ref_desc.displayName);
    UA_QualifiedName_clear(&ref_desc.browseName);
  });
  ref_desc.nodeClass = UA_NODECLASS_VARIABLE;
  ref_desc.typeDefinition.nodeId = UA_NODEID_NUMERIC(0, UA_NODEIDTYPE_NUMERIC);
  auto data = client->getNodeData(&ref_desc, expected_node.path);
  CHECK(data.data[0] == expected_node.data);
}

void verifyNodeValue(SingleProcessorTestController& controller, const opc::NodeId& target, uint16_t namespace_index, const std::string& browse_name, uint8_t expected_value) {
  auto client = minifi::opc::Client::createClient(controller.getLogger(), "", {}, {}, {});
  REQUIRE(client->connect("opc.tcp://127.0.0.1:4840/") == UA_STATUSCODE_GOOD);

  UA_ReferenceDescription ref_desc;
  ref_desc.isForward = true;
  ref_desc.referenceTypeId = UA_NODEID_NUMERIC(0, UA_NODEIDTYPE_NUMERIC);
  ref_desc.nodeId.nodeId = target.get();
  ref_desc.browseName = UA_QUALIFIEDNAME_ALLOC(namespace_index, browse_name.c_str());
  ref_desc.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", browse_name.c_str());
  const auto ref_desc_guard = gsl::finally([&ref_desc] {
    UA_LocalizedText_clear(&ref_desc.displayName);
    UA_QualifiedName_clear(&ref_desc.browseName);
  });
  ref_desc.nodeClass = UA_NODECLASS_VARIABLE;
  ref_desc.typeDefinition.nodeId = UA_NODEID_NUMERIC(0, UA_NODEIDTYPE_NUMERIC);
  auto data = client->getNodeData(&ref_desc, "");
  CHECK(data.data[0] == expected_value);
}

TEST_CASE("Test creating a new node with path node id", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();

  NodeData expected_node{42, server.getNamespaceIndex(), 9999, "everything", "Simulator/Default/Device1", {}};
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Path"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, expected_node.path));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex.name, std::to_string(expected_node.namespace_index)));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "Int"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, std::to_string(expected_node.node_id)));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, std::to_string(expected_node.namespace_index)));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, expected_node.browse_name));

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
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();

  NodeData expected_node{42, server.getNamespaceIndex(), 9999, "everything", "Simulator/Default/Device1/INT3/INT4", "Organizes/Organizes/HasComponent/HasComponent"};
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Path"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, expected_node.path));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::PathReferenceTypes.name, expected_node.path_reference_types));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex.name, std::to_string(expected_node.namespace_index)));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "Int"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, std::to_string(expected_node.node_id)));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, std::to_string(expected_node.namespace_index)));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, expected_node.browse_name));

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
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();

  NodeData expected_node{42, server.getNamespaceIndex(), 9999, "everything", "Simulator/Default/Device1/INT3", "Organizes/Organizes/HasComponent", "Organizes"};
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Path"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, expected_node.path));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::PathReferenceTypes.name, expected_node.path_reference_types));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex.name, std::to_string(expected_node.namespace_index)));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "Int"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, std::to_string(expected_node.node_id)));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, std::to_string(expected_node.namespace_index)));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, expected_node.browse_name));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::CreateNodeReferenceType.name, expected_node.target_reference_type));

  const auto results = controller.trigger(std::to_string(expected_node.data));
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Success).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Success)[0];
  CHECK(controller.plan->getContent(flow_file) == std::to_string(expected_node.data));
  verifyCreatedNode(expected_node, controller);
}

TEST_CASE("Test missing path reference types", "[putopcprocessor]") {
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Path"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, "Simulator/Default/Device1/INT3/INT4"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::PathReferenceTypes.name, "Organizes/Organizes/HasComponent"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "Int"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, "9999"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, "2"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, "everything"));

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Path reference types must be provided for each node pair in the path!");
}

TEST_CASE("Test namespace cannot be empty", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  LogTestController::getInstance().setTrace<processors::PutOPCProcessor>();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Path"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, "Simulator/Default/Device1/INT3/INT4"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::PathReferenceTypes.name, "Organizes/Organizes/HasComponent/HasComponent"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex.name, "2"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, "everything"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, "${missing}"));

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
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Path"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, "Simulator/Default/Device1/INT3/INT4"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::PathReferenceTypes.name, "Organizes/Organizes/HasComponent/HasComponent"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, "everything"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, "invalid_index"));

  const auto results = controller.trigger("42");
  REQUIRE(results.at(processors::PutOPCProcessor::Success).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Failure)[0];
  CHECK(controller.plan->getContent(flow_file) == "42");
  REQUIRE(LogTestController::getInstance().contains("has invalid namespace index (invalid_index), routing to failure"));
}

TEST_CASE("Test username and password should both be provided", "[putopcprocessor]") {
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::Username.name, "user"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::Password.name, ""));

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Both or neither of Username and Password should be provided!");
}

TEST_CASE("Test certificate path and key path should both be provided", "[putopcprocessor]") {
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::CertificatePath.name, "cert"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::KeyPath.name, ""));

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: All or none of Certificate path and Key path should be provided!");
}

TEST_CASE("Test application uri should be provided if certificate is provided", "[putopcprocessor]") {
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::CertificatePath.name, "cert"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::KeyPath.name, "key"));

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Application URI must be provided if Certificate path is provided!");
}

TEST_CASE("Test certificate path must be valid", "[putopcprocessor]") {
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::CertificatePath.name, "/invalid/cert/path"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::KeyPath.name, "key"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ApplicationURI.name, "appuri"));

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Failed to load cert from path: /invalid/cert/path");
}

TEST_CASE("Test key path must be valid", "[putopcprocessor]") {
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  auto test_cert_path = controller.createTempDirectory() / "test_cert.pem";
  {
    std::ofstream cert_file(test_cert_path);
    cert_file << "test";
    cert_file.close();
  }
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::CertificatePath.name, test_cert_path.string()));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::KeyPath.name, "/invalid/key"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ApplicationURI.name, "appuri"));

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Failed to load key from path: /invalid/key");
}

TEST_CASE("Test trusted certs path must be valid", "[putopcprocessor]") {
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  auto test_cert_path = controller.createTempDirectory() / "test_cert.pem";
  {
    std::ofstream cert_file(test_cert_path);
    cert_file << "test";
    cert_file.close();
  }
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::CertificatePath.name, test_cert_path.string()));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::KeyPath.name, test_cert_path.string()));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TrustedPath.name, "/invalid/trusted"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ApplicationURI.name, "appuri"));

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Failed to load trusted server certs from path: /invalid/trusted");
}

TEST_CASE("Test invalid int node id", "[putopcprocessor]") {
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Int"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, "Simulator/Default/Device1"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32"));

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: Simulator/Default/Device1 cannot be used as an int type node ID");
}

TEST_CASE("Test invalid parent node id path", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Path"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, "Simulator/Default/Device1/INT99"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "Int"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, "9999"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, "everything"));

  const auto results = controller.trigger("42");
  REQUIRE(results.at(processors::PutOPCProcessor::Success).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).empty());
  REQUIRE(LogTestController::getInstance().contains("Failed to translate path 'Simulator/Default/Device1/INT99' to a node id: BadNoDataAvailable"));
}

TEST_CASE("Test missing target node id", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Path"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, "Simulator/Default/Device1"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "Int"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, "${missing}"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, "everything"));

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
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Path"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, "Simulator/Default/Device1"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "Int"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, "invalid_int"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, "everything"));

  const auto results = controller.trigger("42");
  REQUIRE(results.at(processors::PutOPCProcessor::Success).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Failure)[0];
  CHECK(controller.plan->getContent(flow_file) == "42");
  REQUIRE(LogTestController::getInstance().contains("invalid_int cannot be used as an int type node ID. Routing to failure!"));
}

TEST_CASE("Test missing target node type", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Path"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, "Simulator/Default/Device1"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "${invalid_type}"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, "9999"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, "everything"));

  const auto results = controller.trigger("42", {{"invalid_type", "invalid"}});
  REQUIRE(results.at(processors::PutOPCProcessor::Success).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Failure)[0];
  CHECK(controller.plan->getContent(flow_file) == "42");
  REQUIRE(LogTestController::getInstance().contains("has invalid target node id type 'invalid', routing to failure!"));
}

TEST_CASE("Test value type mismatch", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
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

TEST_CASE("Test creating a new node with GUID target node id", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();

  NodeData expected_node{.data = 42, .namespace_index = server.getNamespaceIndex(), .browse_name = "everything", .path = "Simulator/Default/Device1",
      .guid_node_id = "12345678-1234-1234-1234-123456789abc"};
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Path"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, expected_node.path));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex.name, std::to_string(expected_node.namespace_index)));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "Guid"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, *expected_node.guid_node_id));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, std::to_string(expected_node.namespace_index)));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, expected_node.browse_name));

  const auto results = controller.trigger(std::to_string(expected_node.data));
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Success).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Success)[0];
  CHECK(controller.plan->getContent(flow_file) == std::to_string(expected_node.data));
  verifyCreatedNode(expected_node, controller);
}

TEST_CASE("Test creating a new node under a GUID parent node id", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();

  const std::string parent_guid = "aabbccdd-1122-3344-5566-778899aabbcc";
  NodeData expected_node{.data = 42, .namespace_index = server.getNamespaceIndex(), .node_id = 8888, .browse_name = "everything",
      .path = "Simulator/Default/Device2/GuidObject", .path_reference_types = "Organizes/Organizes/Organizes"};
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Guid"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, parent_guid));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex.name, std::to_string(expected_node.namespace_index)));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "Int"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, std::to_string(expected_node.node_id)));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, std::to_string(expected_node.namespace_index)));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, expected_node.browse_name));

  const auto results = controller.trigger(std::to_string(expected_node.data));
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Success).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Success)[0];
  CHECK(controller.plan->getContent(flow_file) == std::to_string(expected_node.data));
  verifyCreatedNode(expected_node, controller);
}

TEST_CASE("Test invalid GUID parent node id", "[putopcprocessor]") {
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Guid"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, "not-a-guid"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32"));

  REQUIRE_THROWS_WITH(controller.trigger("42"), "Process Schedule Operation: not-a-guid cannot be used as a GUID type node ID");
}

TEST_CASE("Test invalid GUID target node id", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Path"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, "Simulator/Default/Device1"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "Guid"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, "not-a-guid"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, "everything"));

  const auto results = controller.trigger("42");
  REQUIRE(results.at(processors::PutOPCProcessor::Success).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).size() == 1);
  REQUIRE(LogTestController::getInstance().contains("not-a-guid cannot be used as a GUID type node ID"));
}

TEST_CASE("Test updating an existing Int node without a parent node defined", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();

  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "Int"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, "666"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));

  const auto results = controller.trigger("123");
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Success).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Success)[0];
  CHECK(controller.plan->getContent(flow_file) == "123");
  verifyNodeValue(controller, opc::NodeId{UA_NODEID_NUMERIC(server.getNamespaceIndex(), 666)}, server.getNamespaceIndex(), "666", 123);
}

TEST_CASE("Test updating an existing String node without a parent node defined", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();

  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "String"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, "the.answer.node"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));

  const auto results = controller.trigger("55");
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Success).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Success)[0];
  CHECK(controller.plan->getContent(flow_file) == "55");
  verifyNodeValue(controller, opc::NodeId{UA_NODEID_STRING_ALLOC(server.getNamespaceIndex(), "the.answer.node")}, server.getNamespaceIndex(), "StringNode", 55);
}

TEST_CASE("Test updating an existing GUID node without a parent node defined", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();

  const std::string target_guid = "72962b91-fa75-4ae6-8d28-b404dc7daf63";
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "Guid"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, target_guid));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));

  const auto results = controller.trigger("99");
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Success).size() == 1);
  auto flow_file = results.at(processors::PutOPCProcessor::Success)[0];
  CHECK(controller.plan->getContent(flow_file) == "99");
  UA_Guid guid;
  REQUIRE(UA_Guid_parse(&guid, UA_STRING(const_cast<char*>(target_guid.c_str()))) == UA_STATUSCODE_GOOD);
  verifyNodeValue(controller, opc::NodeId{UA_NODEID_GUID(server.getNamespaceIndex(), guid)}, server.getNamespaceIndex(), target_guid, 99);
}

TEST_CASE("Test creating a new node fails without a parent node defined", "[putopcprocessor]") {
  LogTestController::getInstance().setError<processors::PutOPCProcessor>();
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();

  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "Int"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, "9999"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, std::to_string(server.getNamespaceIndex())));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, "everything"));

  const auto results = controller.trigger("42");
  REQUIRE(results.at(processors::PutOPCProcessor::Success).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).size() == 1);
  REQUIRE(LogTestController::getInstance().contains("no parent node is defined"));
}

TEST_CASE("Test multiple nodes are returned for path", "[putopcprocessor]") {
  OpcUaTestServer server;
  server.start();
  SingleProcessorTestController controller{minifi::test::utils::make_processor<processors::PutOPCProcessor>("PutOPCProcessor")};
  auto put_opc_processor = controller.getProcessor();

  NodeData expected_node{42, server.getNamespaceIndex(), 9999, "everything", "Simulator/Default/AmbiguousParent/Ambiguous", "Organizes/Organizes/Organizes"};
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4840/"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeIDType.name, "Path"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNodeID.name, expected_node.path));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::PathReferenceTypes.name, expected_node.path_reference_types));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ParentNameSpaceIndex.name, std::to_string(expected_node.namespace_index)));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::ValueType.name, "Int32"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeIDType.name, "Int"));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeID.name, std::to_string(expected_node.node_id)));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeNameSpaceIndex.name, std::to_string(expected_node.namespace_index)));
  REQUIRE(put_opc_processor->setProperty(processors::PutOPCProcessor::TargetNodeBrowseName.name, expected_node.browse_name));

  const auto results = controller.trigger(std::to_string(expected_node.data));
  REQUIRE(results.at(processors::PutOPCProcessor::Success).empty());
  REQUIRE(results.at(processors::PutOPCProcessor::Failure).empty());
  REQUIRE(LogTestController::getInstance().contains("exactly one target node is required for put"));
}

}  // namespace org::apache::nifi::minifi::test
