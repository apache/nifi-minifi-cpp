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
#include <string>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "OpcUaTestServer.h"
#include "unit/SingleProcessorTestController.h"
#include "include/FetchOPCFile.h"
#include "unit/TestUtils.h"

namespace org::apache::nifi::minifi::test {

namespace {

constexpr const char* ENDPOINT = "opc.tcp://127.0.0.1:4844/";

class FetchOPCFileTestFixture {
 public:
  FetchOPCFileTestFixture() {
    files_node_ = server_.addFileDirectory("Files", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("Files")),
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER));
    server_.start();
    REQUIRE(processor_->setProperty(processors::FetchOPCFile::OPCServerEndPoint.name, ENDPOINT));
    REQUIRE(processor_->setProperty(processors::FetchOPCFile::NameSpaceIndex.name, std::to_string(server_.getNamespaceIndex())));
  }

 protected:
  OpcUaTestServer server_{4844};
  opc::NodeId files_node_;
  SingleProcessorTestController controller_{minifi::test::utils::make_processor<processors::FetchOPCFile>("FetchOPCFile")};
  core::Processor* processor_ = controller_.getProcessor();
};

}  // namespace

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Fetch file using string node id", "[fetchopcfile]") {
  server_.addFile("test.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("test.txt")), files_node_.get(), "file content");
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  const auto& flow_file = results.at(processors::FetchOPCFile::Success)[0];
  CHECK(controller_.plan->getContent(flow_file) == "file content");
  CHECK(flow_file->getAttribute("Browsename") == "test.txt");
  CHECK(flow_file->getAttribute("NodeID") == "test.txt");
  CHECK(flow_file->getAttribute("NodeID type") == "string");
  CHECK(flow_file->getAttribute("Namespace index") == std::to_string(server_.getNamespaceIndex()));
  CHECK(server_.getOpenFileHandleCount("test.txt") == 0);
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Fetch file using int node id", "[fetchopcfile]") {
  server_.addFile("int_file.txt", UA_NODEID_NUMERIC(server_.getNamespaceIndex(), 7777), files_node_.get(), "int content");
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "Int"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "7777"));

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  const auto& flow_file = results.at(processors::FetchOPCFile::Success)[0];
  CHECK(controller_.plan->getContent(flow_file) == "int content");
  CHECK(flow_file->getAttribute("NodeID") == "7777");
  CHECK(flow_file->getAttribute("NodeID type") == "numeric");
  CHECK(flow_file->getAttribute("Browsename") == "int_file.txt");
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Fetch file using guid node id", "[fetchopcfile]") {
  server_.addFile("guid_file.txt", UA_NODEID_GUID(server_.getNamespaceIndex(), UA_GUID("11223344-5566-7788-99aa-bbccddeeff00")), files_node_.get(), "guid content");
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "Guid"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "11223344-5566-7788-99aa-bbccddeeff00"));

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  const auto& flow_file = results.at(processors::FetchOPCFile::Success)[0];
  CHECK(controller_.plan->getContent(flow_file) == "guid content");
  CHECK(flow_file->getAttribute("NodeID") == "11223344-5566-7788-99aa-bbccddeeff00");
  CHECK(flow_file->getAttribute("NodeID type") == "guid");
  CHECK(flow_file->getAttribute("Browsename") == "guid_file.txt");
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Fetch file using path node id", "[fetchopcfile]") {
  server_.addFile("path_file.txt", UA_NODEID_NUMERIC(server_.getNamespaceIndex(), 0), files_node_.get(), "path content");
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "Path"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "Files/path_file.txt"));

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  const auto& flow_file = results.at(processors::FetchOPCFile::Success)[0];
  CHECK(controller_.plan->getContent(flow_file) == "path content");
  CHECK(flow_file->getAttribute("Browsename") == "path_file.txt");
  // The attributes describe the node the path is resolved to, which got a server assigned numeric node id
  CHECK(flow_file->getAttribute("NodeID type") == "numeric");
  CHECK(flow_file->getAttribute("Namespace index") == std::to_string(server_.getNamespaceIndex()));
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Default chunk size of 32 KB is used if neither the file nor the server specifies MaxByteStringLength", "[fetchopcfile]") {
  std::string content;
  for (size_t i = 0; content.size() < 100000; ++i) {
    content += std::to_string(i) + ",";
  }
  server_.addFile("large.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("large.txt")), files_node_.get(), content);
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "large.txt"));

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(controller_.plan->getContent(results.at(processors::FetchOPCFile::Success)[0]) == content);
  CHECK(server_.getMaxChunkSize("large.txt") == 32768);
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Chunk size is determined by the MaxByteStringLength property of the file", "[fetchopcfile]") {
  const std::string content(1000, 'a');
  server_.addFile("limited.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("limited.txt")), files_node_.get(), content, 64);
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "limited.txt"));

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(controller_.plan->getContent(results.at(processors::FetchOPCFile::Success)[0]) == content);
  CHECK(server_.getMaxChunkSize("limited.txt") == 64);
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Fetch empty file", "[fetchopcfile]") {
  server_.addFile("empty.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("empty.txt")), files_node_.get(), "");
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "empty.txt"));

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(controller_.plan->getContent(results.at(processors::FetchOPCFile::Success)[0]).empty());
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Fetching a non-existent file node yields without creating a flowfile", "[fetchopcfile]") {
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "missing.txt"));

  const auto results = controller_.trigger();
  CHECK(results.at(processors::FetchOPCFile::Success).empty());
  CHECK(processor_->isYield());
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Fetching a node which is not a file yields without creating a flowfile", "[fetchopcfile]") {
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "Files"));

  const auto results = controller_.trigger();
  CHECK(results.at(processors::FetchOPCFile::Success).empty());
  CHECK(processor_->isYield());
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Server MaxByteStringLength is used if the file does not specify one", "[fetchopcfile]") {
  const std::string content(1000, 'a');
  server_.setServerMaxByteStringLength(128);
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "file.txt"));

  SECTION("File has no MaxByteStringLength property") {
    server_.addFile("file.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("file.txt")), files_node_.get(), content);
  }
  SECTION("MaxByteStringLength property of the file is zero") {
    server_.addFile("file.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("file.txt")), files_node_.get(), content, 0);
  }

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(controller_.plan->getContent(results.at(processors::FetchOPCFile::Success)[0]) == content);
  CHECK(server_.getMaxChunkSize("file.txt") == 128);
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "File MaxByteStringLength takes precedence over the server's", "[fetchopcfile]") {
  const std::string content(1000, 'a');
  server_.setServerMaxByteStringLength(128);
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "file.txt"));

  size_t expected_chunk_size = 0;
  SECTION("File limit is smaller than the server's") {
    expected_chunk_size = 64;
  }
  SECTION("File limit is larger than the server's") {
    expected_chunk_size = 500;
  }
  server_.addFile("file.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("file.txt")), files_node_.get(), content,
      gsl::narrow<UA_UInt32>(expected_chunk_size));

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(controller_.plan->getContent(results.at(processors::FetchOPCFile::Success)[0]) == content);
  CHECK(server_.getMaxChunkSize("file.txt") == expected_chunk_size);
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "File metadata is cached between triggers", "[fetchopcfile]") {
  server_.addFile("test.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("test.txt")), files_node_.get(), std::string(1000, 'a'), 64);
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));

  REQUIRE(controller_.trigger().at(processors::FetchOPCFile::Success).size() == 1);
  server_.setFileMaxByteStringLength("test.txt", 200);
  REQUIRE(controller_.trigger().at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(server_.getMaxChunkSize("test.txt") == 64);
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Cached file metadata is refreshed after failing to open the file", "[fetchopcfile]") {
  server_.addFile("test.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("test.txt")), files_node_.get(), std::string(1000, 'a'), 64);
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));

  REQUIRE(controller_.trigger().at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(server_.getMaxChunkSize("test.txt") == 64);

  server_.setFileFailure("test.txt", FileMethod::Open, UA_STATUSCODE_BADINTERNALERROR);
  server_.setFileMaxByteStringLength("test.txt", 200);
  CHECK(controller_.trigger().at(processors::FetchOPCFile::Success).empty());
  CHECK(processor_->isYield());

  server_.clearFileFailure("test.txt", FileMethod::Open);
  REQUIRE(controller_.trigger().at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(server_.getMaxChunkSize("test.txt") == 200);
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Fetch binary file content", "[fetchopcfile]") {
  const std::string content("\x00\x01\xff\xfe\x00" "binary" "\x80\x00", 13);
  server_.addFile("binary.bin", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("binary.bin")), files_node_.get(), content);
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "binary.bin"));

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(controller_.plan->getContent(results.at(processors::FetchOPCFile::Success)[0]) == content);
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "File is fetched again on every trigger", "[fetchopcfile]") {
  server_.addFile("test.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("test.txt")), files_node_.get(), "file content");
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));

  for (size_t i = 0; i < 2; ++i) {
    const auto results = controller_.trigger();
    REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
    CHECK(controller_.plan->getContent(results.at(processors::FetchOPCFile::Success)[0]) == "file content");
  }
  CHECK(server_.getOpenFileHandleCount("test.txt") == 0);
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Unreachable server yields without creating a flowfile", "[fetchopcfile]") {
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::OPCServerEndPoint.name, "opc.tcp://127.0.0.1:4899/"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));

  const auto results = controller_.trigger();
  CHECK(results.at(processors::FetchOPCFile::Success).empty());
  CHECK(processor_->isYield());
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Path which cannot be resolved to a single node yields without creating a flowfile", "[fetchopcfile]") {
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "Path"));

  SECTION("Path does not exist") {
    REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "Files/missing.txt"));
  }
  SECTION("Path resolves to multiple nodes") {
    REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "Simulator/Default/AmbiguousParent/Ambiguous"));
  }

  const auto results = controller_.trigger();
  CHECK(results.at(processors::FetchOPCFile::Success).empty());
  CHECK(processor_->isYield());
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Failing to open the file yields without creating a flowfile", "[fetchopcfile]") {
  server_.addFile("test.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("test.txt")), files_node_.get(), "file content");
  server_.setFileFailure("test.txt", FileMethod::Open, UA_STATUSCODE_BADNOTREADABLE);
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));

  const auto results = controller_.trigger();
  CHECK(results.at(processors::FetchOPCFile::Success).empty());
  CHECK(processor_->isYield());
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Failing to read the file yields without creating a flowfile and closes the file", "[fetchopcfile]") {
  server_.addFile("test.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("test.txt")), files_node_.get(), std::string(1000, 'a'), 100);
  server_.setFileFailure("test.txt", FileMethod::Read, UA_STATUSCODE_BADINTERNALERROR, 1);
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));

  const auto results = controller_.trigger();
  CHECK(results.at(processors::FetchOPCFile::Success).empty());
  CHECK(processor_->isYield());
  CHECK(server_.getOpenFileHandleCount("test.txt") == 0);
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Failing to close the file after reading it still transfers the flowfile", "[fetchopcfile]") {
  LogTestController::getInstance().setWarn<processors::FetchOPCFile>();
  server_.addFile("test.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("test.txt")), files_node_.get(), "file content");
  server_.setFileFailure("test.txt", FileMethod::Close, UA_STATUSCODE_BADINTERNALERROR);
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(controller_.plan->getContent(results.at(processors::FetchOPCFile::Success)[0]) == "file content");
  CHECK(LogTestController::getInstance().contains("Failed to close file node 'test.txt' after reading it: BadInternalError"));
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Lazy mode only fetches the file again if its LastModifiedTime changes", "[fetchopcfile]") {
  LogTestController::getInstance().setDebug<processors::FetchOPCFile>();
  server_.addFile("test.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("test.txt")), files_node_.get(), "file content");
  server_.setFileLastModifiedTime("test.txt", 1000);
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::Lazy.name, "On"));

  REQUIRE(controller_.trigger().at(processors::FetchOPCFile::Success).size() == 1);

  CHECK(controller_.trigger().at(processors::FetchOPCFile::Success).empty());
  CHECK(processor_->isYield());
  CHECK(LogTestController::getInstance().contains("File node 'test.txt' has not changed since the last fetch, skipping"));

  // Same size, so only the LastModifiedTime signals the change
  server_.setFileContent("test.txt", "new  content");
  server_.setFileLastModifiedTime("test.txt", 2000);
  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(controller_.plan->getContent(results.at(processors::FetchOPCFile::Success)[0]) == "new  content");
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Lazy mode fetches the file again if its Size changes", "[fetchopcfile]") {
  server_.addFile("test.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("test.txt")), files_node_.get(), "file content");
  SECTION("File has an unchanged LastModifiedTime property") {
    server_.setFileLastModifiedTime("test.txt", 1000);
  }
  SECTION("File has no LastModifiedTime property") {
    // Only the Size property is available for change detection
  }
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::Lazy.name, "On"));

  REQUIRE(controller_.trigger().at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(controller_.trigger().at(processors::FetchOPCFile::Success).empty());

  server_.setFileContent("test.txt", "longer file content");
  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(controller_.plan->getContent(results.at(processors::FetchOPCFile::Success)[0]) == "longer file content");
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Lazy mode cannot detect same size changes of a file without LastModifiedTime property", "[fetchopcfile]") {
  LogTestController::getInstance().setWarn<processors::FetchOPCFile>();
  server_.addFile("test.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("test.txt")), files_node_.get(), "file content");
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::Lazy.name, "On"));

  REQUIRE(controller_.trigger().at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(LogTestController::getInstance().contains("File node 'test.txt' has no LastModifiedTime property, lazy mode can only detect the changes of its Size"));

  server_.setFileContent("test.txt", "new  content");
  CHECK(controller_.trigger().at(processors::FetchOPCFile::Success).empty());
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Lazy mode fetches the file on every trigger if it has neither LastModifiedTime nor Size property", "[fetchopcfile]") {
  LogTestController::getInstance().setWarn<processors::FetchOPCFile>();
  server_.addFile("test.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("test.txt")), files_node_.get(), "file content");
  server_.removeFileSizeProperty("test.txt");
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::Lazy.name, "On"));

  REQUIRE(controller_.trigger().at(processors::FetchOPCFile::Success).size() == 1);
  REQUIRE(controller_.trigger().at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(LogTestController::getInstance().contains(
      "File node 'test.txt' has neither LastModifiedTime nor Size property, lazy mode cannot detect its changes, it is fetched on every trigger"));
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Lazy mode does not store the state of a file that failed to be fetched", "[fetchopcfile]") {
  server_.addFile("test.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("test.txt")), files_node_.get(), "file content");
  server_.setFileLastModifiedTime("test.txt", 1000);
  server_.setFileFailure("test.txt", FileMethod::Read, UA_STATUSCODE_BADINTERNALERROR);
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::Lazy.name, "On"));

  CHECK(controller_.trigger().at(processors::FetchOPCFile::Success).empty());

  server_.clearFileFailure("test.txt", FileMethod::Read);
  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(controller_.plan->getContent(results.at(processors::FetchOPCFile::Success)[0]) == "file content");
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "File is opened for reading only", "[fetchopcfile]") {
  server_.addFile("test.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("test.txt")), files_node_.get(), "file content");
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));

  // The test server rejects opening its files for writing
  REQUIRE(controller_.trigger().at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(server_.getLastOpenMode("test.txt") == opc::FILE_MODE_READ);
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "End of file signaled with an empty variant", "[fetchopcfile]") {
  server_.addFile("test.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("test.txt")), files_node_.get(), std::string(1000, 'a'), 100);
  server_.setFileEndOfFileAsEmptyVariant("test.txt");
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(controller_.plan->getContent(results.at(processors::FetchOPCFile::Success)[0]) == std::string(1000, 'a'));
  CHECK(server_.getOpenFileHandleCount("test.txt") == 0);
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Server returning less data than requested before the end of the file", "[fetchopcfile]") {
  std::string content;
  for (size_t i = 0; i < 200; ++i) {
    content += std::to_string(i) + ",";
  }
  server_.addFile("test.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("test.txt")), files_node_.get(), content);
  server_.setFileMaxReadLength("test.txt", 7);
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(controller_.plan->getContent(results.at(processors::FetchOPCFile::Success)[0]) == content);
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Malformed Open response yields without creating a flowfile", "[fetchopcfile]") {
  server_.addFile("test.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("test.txt")), files_node_.get(), "file content");
  SECTION("File handle of the wrong type") {
    server_.setFileMalformedResponse("test.txt", FileMethod::Open, MalformedResponse::WrongOutputType);
  }
  SECTION("Additional output") {
    server_.setFileMalformedResponse("test.txt", FileMethod::Open, MalformedResponse::ExtraOutput);
  }
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));

  CHECK(controller_.trigger().at(processors::FetchOPCFile::Success).empty());
  CHECK(processor_->isYield());
  CHECK(LogTestController::getInstance().contains("Failed to open file node 'test.txt' for reading: BadTypeMismatch"));
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Malformed Read response yields without creating a flowfile and closes the file", "[fetchopcfile]") {
  server_.addFile("test.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("test.txt")), files_node_.get(), "file content");
  SECTION("Data of the wrong type") {
    server_.setFileMalformedResponse("test.txt", FileMethod::Read, MalformedResponse::WrongOutputType);
  }
  SECTION("Additional output") {
    server_.setFileMalformedResponse("test.txt", FileMethod::Read, MalformedResponse::ExtraOutput);
  }
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));

  CHECK(controller_.trigger().at(processors::FetchOPCFile::Success).empty());
  CHECK(processor_->isYield());
  CHECK(LogTestController::getInstance().contains("Failed to read file node 'test.txt': BadTypeMismatch"));
  CHECK(server_.getOpenFileHandleCount("test.txt") == 0);
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "MaxByteStringLength property of the wrong type is ignored", "[fetchopcfile]") {
  const std::string content(1000, 'a');
  server_.setServerMaxByteStringLength(128);
  server_.addFile("test.txt", UA_NODEID_STRING(server_.getNamespaceIndex(), const_cast<char*>("test.txt")), files_node_.get(), content);
  UA_Int32 max_byte_string_length = 64;
  server_.addFileProperty("test.txt", "MaxByteStringLength", &max_byte_string_length, UA_TYPES[UA_TYPES_INT32]);
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "String"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "test.txt"));

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  CHECK(controller_.plan->getContent(results.at(processors::FetchOPCFile::Success)[0]) == content);
  CHECK(server_.getMaxChunkSize("test.txt") == 128);
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Path is resolved once and again after a reconnect", "[fetchopcfile]") {
  server_.addFile("path_file.txt", UA_NODEID_NUMERIC(server_.getNamespaceIndex(), 0), files_node_.get(), "path content");
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "Path"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "Files/path_file.txt"));

  REQUIRE(controller_.trigger().at(processors::FetchOPCFile::Success).size() == 1);

  // A second node with the same browse name makes the path ambiguous, which only matters when the path is resolved again
  server_.addFile("path_file.txt", UA_NODEID_NUMERIC(server_.getNamespaceIndex(), 0), files_node_.get(), "path content");
  REQUIRE(controller_.trigger().at(processors::FetchOPCFile::Success).size() == 1);

  server_.stop();
  server_.start();
  REQUIRE(utils::verifyEventHappenedInPollTime(10s, [&]() {
    CHECK(controller_.trigger().at(processors::FetchOPCFile::Success).empty());
    return LogTestController::getInstance().contains("Path 'Files/path_file.txt' resolved to 2 node ids; exactly one file node is required for fetch");
  }, 100ms));
}

TEST_CASE_METHOD(FetchOPCFileTestFixture, "Path is resolved again if the file node it was resolved to is replaced", "[fetchopcfile]") {
  server_.addFile("path_file.txt", UA_NODEID_NUMERIC(server_.getNamespaceIndex(), 9001), files_node_.get(), "old content");
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeIDType.name, "Path"));
  REQUIRE(processor_->setProperty(processors::FetchOPCFile::NodeID.name, "Files/path_file.txt"));

  REQUIRE(controller_.trigger().at(processors::FetchOPCFile::Success).size() == 1);

  server_.removeFile("path_file.txt");
  server_.addFile("path_file.txt", UA_NODEID_NUMERIC(server_.getNamespaceIndex(), 9002), files_node_.get(), "new content");

  // The node the path was resolved to no longer exists
  CHECK(controller_.trigger().at(processors::FetchOPCFile::Success).empty());
  CHECK(processor_->isYield());

  const auto results = controller_.trigger();
  REQUIRE(results.at(processors::FetchOPCFile::Success).size() == 1);
  const auto& flow_file = results.at(processors::FetchOPCFile::Success)[0];
  CHECK(controller_.plan->getContent(flow_file) == "new content");
  CHECK(flow_file->getAttribute("NodeID") == "9002");
}

}  // namespace org::apache::nifi::minifi::test
