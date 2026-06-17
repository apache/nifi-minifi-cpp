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

#include "LmdbStream.h"
#include "lmdb.h"
#include "unit/Catch.h"
#include "unit/TestBase.h"
#include "LmdbWrapper.h"

namespace org::apache::nifi::minifi::test {

using LmdbStream = org::apache::nifi::minifi::extensions::lmdb::LmdbStream;
using LmdbWrapper = org::apache::nifi::minifi::extensions::lmdb::LmdbWrapper;

class LmdbStreamTest : TestController {
 public:
  LmdbStreamTest() : db_path_(createTempDirectory().string()) {
    lmdb_wrapper_.initialize(db_path_, 100ULL * 1024 * 1024);  // 100 MB
  }

 protected:
  std::string db_path_;
  LmdbWrapper lmdb_wrapper_;
};

namespace {

size_t writeString(LmdbStream& stream, const std::string& content) {
  return stream.write(reinterpret_cast<const uint8_t*>(content.data()), content.size());
}

std::string bytesToString(const std::vector<std::byte>& buf, size_t len) {
  return {reinterpret_cast<const char*>(buf.data()), len};
}

}  // namespace

TEST_CASE_METHOD(LmdbStreamTest, "Simple write tests") {
  std::string content;
  SECTION("Non-empty value") {
    content = "banana";
  }
  SECTION("Empty value") {
    content = "";
  }

  {
    LmdbStream stream(db_path_, lmdb_wrapper_, true);
    REQUIRE_FALSE(minifi::io::isError(writeString(stream, content)));
  }
  auto val = lmdb_wrapper_.getValue(db_path_);
  REQUIRE(val.has_value());
  CHECK(val->size() == content.size());
  CHECK(*val == content);
}

TEST_CASE_METHOD(LmdbStreamTest, "Multiple write test") {
  std::string content = "banana";
  std::string expected_content;

  {
    LmdbStream stream(db_path_, lmdb_wrapper_, true);
    for (int i = 0; i < 5; ++i) {
      REQUIRE_FALSE(minifi::io::isError(writeString(stream, content)));
      expected_content += content;
    }
  }
  auto val = lmdb_wrapper_.getValue(db_path_);
  REQUIRE(val.has_value());
  CHECK(val->size() == expected_content.size());
  CHECK(*val == expected_content);
}

TEST_CASE_METHOD(LmdbStreamTest, "Simple read tests") {
  std::string content;
  SECTION("Non-empty value") {
    content = "banana";
  }
  SECTION("Empty value") {
    content = "";
  }

  LmdbStream write_stream(db_path_, lmdb_wrapper_, true);
  REQUIRE_FALSE(minifi::io::isError(writeString(write_stream, content)));
  write_stream.close();

  LmdbStream read_stream(db_path_, lmdb_wrapper_, false);
  std::vector<std::byte> buffer(content.size());
  REQUIRE_FALSE(minifi::io::isError(read_stream.read(buffer)));
  REQUIRE(bytesToString(buffer, buffer.size()) == content);
}

TEST_CASE_METHOD(LmdbStreamTest, "Read in chunks") {
  std::string content = "banana";
  LmdbStream write_stream(db_path_, lmdb_wrapper_, true);
  REQUIRE_FALSE(minifi::io::isError(writeString(write_stream, content)));
  write_stream.close();

  LmdbStream read_stream(db_path_, lmdb_wrapper_, false);
  std::vector<std::byte> buffer(2);

  REQUIRE_FALSE(minifi::io::isError(read_stream.read(buffer)));
  REQUIRE(bytesToString(buffer, buffer.size()) == "ba");

  REQUIRE_FALSE(minifi::io::isError(read_stream.read(buffer)));
  REQUIRE(bytesToString(buffer, buffer.size()) == "na");

  std::vector<std::byte> buffer2(5);
  const auto read_result = read_stream.read(buffer2);
  REQUIRE_FALSE(minifi::io::isError(read_result));
  REQUIRE(bytesToString(buffer2, read_result) == "na");
}

TEST_CASE_METHOD(LmdbStreamTest, "Reading a nonexistent key returns STREAM_ERROR") {
  LmdbStream read_stream(db_path_, lmdb_wrapper_, false);
  std::vector<std::byte> buffer(8);
  REQUIRE(minifi::io::isError(read_stream.read(buffer)));
}

TEST_CASE_METHOD(LmdbStreamTest, "Reading after EOF returns zero") {
  LmdbStream write_stream(db_path_, lmdb_wrapper_, true);
  REQUIRE_FALSE(minifi::io::isError(writeString(write_stream, "abc")));
  write_stream.close();

  LmdbStream read_stream(db_path_, lmdb_wrapper_, false);
  std::vector<std::byte> buffer(3);
  REQUIRE(read_stream.read(buffer) == 3);
  REQUIRE(read_stream.read(buffer) == 0);
}

TEST_CASE_METHOD(LmdbStreamTest, "Reading into an empty buffer returns zero") {
  LmdbStream write_stream(db_path_, lmdb_wrapper_, true);
  REQUIRE_FALSE(minifi::io::isError(writeString(write_stream, "abc")));
  write_stream.close();

  LmdbStream read_stream(db_path_, lmdb_wrapper_, false);
  std::vector<std::byte> empty_buffer;
  REQUIRE(read_stream.read(empty_buffer) == 0);
}

TEST_CASE_METHOD(LmdbStreamTest, "seek and tell control read offset") {
  LmdbStream write_stream(db_path_, lmdb_wrapper_, true);
  REQUIRE_FALSE(minifi::io::isError(writeString(write_stream, "banana")));
  write_stream.close();

  LmdbStream read_stream(db_path_, lmdb_wrapper_, false);
  read_stream.seek(2);
  REQUIRE(read_stream.tell() == 2);
  std::vector<std::byte> buffer(3);
  REQUIRE(read_stream.read(buffer) == 3);
  REQUIRE(bytesToString(buffer, buffer.size()) == "nan");
  REQUIRE(read_stream.tell() == 5);
}

TEST_CASE_METHOD(LmdbStreamTest, "size reflects buffered writes before commit") {
  LmdbStream write_stream(db_path_, lmdb_wrapper_, true);
  REQUIRE(write_stream.size() == 0);
  REQUIRE_FALSE(minifi::io::isError(writeString(write_stream, "hello")));
  REQUIRE(write_stream.size() == 5);
  REQUIRE_FALSE(minifi::io::isError(writeString(write_stream, "!")));
  REQUIRE(write_stream.size() == 6);
}

TEST_CASE_METHOD(LmdbStreamTest, "Writing to a read-only stream returns STREAM_ERROR") {
  LmdbStream read_stream(db_path_, lmdb_wrapper_, false);
  REQUIRE(minifi::io::isError(writeString(read_stream, "anything")));
}

TEST_CASE_METHOD(LmdbStreamTest, "Writing nullptr with a non-zero length returns STREAM_ERROR") {
  LmdbStream write_stream(db_path_, lmdb_wrapper_, true);
  REQUIRE(minifi::io::isError(write_stream.write(static_cast<const uint8_t*>(nullptr), 4)));
}

TEST_CASE_METHOD(LmdbStreamTest, "Writing zero bytes is a no-op without error") {
  LmdbStream write_stream(db_path_, lmdb_wrapper_, true);
  const std::array<uint8_t, 1> dummy{};
  REQUIRE(write_stream.write(dummy.data(), 0) == 0);
  REQUIRE(write_stream.size() == 0);
}

TEST_CASE_METHOD(LmdbStreamTest, "commit on a read-only stream returns false") {
  LmdbStream read_stream(db_path_, lmdb_wrapper_, false);
  REQUIRE_FALSE(read_stream.commit());
}

TEST_CASE_METHOD(LmdbStreamTest, "Repeated commit is a no-op after the first") {
  LmdbStream write_stream(db_path_, lmdb_wrapper_, true);
  REQUIRE_FALSE(minifi::io::isError(writeString(write_stream, "banana")));
  REQUIRE(write_stream.commit());
  REQUIRE_FALSE(write_stream.commit());
}

TEST_CASE_METHOD(LmdbStreamTest, "Destructor commits buffered writes") {
  {
    LmdbStream write_stream(db_path_, lmdb_wrapper_, true);
    REQUIRE_FALSE(minifi::io::isError(writeString(write_stream, "destroyed-write")));
    // Stream goes out of scope here; the destructor must commit
  }
  auto val = lmdb_wrapper_.getValue(db_path_);
  REQUIRE(val.has_value());
  REQUIRE(*val == "destroyed-write");
}

TEST_CASE_METHOD(LmdbStreamTest, "Reopening an existing key in write mode appends to existing value") {
  {
    LmdbStream write_stream(db_path_, lmdb_wrapper_, true);
    REQUIRE_FALSE(minifi::io::isError(writeString(write_stream, "first-")));
  }

  LmdbStream reopened_stream(db_path_, lmdb_wrapper_, true);
  REQUIRE(reopened_stream.size() == 6);
  REQUIRE_FALSE(minifi::io::isError(writeString(reopened_stream, "second")));
  REQUIRE(reopened_stream.commit());

  auto val = lmdb_wrapper_.getValue(db_path_);
  REQUIRE(val.has_value());
  REQUIRE(*val == "first-second");
}

}  // namespace org::apache::nifi::minifi::test
