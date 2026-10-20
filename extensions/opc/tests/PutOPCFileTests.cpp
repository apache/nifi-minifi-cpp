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
#include <array>
#include <memory>
#include <string>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "OpcUaTestServer.h"
#include "unit/SingleProcessorTestController.h"
#include "include/PutOPCFile.h"
#include "unit/TestUtils.h"

namespace org::apache::nifi::minifi::test {

namespace {

constexpr const char* ENDPOINT = "opc.tcp://127.0.0.1:4845/";

class PutOPCFileTestFixture {
 public:
  PutOPCFileTestFixture() {
    files_node_ = server_.addFileDirectory("Files", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("Files")),
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER));
    server_.start();
    REQUIRE(processor_->setProperty(processors::PutOPCFile::OPCServerEndPoint.name, ENDPOINT));
  }

 protected:
  void setTargetNode(const std::string& node_id) {
    REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeIDType.name, "String"));
    REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeID.name, node_id));
    REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeNameSpaceIndex.name, std::to_string(server_.getNamespaceIndex())));
  }

  void setParentNode() {
    REQUIRE(processor_->setProperty(processors::PutOPCFile::ParentNodeIDType.name, "String"));
    REQUIRE(processor_->setProperty(processors::PutOPCFile::ParentNodeID.name, "Files"));
    REQUIRE(processor_->setProperty(processors::PutOPCFile::ParentNameSpaceIndex.name, std::to_string(server_.getNamespaceIndex())));
  }

  void addFile(const std::string& name, const std::string& content, bool writable = true) {
    server_.addFile(name, UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>(name.c_str())), files_node_.get(), content, std::nullopt, writable);
  }

  static std::shared_ptr<core::FlowFile> checkSuccess(const ProcessorTriggerResult& results) {
    CHECK(results.at(processors::PutOPCFile::Failure).empty());
    REQUIRE(results.at(processors::PutOPCFile::Success).size() == 1);
    return results.at(processors::PutOPCFile::Success)[0];
  }

  static void checkFailure(const ProcessorTriggerResult& results) {
    CHECK(results.at(processors::PutOPCFile::Success).empty());
    CHECK(results.at(processors::PutOPCFile::Failure).size() == 1);
  }

  OpcUaTestServer server_{4845};
  opc::NodeId files_node_;
  SingleProcessorTestController controller_{minifi::test::utils::make_processor<processors::PutOPCFile>("PutOPCFile")};
  core::Processor* processor_ = controller_.getProcessor();
};

}  // namespace

TEST_CASE_METHOD(PutOPCFileTestFixture, "Overwrite an existing file using its node id", "[putopcfile]") {
  addFile("test.txt", "old content which is longer");
  setTargetNode("test.txt");

  const auto flow_file = checkSuccess(controller_.trigger("new content"));
  CHECK(server_.getFileContent("test.txt") == "new content");
  CHECK(server_.getOpenFileHandleCount("test.txt") == 0);
  CHECK(flow_file->getAttribute("NodeID") == "test.txt");
  CHECK(flow_file->getAttribute("NodeID type") == "string");
  CHECK(flow_file->getAttribute("Namespace index") == std::to_string(server_.getNamespaceIndex()));
  CHECK(flow_file->getAttribute("Browsename") == "test.txt");
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Append to an existing file", "[putopcfile]") {
  addFile("test.txt", "first,");
  setTargetNode("test.txt");
  REQUIRE(processor_->setProperty(processors::PutOPCFile::WriteMode.name, "Append"));

  checkSuccess(controller_.trigger("second"));
  CHECK(server_.getFileContent("test.txt") == "first,second");
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Target node id is evaluated using expression language", "[putopcfile]") {
  addFile("test.txt", "");
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeIDType.name, "${node_id_type}"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeID.name, "${node_id}"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeNameSpaceIndex.name, "${namespace_index}"));

  checkSuccess(controller_.trigger("content", {{"node_id_type", "String"}, {"node_id", "test.txt"}, {"namespace_index", std::to_string(server_.getNamespaceIndex())}}));
  CHECK(server_.getFileContent("test.txt") == "content");
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Default chunk size of 32 KB is used if neither the file nor the server specifies MaxByteStringLength", "[putopcfile]") {
  std::string content;
  for (size_t i = 0; content.size() < 100000; ++i) {
    content += std::to_string(i) + ",";
  }
  addFile("large.txt", "");
  setTargetNode("large.txt");

  checkSuccess(controller_.trigger(content));
  CHECK(server_.getFileContent("large.txt") == content);
  CHECK(server_.getMaxChunkSize("large.txt") == 32768);
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Non-existent target node is created in the parent node and updated on the next put", "[putopcfile]") {
  setTargetNode("new.txt");
  setParentNode();
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeBrowseName.name, "${filename}"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::WriteMode.name, "Append"));

  const auto created_flow_file = checkSuccess(controller_.trigger("first,", {{"filename", "new.txt"}}));
  CHECK(server_.getFileContent("new.txt") == "first,");
  // The node id of the created file is assigned by the server
  CHECK(created_flow_file->getAttribute("NodeID type") == "numeric");
  CHECK(created_flow_file->getAttribute("Namespace index") == std::to_string(server_.getNamespaceIndex()));
  CHECK(created_flow_file->getAttribute("Browsename") == "new.txt");

  // The target node id still does not exist, the file created earlier is found by its browse name instead of creating it again
  const auto updated_flow_file = checkSuccess(controller_.trigger("second", {{"filename", "new.txt"}}));
  CHECK(server_.getFileContent("new.txt") == "first,second");
  CHECK(updated_flow_file->getAttribute("NodeID") == created_flow_file->getAttribute("NodeID"));
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "File is created in the parent node given by path without a target node id", "[putopcfile]") {
  REQUIRE(processor_->setProperty(processors::PutOPCFile::ParentNodeIDType.name, "Path"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::ParentNodeID.name, "Files"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::ParentNameSpaceIndex.name, std::to_string(server_.getNamespaceIndex())));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeBrowseName.name, "custom.txt"));

  checkSuccess(controller_.trigger("custom name content"));
  CHECK(server_.getFileContent("custom.txt") == "custom name content");
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Existing file is found by its browse name in the parent node without a target node id", "[putopcfile]") {
  addFile("existing.txt", "old");
  setParentNode();
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeBrowseName.name, "existing.txt"));

  SECTION("Overwrite") {
    const auto flow_file = checkSuccess(controller_.trigger("new"));
    CHECK(server_.getFileContent("existing.txt") == "new");
    CHECK(flow_file->getAttribute("NodeID") == "existing.txt");
  }
  SECTION("Append") {
    REQUIRE(processor_->setProperty(processors::PutOPCFile::WriteMode.name, "Append"));
    checkSuccess(controller_.trigger("new"));
    CHECK(server_.getFileContent("existing.txt") == "oldnew");
  }
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Existing files are found by their browse name on any page of the browse results of the parent node", "[putopcfile]") {
  const std::array<std::string, 3> file_names{"first.txt", "second.txt", "third.txt"};
  for (const auto& file_name : file_names) {
    addFile(file_name, "");
  }
  server_.setMaxReferencesPerNode(1);
  setParentNode();
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeBrowseName.name, "${filename}"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::WriteMode.name, "Append"));

  // The server holds 32 continuation points per session, which run out unless the ones of the pages not requested are released
  constexpr size_t ROUNDS = 20;
  for (size_t round = 0; round < ROUNDS; ++round) {
    for (const auto& file_name : file_names) {
      const auto flow_file = checkSuccess(controller_.trigger("x", {{"filename", file_name}}));
      CHECK(flow_file->getAttribute("NodeID") == file_name);
    }
  }
  for (const auto& file_name : file_names) {
    CHECK(server_.getFileContent(file_name) == std::string(ROUNDS, 'x'));
  }
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Existing file is found by its browse name in the parent node regardless of the namespace of the browse name", "[putopcfile]") {
  // The namespace index of the browse name of a file is server specific, it does not have to match the one of the directory
  constexpr UA_UInt16 BROWSE_NAME_NAMESPACE_INDEX = 0;
  server_.addFile("other_namespace.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("other_namespace.txt")), files_node_.get(), "old",
      std::nullopt, true, BROWSE_NAME_NAMESPACE_INDEX);
  setParentNode();
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeBrowseName.name, "other_namespace.txt"));

  const auto flow_file = checkSuccess(controller_.trigger("new"));
  CHECK(server_.getFileContent("other_namespace.txt") == "new");
  CHECK(flow_file->getAttribute("NodeID") == "other_namespace.txt");
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Non-existent target node without a parent node is routed to failure", "[putopcfile]") {
  setTargetNode("missing.txt");
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeBrowseName.name, "missing.txt"));

  checkFailure(controller_.trigger("content"));
  CHECK_FALSE(server_.getFileContent("missing.txt"));
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Non-existent target node without a browse name is routed to failure", "[putopcfile]") {
  setTargetNode("missing.txt");
  setParentNode();

  SECTION("Browse name is not set") {
  }
  SECTION("Browse name evaluates to an empty string") {
    REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeBrowseName.name, "${missing_attribute}"));
  }

  checkFailure(controller_.trigger("content"));
  CHECK_FALSE(server_.getFileContent("missing.txt"));
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Target node which is not a file is routed to failure", "[putopcfile]") {
  setTargetNode("Files");
  checkFailure(controller_.trigger("content"));
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Read-only file is routed to failure", "[putopcfile]") {
  addFile("readonly.txt", "original", false);
  setTargetNode("readonly.txt");

  checkFailure(controller_.trigger("content"));
  CHECK(server_.getFileContent("readonly.txt") == "original");
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Invalid target node id type is routed to failure", "[putopcfile]") {
  addFile("test.txt", "");
  setTargetNode("test.txt");
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeIDType.name, "Invalid"));

  checkFailure(controller_.trigger("content"));
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Write an existing file using a target path", "[putopcfile]") {
  addFile("test.txt", "old");
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeIDType.name, "Path"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeID.name, "${directory}/${filename}"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeNameSpaceIndex.name, std::to_string(server_.getNamespaceIndex())));

  const auto flow_file = checkSuccess(controller_.trigger("new", {{"directory", "Files"}, {"filename", "test.txt"}}));
  CHECK(server_.getFileContent("test.txt") == "new");
  CHECK(flow_file->getAttribute("NodeID") == "test.txt");
  CHECK(flow_file->getAttribute("Browsename") == "test.txt");
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Non-existent target path is routed to failure even if a parent node is set", "[putopcfile]") {
  setParentNode();
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeIDType.name, "Path"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeID.name, "Files/missing.txt"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeNameSpaceIndex.name, std::to_string(server_.getNamespaceIndex())));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeBrowseName.name, "missing.txt"));

  checkFailure(controller_.trigger("content"));
  CHECK_FALSE(server_.getFileContent("missing.txt"));
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Target path without any path elements is routed to failure", "[putopcfile]") {
  addFile("test.txt", "old");
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeIDType.name, "Path"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeNameSpaceIndex.name, std::to_string(server_.getNamespaceIndex())));

  SECTION("Path of separators only") {
    REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeID.name, " / / "));
  }
  SECTION("Path evaluated from missing attributes") {
    REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeID.name, "${directory}/${filename}"));
  }
  SECTION("Path of separators only with reference types") {
    REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeID.name, "/"));
    REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetPathReferenceTypes.name, "Organizes"));
  }

  ProcessorTriggerResult results;
  REQUIRE_NOTHROW(results = controller_.trigger("new"));
  checkFailure(results);
  CHECK(server_.getFileContent("test.txt") == "old");
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Target path with HasComponent reference type does not find a file organized by the directory", "[putopcfile]") {
  addFile("test.txt", "old");
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeIDType.name, "Path"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeID.name, "Files/test.txt"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeNameSpaceIndex.name, std::to_string(server_.getNamespaceIndex())));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetPathReferenceTypes.name, "HasComponent"));

  checkFailure(controller_.trigger("new"));
  CHECK(server_.getFileContent("test.txt") == "old");
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Invalid target path reference types are routed to failure", "[putopcfile]") {
  addFile("test.txt", "old");
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeIDType.name, "Path"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeID.name, "Files/test.txt"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeNameSpaceIndex.name, std::to_string(server_.getNamespaceIndex())));

  SECTION("Reference type count does not match the path") {
    REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetPathReferenceTypes.name, "Organizes/HasComponent"));
  }
  SECTION("Unsupported reference type") {
    REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetPathReferenceTypes.name, "HasChild"));
  }

  checkFailure(controller_.trigger("new"));
  CHECK(server_.getFileContent("test.txt") == "old");
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "File with the browse name of a method of the parent node is created", "[putopcfile]") {
  setParentNode();
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeBrowseName.name, "CreateFile"));

  checkSuccess(controller_.trigger("content"));
  CHECK(server_.getFileContent("CreateFile") == "content");
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Write an existing file using int and guid target node ids", "[putopcfile]") {
  SECTION("Int node id") {
    server_.addFile("int_file.txt", UA_NODEID_NUMERIC(server_.getNamespaceIndex(), 7777), files_node_.get(), "old");
    REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeIDType.name, "Int"));
    REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeID.name, "7777"));
    REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeNameSpaceIndex.name, std::to_string(server_.getNamespaceIndex())));

    const auto flow_file = checkSuccess(controller_.trigger("new"));
    CHECK(server_.getFileContent("int_file.txt") == "new");
    CHECK(flow_file->getAttribute("NodeID") == "7777");
    CHECK(flow_file->getAttribute("NodeID type") == "numeric");
  }
  SECTION("Guid node id") {
    server_.addFile("guid_file.txt", UA_NODEID_GUID(server_.getNamespaceIndex(), UA_GUID("11223344-5566-7788-99aa-bbccddeeff00")), files_node_.get(), "old");
    REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeIDType.name, "Guid"));
    REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeID.name, "11223344-5566-7788-99aa-bbccddeeff00"));
    REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeNameSpaceIndex.name, std::to_string(server_.getNamespaceIndex())));

    const auto flow_file = checkSuccess(controller_.trigger("new"));
    CHECK(server_.getFileContent("guid_file.txt") == "new");
    CHECK(flow_file->getAttribute("NodeID") == "11223344-5566-7788-99aa-bbccddeeff00");
    CHECK(flow_file->getAttribute("NodeID type") == "guid");
  }
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Invalid target node id properties are routed to failure", "[putopcfile]") {
  addFile("test.txt", "old");
  setParentNode();
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeBrowseName.name, "test.txt"));

  SECTION("Invalid namespace index") {
    setTargetNode("test.txt");
    REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeNameSpaceIndex.name, "${namespace_index}"));
  }
  SECTION("Target node id evaluates to an empty string, which does not fall back to the parent node") {
    setTargetNode("${missing_attribute}");
  }
  SECTION("Invalid guid") {
    setTargetNode("not-a-guid");
    REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeIDType.name, "Guid"));
  }
  SECTION("Non-numeric int") {
    setTargetNode("not-a-number");
    REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeIDType.name, "Int"));
  }

  checkFailure(controller_.trigger("new", {{"namespace_index", "abc"}}));
  CHECK(server_.getFileContent("test.txt") == "old");
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Target path resolving to multiple nodes is routed to failure", "[putopcfile]") {
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeIDType.name, "Path"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeID.name, "Simulator/Default/AmbiguousParent/Ambiguous"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeNameSpaceIndex.name, std::to_string(server_.getNamespaceIndex())));

  checkFailure(controller_.trigger("content"));
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Non-existent parent node is routed to failure", "[putopcfile]") {
  REQUIRE(processor_->setProperty(processors::PutOPCFile::ParentNodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::ParentNodeID.name, "MissingDirectory"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::ParentNameSpaceIndex.name, std::to_string(server_.getNamespaceIndex())));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeBrowseName.name, "new.txt"));

  checkFailure(controller_.trigger("content"));
  CHECK_FALSE(server_.getFileContent("new.txt"));
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Parent node which is not a directory is routed to failure", "[putopcfile]") {
  addFile("not_a_directory.txt", "");
  REQUIRE(processor_->setProperty(processors::PutOPCFile::ParentNodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::ParentNodeID.name, "not_a_directory.txt"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::ParentNameSpaceIndex.name, std::to_string(server_.getNamespaceIndex())));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeBrowseName.name, "new.txt"));

  checkFailure(controller_.trigger("content"));
  CHECK_FALSE(server_.getFileContent("new.txt"));
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Failing to create the file in the parent node is routed to failure", "[putopcfile]") {
  setParentNode();
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeBrowseName.name, "new.txt"));

  SECTION("CreateFile fails") {
    server_.setCreateFileFailure(UA_STATUSCODE_BADUSERACCESSDENIED);
    checkFailure(controller_.trigger("content"));
    CHECK_FALSE(server_.getFileContent("new.txt"));
  }
  SECTION("CreateFile returns the node id of the file with the wrong type") {
    server_.setCreateFileMalformedResponse(MalformedResponse::WrongOutputType);
    checkFailure(controller_.trigger("content"));
    CHECK(server_.getFileContent("new.txt") == "");
  }
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Unreachable server yields and keeps the flowfile in the queue", "[putopcfile]") {
  REQUIRE(processor_->setProperty(processors::PutOPCFile::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4899/"));
  setTargetNode("test.txt");

  ProcessorTriggerResult results;
  REQUIRE_NOTHROW(results = controller_.trigger("content"));
  CHECK(results.at(processors::PutOPCFile::Success).empty());
  CHECK(results.at(processors::PutOPCFile::Failure).empty());
  CHECK(processor_->isYield());
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Parent path which cannot be resolved is routed to failure", "[putopcfile]") {
  REQUIRE(processor_->setProperty(processors::PutOPCFile::ParentNodeIDType.name, "Path"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::ParentNameSpaceIndex.name, std::to_string(server_.getNamespaceIndex())));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeBrowseName.name, "new.txt"));

  SECTION("Parent path does not exist") {
    REQUIRE(processor_->setProperty(processors::PutOPCFile::ParentNodeID.name, "MissingDirectory"));
  }
  SECTION("Parent path resolves to multiple nodes") {
    REQUIRE(processor_->setProperty(processors::PutOPCFile::ParentNodeID.name, "Simulator/Default/AmbiguousParent/Ambiguous"));
  }

  checkFailure(controller_.trigger("content"));
  CHECK_FALSE(server_.getFileContent("new.txt"));
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Existing target node is written even if the parent path cannot be resolved", "[putopcfile]") {
  addFile("test.txt", "old");
  setTargetNode("test.txt");
  REQUIRE(processor_->setProperty(processors::PutOPCFile::ParentNodeIDType.name, "Path"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::ParentNodeID.name, "MissingDirectory"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::ParentNameSpaceIndex.name, std::to_string(server_.getNamespaceIndex())));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeBrowseName.name, "test.txt"));

  checkSuccess(controller_.trigger("new"));
  CHECK(server_.getFileContent("test.txt") == "new");
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Parent path reference types are used to resolve the parent path", "[putopcfile]") {
  server_.addFileDirectory("Subdirectory", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("Subdirectory")), files_node_.get());
  REQUIRE(processor_->setProperty(processors::PutOPCFile::ParentNodeIDType.name, "Path"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::ParentNodeID.name, "Files/Subdirectory"));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::ParentNameSpaceIndex.name, std::to_string(server_.getNamespaceIndex())));
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeBrowseName.name, "new.txt"));

  SECTION("Reference type of the directory") {
    REQUIRE(processor_->setProperty(processors::PutOPCFile::PathReferenceTypes.name, "Organizes"));
    checkSuccess(controller_.trigger("content"));
    CHECK(server_.getFileContent("new.txt") == "content");
  }
  SECTION("Reference type not used by the directory") {
    REQUIRE(processor_->setProperty(processors::PutOPCFile::PathReferenceTypes.name, "HasComponent"));
    checkFailure(controller_.trigger("content"));
    CHECK_FALSE(server_.getFileContent("new.txt"));
  }
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Existing target node is written instead of the file with the target browse name in the parent node", "[putopcfile]") {
  addFile("target.txt", "target");
  addFile("other.txt", "other");
  setTargetNode("target.txt");
  setParentNode();
  REQUIRE(processor_->setProperty(processors::PutOPCFile::TargetNodeBrowseName.name, "other.txt"));

  checkSuccess(controller_.trigger("new"));
  CHECK(server_.getFileContent("target.txt") == "new");
  CHECK(server_.getFileContent("other.txt") == "other");
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Empty flowfile content empties the file in overwrite mode", "[putopcfile]") {
  addFile("test.txt", "old content");
  setTargetNode("test.txt");

  checkSuccess(controller_.trigger(""));
  CHECK(server_.getFileContent("test.txt") == "");
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Write binary flowfile content", "[putopcfile]") {
  const std::string content("\x00\x01\xff\xfe\x00" "binary" "\x80\x00", 13);
  addFile("binary.bin", "");
  setTargetNode("binary.bin");

  checkSuccess(controller_.trigger(content));
  CHECK(server_.getFileContent("binary.bin") == content);
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Chunk size is determined by the MaxByteStringLength property of the file", "[putopcfile]") {
  const std::string content(1000, 'a');
  server_.setServerMaxByteStringLength(128);
  server_.addFile("limited.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("limited.txt")), files_node_.get(), "", 64);
  setTargetNode("limited.txt");

  checkSuccess(controller_.trigger(content));
  CHECK(server_.getFileContent("limited.txt") == content);
  CHECK(server_.getMaxChunkSize("limited.txt") == 64);
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Server MaxByteStringLength is used if the file does not specify one", "[putopcfile]") {
  const std::string content(1000, 'a');
  server_.setServerMaxByteStringLength(128);
  addFile("test.txt", "");
  setTargetNode("test.txt");

  checkSuccess(controller_.trigger(content));
  CHECK(server_.getFileContent("test.txt") == content);
  CHECK(server_.getMaxChunkSize("test.txt") == 128);
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Failing to write the file is routed to failure and closes the file", "[putopcfile]") {
  server_.addFile("test.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("test.txt")), files_node_.get(), "", 100);
  server_.setFileFailure("test.txt", FileMethod::Write, UA_STATUSCODE_BADINTERNALERROR, 1);
  setTargetNode("test.txt");

  checkFailure(controller_.trigger(std::string(1000, 'a')));
  CHECK(server_.getOpenFileHandleCount("test.txt") == 0);
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Failing to close the file after writing it is routed to failure", "[putopcfile]") {
  addFile("test.txt", "");
  server_.setFileFailure("test.txt", FileMethod::Close, UA_STATUSCODE_BADINTERNALERROR);
  setTargetNode("test.txt");

  checkFailure(controller_.trigger("content"));
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Either target node id or parent node has to be set", "[putopcfile]") {
  REQUIRE_THROWS_WITH(controller_.trigger("content"), "Process Schedule Operation: Either Target node ID or Parent node ID must be set");
}

TEST_CASE_METHOD(PutOPCFileTestFixture, "Parent node ID type has to be set if parent node ID is set", "[putopcfile]") {
  REQUIRE(processor_->setProperty(processors::PutOPCFile::ParentNodeID.name, "Files"));
  REQUIRE_THROWS_WITH(controller_.trigger("content"), "Process Schedule Operation: Property 'Parent node ID type' is missing");
}

}  // namespace org::apache::nifi::minifi::test
