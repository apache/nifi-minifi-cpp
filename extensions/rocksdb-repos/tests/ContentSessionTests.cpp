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

#include <array>
#include <memory>
#include <string>

#include "core/Core.h"
#include "FileSystemRepository.h"
#include "VolatileContentRepository.h"
#include "DatabaseContentRepository.h"
#include "core/BufferedContentSession.h"
#include "FlowFileRecord.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "utils/gsl.h"

template<typename ContentRepositoryClass>
class ContentSessionController : public TestController {
 public:
  ContentSessionController()
      : contentRepository(std::make_shared<ContentRepositoryClass>()) {
    auto contentRepoPath = createTempDirectory();
    auto config = std::make_shared<minifi::ConfigureImpl>();
    config->setHome(contentRepoPath);
    config->set(minifi::Configure::nifi_dbcontent_repository_directory_default, contentRepoPath.string());
    contentRepository->initialize(config);
  }

  ~ContentSessionController() {
    log.reset();
  }

  ContentSessionController(ContentSessionController&&) = delete;
  ContentSessionController(const ContentSessionController&) = delete;
  ContentSessionController& operator=(ContentSessionController&&) = delete;
  ContentSessionController& operator=(const ContentSessionController&) = delete;

  std::shared_ptr<core::ContentRepository> contentRepository;
};

const std::shared_ptr<minifi::io::OutputStream>& operator<<(const std::shared_ptr<minifi::io::OutputStream>& stream, const std::string& str) {
  REQUIRE(stream->write(reinterpret_cast<const uint8_t*>(str.data()), str.length()) == str.length());
  return stream;
}

const std::shared_ptr<minifi::io::InputStream>& operator>>(const std::shared_ptr<minifi::io::InputStream>& stream, std::string& str) {
  str = "";
  std::array<std::byte, 4096> buffer{};
  while (true) {
    const auto ret = stream->read(buffer);
    REQUIRE_FALSE(minifi::io::isError(ret));
    if (ret == 0) { break; }
    str += std::string{reinterpret_cast<char*>(buffer.data()), ret};
  }
  return stream;
}

void NO_CREATE(const std::shared_ptr<minifi::ResourceClaim>&) {
  REQUIRE(false);
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
  const bool is_buffered_session = std::dynamic_pointer_cast<core::BufferedContentSession>(session) != nullptr;

  REQUIRE_THROWS(session->write(oldClaim));

  REQUIRE_NOTHROW(session->read(oldClaim));

  session->append(oldClaim, 4, NO_CREATE) << "-addendum";

  std::shared_ptr<minifi::ResourceClaim> copied_claim;
  {
    auto other_session = contentRepository->createSession();
    other_session->append(oldClaim, 4, [&] (auto new_claim) {
      copied_claim = new_claim;
    }) << "-some extra content";
    other_session->commit();
  }
  REQUIRE(copied_claim);

  {
    std::string read_content;
    read_content.resize(contentRepository->size(*copied_claim));
    contentRepository->read(*copied_claim)->read(as_writable_bytes(std::span(read_content)));
    REQUIRE(read_content == "data-some extra content");
  }

  // TODO(adebreceni): MINIFICPP-1954
  if (is_buffered_session) {
    REQUIRE_THROWS(session->read(oldClaim));
  }

  auto claim1 = session->create();
  session->write(claim1) << "hello content!";
  {
    std::string content;
    session->read(claim1) >> content;
    REQUIRE(content == "hello content!");
  }

  auto claim2 = session->create();
  session->append(claim2, 0, NO_CREATE) << "beginning";
  session->append(claim2, 9, NO_CREATE) << "-end";

  auto claim3 = session->create();
  session->write(claim3) << "first";
  session->append(claim3, 5, NO_CREATE) << "-last";

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

    if (is_buffered_session) {
      std::string content;
      contentRepository->read(*oldClaim) >> content;
      REQUIRE(content == "data");
      REQUIRE(!contentRepository->exists(*claim1));
      REQUIRE(!contentRepository->exists(*claim2));
      REQUIRE(!contentRepository->exists(*claim3));
      REQUIRE(!contentRepository->exists(*claim4));
    }
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
