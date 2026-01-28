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

namespace org::apache::nifi::minifi::test {

class LmdbStreamTest : TestController {
 public:
  LmdbStreamTest() : db_path_(createTempDirectory().string()) {
    if (const int rc = mdb_env_create(&lmdb_env_)) {
      throw std::runtime_error("Failed to create LMDB environment: " + std::string(mdb_strerror(rc)));
    }
    mdb_env_set_mapsize(lmdb_env_, 100ULL * 1024 * 1024);  // 100 MB

    if (const int rc = mdb_env_open(lmdb_env_, db_path_.c_str(), MDB_NOTLS, 0664)) {
      throw std::runtime_error("Failed to open LMDB environment " + db_path_ + ": " + std::string(mdb_strerror(rc)));
    }

    MDB_txn* init_txn = nullptr;
    mdb_txn_begin(lmdb_env_, nullptr, 0, &init_txn);
    if (const auto rc = mdb_dbi_open(init_txn, nullptr, 0, &lmdb_handle_); rc != MDB_SUCCESS) {
      mdb_txn_abort(init_txn);
      mdb_env_close(lmdb_env_);
      throw std::runtime_error("Failed to open LMDB database: " + std::string(mdb_strerror(rc)));
    }
    mdb_txn_commit(init_txn);
  }

  LmdbStreamTest(const LmdbStreamTest&) = delete;
  LmdbStreamTest& operator=(const LmdbStreamTest&) = delete;
  LmdbStreamTest(LmdbStreamTest&&) = delete;
  LmdbStreamTest& operator=(LmdbStreamTest&&) = delete;

  ~LmdbStreamTest() override {
    mdb_dbi_close(lmdb_env_, lmdb_handle_);
    mdb_env_close(lmdb_env_);
  }

  std::optional<std::string> readValue(const std::string& key) {
    MDB_val db_key{key.size(), const_cast<char*>(key.data())};
    MDB_val db_value{};

    MDB_txn* txn = nullptr;
    mdb_txn_begin(lmdb_env_, nullptr, MDB_RDONLY, &txn);
    auto guard = gsl::finally([txn] { mdb_txn_abort(txn); });
    const auto result = mdb_get(txn, lmdb_handle_, &db_key, &db_value);

    if (result == MDB_SUCCESS) { return std::string(static_cast<char*>(db_value.mv_data), db_value.mv_size); }
    return std::nullopt;
  }

 protected:
  std::string db_path_;
  MDB_env* lmdb_env_{nullptr};
  MDB_dbi lmdb_handle_{};
};

namespace {

size_t writeString(io::LmdbStream& stream, const std::string& content) {
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
    io::LmdbStream stream(db_path_, lmdb_env_, &lmdb_handle_, true);
    REQUIRE_FALSE(minifi::io::isError(writeString(stream, content)));
  }
  auto val = readValue(db_path_);
  REQUIRE(val.has_value());
  CHECK(val->size() == content.size());
  CHECK(*val == content);
}

TEST_CASE_METHOD(LmdbStreamTest, "Multiple write test") {
  std::string content = "banana";
  std::string expected_content;

  {
    io::LmdbStream stream(db_path_, lmdb_env_, &lmdb_handle_, true);
    for (int i = 0; i < 5; ++i) {
      REQUIRE_FALSE(minifi::io::isError(writeString(stream, content)));
      expected_content += content;
    }
  }
  auto val = readValue(db_path_);
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

  io::LmdbStream write_stream(db_path_, lmdb_env_, &lmdb_handle_, true);
  REQUIRE_FALSE(minifi::io::isError(writeString(write_stream, content)));
  write_stream.close();

  io::LmdbStream read_stream(db_path_, lmdb_env_, &lmdb_handle_, false);
  std::vector<std::byte> buffer(content.size());
  REQUIRE_FALSE(minifi::io::isError(read_stream.read(buffer)));
  REQUIRE(bytesToString(buffer, buffer.size()) == content);
}

TEST_CASE_METHOD(LmdbStreamTest, "Read in chunks") {
  std::string content = "banana";
  io::LmdbStream write_stream(db_path_, lmdb_env_, &lmdb_handle_, true);
  REQUIRE_FALSE(minifi::io::isError(writeString(write_stream, content)));
  write_stream.close();

  io::LmdbStream read_stream(db_path_, lmdb_env_, &lmdb_handle_, false);
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
  io::LmdbStream read_stream(db_path_, lmdb_env_, &lmdb_handle_, false);
  std::vector<std::byte> buffer(8);
  REQUIRE(minifi::io::isError(read_stream.read(buffer)));
}

TEST_CASE_METHOD(LmdbStreamTest, "Reading after EOF returns zero") {
  io::LmdbStream write_stream(db_path_, lmdb_env_, &lmdb_handle_, true);
  REQUIRE_FALSE(minifi::io::isError(writeString(write_stream, "abc")));
  write_stream.close();

  io::LmdbStream read_stream(db_path_, lmdb_env_, &lmdb_handle_, false);
  std::vector<std::byte> buffer(3);
  REQUIRE(read_stream.read(buffer) == 3);
  REQUIRE(read_stream.read(buffer) == 0);
}

TEST_CASE_METHOD(LmdbStreamTest, "Reading into an empty buffer returns zero") {
  io::LmdbStream write_stream(db_path_, lmdb_env_, &lmdb_handle_, true);
  REQUIRE_FALSE(minifi::io::isError(writeString(write_stream, "abc")));
  write_stream.close();

  io::LmdbStream read_stream(db_path_, lmdb_env_, &lmdb_handle_, false);
  std::vector<std::byte> empty_buffer;
  REQUIRE(read_stream.read(empty_buffer) == 0);
}

TEST_CASE_METHOD(LmdbStreamTest, "seek and tell control read offset") {
  io::LmdbStream write_stream(db_path_, lmdb_env_, &lmdb_handle_, true);
  REQUIRE_FALSE(minifi::io::isError(writeString(write_stream, "banana")));
  write_stream.close();

  io::LmdbStream read_stream(db_path_, lmdb_env_, &lmdb_handle_, false);
  read_stream.seek(2);
  REQUIRE(read_stream.tell() == 2);
  std::vector<std::byte> buffer(3);
  REQUIRE(read_stream.read(buffer) == 3);
  REQUIRE(bytesToString(buffer, buffer.size()) == "nan");
  REQUIRE(read_stream.tell() == 5);
}

TEST_CASE_METHOD(LmdbStreamTest, "size reflects buffered writes before commit") {
  io::LmdbStream write_stream(db_path_, lmdb_env_, &lmdb_handle_, true);
  REQUIRE(write_stream.size() == 0);
  REQUIRE_FALSE(minifi::io::isError(writeString(write_stream, "hello")));
  REQUIRE(write_stream.size() == 5);
  REQUIRE_FALSE(minifi::io::isError(writeString(write_stream, "!")));
  REQUIRE(write_stream.size() == 6);
}

TEST_CASE_METHOD(LmdbStreamTest, "Writing to a read-only stream returns STREAM_ERROR") {
  io::LmdbStream read_stream(db_path_, lmdb_env_, &lmdb_handle_, false);
  REQUIRE(minifi::io::isError(writeString(read_stream, "anything")));
}

TEST_CASE_METHOD(LmdbStreamTest, "Writing nullptr with a non-zero length returns STREAM_ERROR") {
  io::LmdbStream write_stream(db_path_, lmdb_env_, &lmdb_handle_, true);
  REQUIRE(minifi::io::isError(write_stream.write(static_cast<const uint8_t*>(nullptr), 4)));
}

TEST_CASE_METHOD(LmdbStreamTest, "Writing zero bytes is a no-op without error") {
  io::LmdbStream write_stream(db_path_, lmdb_env_, &lmdb_handle_, true);
  const std::array<uint8_t, 1> dummy{};
  REQUIRE(write_stream.write(dummy.data(), 0) == 0);
  REQUIRE(write_stream.size() == 0);
}

TEST_CASE_METHOD(LmdbStreamTest, "commit on a read-only stream returns false") {
  io::LmdbStream read_stream(db_path_, lmdb_env_, &lmdb_handle_, false);
  REQUIRE_FALSE(read_stream.commit());
}

TEST_CASE_METHOD(LmdbStreamTest, "Repeated commit is a no-op after the first") {
  io::LmdbStream write_stream(db_path_, lmdb_env_, &lmdb_handle_, true);
  REQUIRE_FALSE(minifi::io::isError(writeString(write_stream, "banana")));
  REQUIRE(write_stream.commit());
  REQUIRE_FALSE(write_stream.commit());
}

TEST_CASE_METHOD(LmdbStreamTest, "Destructor commits buffered writes") {
  {
    io::LmdbStream write_stream(db_path_, lmdb_env_, &lmdb_handle_, true);
    REQUIRE_FALSE(minifi::io::isError(writeString(write_stream, "destroyed-write")));
    // Stream goes out of scope here; the destructor must commit
  }
  auto val = readValue(db_path_);
  REQUIRE(val.has_value());
  REQUIRE(*val == "destroyed-write");
}

TEST_CASE_METHOD(LmdbStreamTest, "Reopening an existing key in write mode appends to existing value") {
  {
    io::LmdbStream write_stream(db_path_, lmdb_env_, &lmdb_handle_, true);
    REQUIRE_FALSE(minifi::io::isError(writeString(write_stream, "first-")));
  }

  io::LmdbStream reopened_stream(db_path_, lmdb_env_, &lmdb_handle_, true);
  REQUIRE(reopened_stream.size() == 6);
  REQUIRE_FALSE(minifi::io::isError(writeString(reopened_stream, "second")));
  REQUIRE(reopened_stream.commit());

  auto val = readValue(db_path_);
  REQUIRE(val.has_value());
  REQUIRE(*val == "first-second");
}

}  // namespace org::apache::nifi::minifi::test
