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

#include <memory>
#include <string>

#include "core/Core.h"
#include "DatabaseContentRepository.h"
#include "FlowFileRecord.h"
#include "properties/Configure.h"
#include "provenance/Provenance.h"
#include "../TestBase.h"
#include "../unit/ProvenanceTestHelper.h"

TEST_CASE("Write Claim", "[TestDBCR1]") {
  TestController testController;
  char format[] = "/var/tmp/testRepo.XXXXXX";
  auto dir = testController.createTempDirectory(format);
  auto content_repo = std::make_shared<core::repository::DatabaseContentRepository>();

  auto configuration = std::make_shared<org::apache::nifi::minifi::Configure>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, dir);
  REQUIRE(content_repo->initialize(configuration));


  auto claim = std::make_shared<minifi::ResourceClaim>(content_repo);
  auto stream = content_repo->write(*claim);

  stream->write("well hello there");

  stream->close();

  content_repo->stop();
  // reclaim the memory
  content_repo = nullptr;

  content_repo = std::make_shared<core::repository::DatabaseContentRepository>();

  configuration = std::make_shared<org::apache::nifi::minifi::Configure>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, dir);
  REQUIRE(content_repo->initialize(configuration));

  auto read_stream = content_repo->read(*claim);

  std::string readstr;
  read_stream->read(readstr);

  REQUIRE(readstr == "well hello there");

  // should not be able to write to the read stream
  // -1 will indicate that we were not able to write any data

  REQUIRE(minifi::io::isError(read_stream->write("other value")));
}

TEST_CASE("Delete Claim", "[TestDBCR2]") {
  TestController testController;
  char format[] = "/var/tmp/testRepo.XXXXXX";
  auto dir = testController.createTempDirectory(format);
  auto content_repo = std::make_shared<core::repository::DatabaseContentRepository>();

  auto configuration = std::make_shared<org::apache::nifi::minifi::Configure>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, dir);
  REQUIRE(content_repo->initialize(configuration));


  auto claim = std::make_shared<minifi::ResourceClaim>(content_repo);
  auto stream = content_repo->write(*claim);

  stream->write("well hello there");

  stream->close();

  content_repo->stop();

  // reclaim the memory
  content_repo = nullptr;

  content_repo = std::make_shared<core::repository::DatabaseContentRepository>();

  configuration = std::make_shared<org::apache::nifi::minifi::Configure>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, dir);
  REQUIRE(content_repo->initialize(configuration));

  content_repo->remove(*claim);

  auto read_stream = content_repo->read(*claim);

  std::string readstr;

  // error tells us we have an invalid stream
  REQUIRE(minifi::io::isError(read_stream->read(readstr)));
}

TEST_CASE("Test Empty Claim", "[TestDBCR3]") {
  TestController testController;
  char format[] = "/var/tmp/testRepo.XXXXXX";
  auto dir = testController.createTempDirectory(format);
  auto content_repo = std::make_shared<core::repository::DatabaseContentRepository>();

  auto configuration = std::make_shared<org::apache::nifi::minifi::Configure>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, dir);
  REQUIRE(content_repo->initialize(configuration));

  auto claim = std::make_shared<minifi::ResourceClaim>(content_repo);
  auto stream = content_repo->write(*claim);

  // we're writing nothing to the stream.

  stream->close();

  content_repo->stop();

  // reclaim the memory
  content_repo = nullptr;

  content_repo = std::make_shared<core::repository::DatabaseContentRepository>();

  configuration = std::make_shared<org::apache::nifi::minifi::Configure>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, dir);
  REQUIRE(content_repo->initialize(configuration));

  auto read_stream = content_repo->read(*claim);

  std::string readstr;

  // error tells us we have an invalid stream
  REQUIRE(minifi::io::isError(read_stream->read(readstr)));
}

TEST_CASE("Delete NonExistent Claim", "[TestDBCR4]") {
  TestController testController;
  char format[] = "/var/tmp/testRepo.XXXXXX";
  auto dir = testController.createTempDirectory(format);
  auto content_repo = std::make_shared<core::repository::DatabaseContentRepository>();

  auto configuration = std::make_shared<org::apache::nifi::minifi::Configure>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, dir);
  REQUIRE(content_repo->initialize(configuration));

  auto claim = std::make_shared<minifi::ResourceClaim>(content_repo);
  auto claim2 = std::make_shared<minifi::ResourceClaim>(content_repo);
  auto stream = content_repo->write(*claim);

  stream->write("well hello there");

  stream->close();

  content_repo->stop();

  // reclaim the memory
  content_repo = nullptr;

  content_repo = std::make_shared<core::repository::DatabaseContentRepository>();

  configuration = std::make_shared<org::apache::nifi::minifi::Configure>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, dir);
  REQUIRE(content_repo->initialize(configuration));

  // we won't complain if it does not exist
  REQUIRE(content_repo->remove(*claim2));

  auto read_stream = content_repo->read(*claim);

  std::string readstr;

  read_stream->read(readstr);

  REQUIRE(readstr == "well hello there");
}

TEST_CASE("Delete Remove Count Claim", "[TestDBCR5]") {
  TestController testController;
  char format[] = "/var/tmp/testRepo.XXXXXX";
  auto dir = testController.createTempDirectory(format);
  auto content_repo = std::make_shared<core::repository::DatabaseContentRepository>();

  auto configuration = std::make_shared<org::apache::nifi::minifi::Configure>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, dir);
  REQUIRE(content_repo->initialize(configuration));

  auto claim = std::make_shared<minifi::ResourceClaim>(content_repo);
  auto claim2 = std::make_shared<minifi::ResourceClaim>(content_repo);
  auto stream = content_repo->write(*claim);

  stream->write("well hello there");

  stream->close();

  content_repo->stop();

  // reclaim the memory
  content_repo = nullptr;

  content_repo = std::make_shared<core::repository::DatabaseContentRepository>();

  configuration = std::make_shared<org::apache::nifi::minifi::Configure>();
  configuration->set(minifi::Configure::nifi_dbcontent_repository_directory_default, dir);
  REQUIRE(content_repo->initialize(configuration));

  // increment twice. verify we have 2 for the stream count
  // and then test the removal and verify that the claim was removed by virtue of obtaining
  // its count.
  content_repo->incrementStreamCount(*claim2);
  content_repo->incrementStreamCount(*claim2);
  REQUIRE(content_repo->getStreamCount(*claim2) == 2);
  content_repo->decrementStreamCount(*claim2);
  REQUIRE(content_repo->decrementStreamCount(*claim2) == core::StreamManager<minifi::ResourceClaim>::StreamState::Deleted);
  REQUIRE(content_repo->getStreamCount(*claim2) == 0);
  auto read_stream = content_repo->read(*claim);

  std::string readstr;

  read_stream->read(readstr);

  REQUIRE(readstr == "well hello there");
}
