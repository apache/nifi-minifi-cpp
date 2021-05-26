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
#include "FileSystemRepository.h"
#include "VolatileContentRepository.h"
#include "DatabaseContentRepository.h"
#include "FlowFileRecord.h"
#include "../TestBase.h"
#include "utils/gsl.h"

template<typename ContentRepositoryClass>
class ContentSessionController : public TestController {
 public:
  ContentSessionController() {
    char format[] = "/var/tmp/content_repo.XXXXXX";
    std::string contentRepoPath = createTempDirectory(format);
    auto config = std::make_shared<minifi::Configure>();
    config->set(minifi::Configure::nifi_dbcontent_repository_directory_default, contentRepoPath);
    contentRepository = std::make_shared<ContentRepositoryClass>();
    contentRepository->initialize(config);
  }

  ~ContentSessionController() {
    log.reset();
  }

  std::shared_ptr<core::ContentRepository> contentRepository;
};

const std::shared_ptr<minifi::io::BaseStream>& operator<<(const std::shared_ptr<minifi::io::BaseStream>& stream, const std::string& str) {
  REQUIRE(stream->write(reinterpret_cast<const uint8_t*>(str.data()), str.length()) == str.length());
  return stream;
}

const std::shared_ptr<minifi::io::BaseStream>& operator>>(const std::shared_ptr<minifi::io::BaseStream>& stream, std::string& str) {
  str = "";
  uint8_t buffer[4096]{};
  while (true) {
    const auto ret = stream->read(buffer, sizeof(buffer));
    REQUIRE_FALSE(minifi::io::isError(ret));
    if (ret == 0) { break; }
    str += std::string{reinterpret_cast<char*>(buffer), ret};
  }
  return stream;
}

// TODO(adebreceni):
//  seems like the current version of Catch2 does not support templated tests
//  we should update instead of creating make-shift macros
template<typename ContentRepositoryClass>
void test_template() {
  ContentSessionController<ContentRepositoryClass> controller;
  std::shared_ptr<core::ContentRepository> contentRepository = controller.contentRepository;


  std::shared_ptr<minifi::ResourceClaim> oldClaim;
  {
    auto session = contentRepository->createSession();
    oldClaim = session->create();
    session->write(oldClaim) << "data";
    session->commit();
  }

  auto session = contentRepository->createSession();
  REQUIRE_THROWS(session->write(oldClaim));

  REQUIRE_NOTHROW(session->read(oldClaim));
  session->write(oldClaim, core::ContentSession::WriteMode::APPEND) << "-addendum";
  REQUIRE_THROWS(session->read(oldClaim));  // now throws because we appended to the content

  auto claim1 = session->create();
  session->write(claim1) << "hello content!";
  REQUIRE_THROWS(session->read(claim1));  // TODO(adebreceni): we currently have no means to create joined streams

  auto claim2 = session->create();
  session->write(claim2, core::ContentSession::WriteMode::APPEND) << "beginning";
  session->write(claim2, core::ContentSession::WriteMode::APPEND) << "-end";

  auto claim3 = session->create();
  session->write(claim3) << "first";
  session->write(claim3, core::ContentSession::WriteMode::APPEND) << "-last";

  auto claim4 = session->create();
  session->write(claim4) << "beginning";
  session->write(claim4) << "overwritten";

  SECTION("Commit") {
    session->commit();

    std::string content;
    contentRepository->read(*oldClaim) >> content;
    REQUIRE(content == "data-addendum");

    contentRepository->read(*claim1) >> content;
    REQUIRE(content == "hello content!");

    contentRepository->read(*claim2) >> content;
    REQUIRE(content == "beginning-end");

    contentRepository->read(*claim3) >> content;
    REQUIRE(content == "first-last");

    contentRepository->read(*claim4) >> content;
    REQUIRE(content == "overwritten");
  }

  SECTION("Rollback") {
    session->rollback();

    std::string content;
    contentRepository->read(*oldClaim) >> content;
    REQUIRE(content == "data");

    REQUIRE(!contentRepository->exists(*claim1));
    REQUIRE(!contentRepository->exists(*claim2));
    REQUIRE(!contentRepository->exists(*claim3));
    REQUIRE(!contentRepository->exists(*claim4));
  }
}

TEST_CASE("ContentSession behavior") {
  SECTION("FileSystemRepository") {
    test_template<core::repository::FileSystemRepository>();
  }
  SECTION("VolatileContentRepository") {
    test_template<core::repository::VolatileContentRepository>();
  }
  SECTION("DatabaseContentRepository") {
    test_template<core::repository::DatabaseContentRepository>();
  }
}
