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

#include "LmdbContentRepository.h"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "core/repository/VolatileContentRepository.h"
#include "unit/Catch.h"
#include "unit/TestBase.h"
#include "utils/ConfigurationUtils.h"

namespace org::apache::nifi::minifi::test {

class LmdbContentSessionController : public TestController {
 public:
  LmdbContentSessionController() : content_repository_(std::make_shared<extensions::lmdb::LmdbContentRepository>()) {
    auto content_repo_path = createTempDirectory();
    auto config = std::make_shared<ConfigureImpl>();
    config->set(Configure::nifi_dbcontent_repository_directory_default, content_repo_path.string());
    content_repository_->initialize(config);
  }

  ~LmdbContentSessionController() override { log.reset(); }

  LmdbContentSessionController(LmdbContentSessionController&&) = delete;
  LmdbContentSessionController(const LmdbContentSessionController&) = delete;
  LmdbContentSessionController& operator=(LmdbContentSessionController&&) = delete;
  LmdbContentSessionController& operator=(const LmdbContentSessionController&) = delete;

  std::shared_ptr<core::ContentRepository> content_repository_;
};

namespace {

std::shared_ptr<io::OutputStream> operator<<(std::shared_ptr<io::OutputStream> stream, const std::string& str) {
  REQUIRE(stream->write(reinterpret_cast<const uint8_t*>(str.data()), str.length()) == str.length());
  return stream;
}

std::shared_ptr<io::InputStream> operator>>(std::shared_ptr<io::InputStream> stream, std::string& str) {
  str.clear();
  std::array<std::byte, utils::configuration::DEFAULT_BUFFER_SIZE> buffer{};
  while (true) {
    const auto ret = stream->read(buffer);
    REQUIRE_FALSE(io::isError(ret));
    if (ret == 0) { break; }
    str += std::string{reinterpret_cast<char*>(buffer.data()), ret};
  }
  return stream;
}

void requireNoCreate(const std::shared_ptr<ResourceClaim>&) {
  REQUIRE(false);
}

std::shared_ptr<ResourceClaim> makeCommittedClaim(const std::shared_ptr<core::ContentRepository>& content_repository, const std::string& content) {
  auto session = content_repository->createSession();
  auto claim = session->create();
  session->write(claim) << content;
  session->commit();
  return claim;
}

}  // namespace

TEST_CASE("Writing a previously committed claim from a new session throws", "[lmdb]") {
  LmdbContentSessionController controller;
  auto old_claim = makeCommittedClaim(controller.content_repository_, "data");

  auto session = controller.content_repository_->createSession();
  REQUIRE_THROWS(session->write(old_claim));
}

TEST_CASE("Reading a previously committed claim from a new session succeeds", "[lmdb]") {
  LmdbContentSessionController controller;
  auto old_claim = makeCommittedClaim(controller.content_repository_, "data");

  auto session = controller.content_repository_->createSession();
  REQUIRE_NOTHROW(session->read(old_claim));
}

TEST_CASE("Reading a claim after appending to it in the same session throws", "[lmdb]") {
  LmdbContentSessionController controller;
  auto old_claim = makeCommittedClaim(controller.content_repository_, "data");

  auto session = controller.content_repository_->createSession();
  session->append(old_claim, 4, requireNoCreate) << "-addendum";
  REQUIRE_THROWS(session->read(old_claim));
}

TEST_CASE("Append with on_copy callback produces a new claim with concatenated content when the append lock is held", "[lmdb]") {
  LmdbContentSessionController controller;
  auto old_claim = makeCommittedClaim(controller.content_repository_, "data");

  auto blocking_session = controller.content_repository_->createSession();
  blocking_session->append(old_claim, 4, requireNoCreate) << "-addendum";

  std::shared_ptr<ResourceClaim> copied_claim;
  {
    auto other_session = controller.content_repository_->createSession();
    other_session->append(old_claim, 4, [&](auto new_claim) { copied_claim = std::move(new_claim); }) << "-some extra content";
    other_session->commit();
  }
  REQUIRE(copied_claim);

  std::string read_content;
  read_content.resize(controller.content_repository_->size(*copied_claim));
  controller.content_repository_->read(*copied_claim)->read(as_writable_bytes(std::span(read_content)));
  REQUIRE(read_content == "data-some extra content");
}

TEST_CASE("Session commits a write to a new claim", "[lmdb]") {
  LmdbContentSessionController controller;

  std::shared_ptr<ResourceClaim> claim;
  {
    auto session = controller.content_repository_->createSession();
    claim = session->create();
    session->write(claim) << "hello content!";

    std::string buffered_content;
    session->read(claim) >> buffered_content;
    REQUIRE(buffered_content == "hello content!");

    session->commit();
  }

  std::string content;
  controller.content_repository_->read(*claim) >> content;
  REQUIRE(content == "hello content!");
}

TEST_CASE("Session commits multiple appends from offsets on a new claim", "[lmdb]") {
  LmdbContentSessionController controller;

  std::shared_ptr<ResourceClaim> claim;
  {
    auto session = controller.content_repository_->createSession();
    claim = session->create();
    session->append(claim, 0, requireNoCreate) << "beginning";
    session->append(claim, 9, requireNoCreate) << "-end";
    session->commit();
  }

  std::string content;
  controller.content_repository_->read(*claim) >> content;
  REQUIRE(content == "beginning-end");
}

TEST_CASE("Session commits a write followed by an append on a new claim", "[lmdb]") {
  LmdbContentSessionController controller;

  std::shared_ptr<ResourceClaim> claim;
  {
    auto session = controller.content_repository_->createSession();
    claim = session->create();
    session->write(claim) << "first";
    session->append(claim, 5, requireNoCreate) << "-last";
    session->commit();
  }

  std::string content;
  controller.content_repository_->read(*claim) >> content;
  REQUIRE(content == "first-last");
}

TEST_CASE("Session commits a write that overwrites an earlier write on the same claim", "[lmdb]") {
  LmdbContentSessionController controller;

  std::shared_ptr<ResourceClaim> claim;
  {
    auto session = controller.content_repository_->createSession();
    claim = session->create();
    session->write(claim) << "beginning";
    session->write(claim) << "overwritten";
    session->commit();
  }

  std::string content;
  controller.content_repository_->read(*claim) >> content;
  REQUIRE(content == "overwritten");
}

TEST_CASE("Session commits an append to a previously committed claim", "[lmdb]") {
  LmdbContentSessionController controller;
  auto old_claim = makeCommittedClaim(controller.content_repository_, "data");

  {
    auto session = controller.content_repository_->createSession();
    session->append(old_claim, 4, requireNoCreate) << "-addendum";
    session->commit();
  }

  std::string content;
  controller.content_repository_->read(*old_claim) >> content;
  REQUIRE(content == "data-addendum");
}

TEST_CASE("Session rollback discards new claims", "[lmdb]") {
  LmdbContentSessionController controller;
  std::shared_ptr<core::ContentRepository> content_repository = controller.content_repository_;

  std::shared_ptr<ResourceClaim> claim;
  {
    auto session = content_repository->createSession();
    claim = session->create();
    session->write(claim) << "discarded";
    session->rollback();
  }
  REQUIRE_FALSE(content_repository->exists(*claim));
}

TEST_CASE("Session rollback leaves a previously committed claim unchanged", "[lmdb]") {
  LmdbContentSessionController controller;
  auto old_claim = makeCommittedClaim(controller.content_repository_, "data");

  {
    auto session = controller.content_repository_->createSession();
    session->append(old_claim, 4, requireNoCreate) << "-addendum";
    session->rollback();
  }

  std::string content;
  controller.content_repository_->read(*old_claim) >> content;
  REQUIRE(content == "data");
}

TEST_CASE("Buffered session reads buffered content before commit", "[lmdb]") {
  LmdbContentSessionController controller;

  auto session = controller.content_repository_->createSession();
  auto claim = session->create();
  session->write(claim) << "uncommitted-data";

  std::string content;
  session->read(claim) >> content;
  REQUIRE(content == "uncommitted-data");

  REQUIRE_FALSE(controller.content_repository_->exists(*claim));
}

TEST_CASE("Empty new claim commit does not throw", "[lmdb]") {
  LmdbContentSessionController controller;

  auto session = controller.content_repository_->createSession();
  auto claim = session->create();
  REQUIRE_NOTHROW(session->commit());

  REQUIRE(controller.content_repository_->exists(*claim));
  REQUIRE(controller.content_repository_->size(*claim) == 0);
}

TEST_CASE("Rollback after a commit on a different session leaves committed content untouched", "[lmdb]") {
  LmdbContentSessionController controller;

  std::shared_ptr<ResourceClaim> claim;
  {
    auto first_session = controller.content_repository_->createSession();
    claim = first_session->create();
    first_session->write(claim) << "committed";
    first_session->commit();
  }

  {
    auto rolled_back_session = controller.content_repository_->createSession();
    rolled_back_session->append(claim, 9, requireNoCreate) << "-rolled-back";
    rolled_back_session->rollback();
  }

  std::string content;
  controller.content_repository_->read(*claim) >> content;
  REQUIRE(content == "committed");
}

TEST_CASE("LmdbContentRepository::Session commit throws when underlying repository is not LmdbContentRepository", "[lmdb]") {
  auto unrelated_repository = std::make_shared<core::repository::VolatileContentRepository>();
  unrelated_repository->initialize(std::make_shared<ConfigureImpl>());

  extensions::lmdb::LmdbContentRepository::Session session(unrelated_repository);
  REQUIRE_THROWS_WITH(session.commit(), Catch::Matchers::ContainsSubstring("Session's repository is not an LmdbContentRepository"));
}

}  // namespace org::apache::nifi::minifi::test
