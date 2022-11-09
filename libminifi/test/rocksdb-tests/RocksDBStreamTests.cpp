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

#include "../TestBase.h"
#include "../Catch.h"
#include "../../extensions/rocksdb-repos/RocksDbStream.h"
#include "../../extensions/rocksdb-repos/DatabaseContentRepository.h"
#include "../../extensions/rocksdb-repos/database/StringAppender.h"

class RocksDBStreamTest : TestController {
 public:
  RocksDBStreamTest() {
    dbPath = createTempDirectory();
    auto set_db_opts = [] (minifi::internal::Writable<rocksdb::DBOptions>& db_opts) {
      db_opts.set(&rocksdb::DBOptions::create_if_missing, true);
      db_opts.set(&rocksdb::DBOptions::use_direct_io_for_flush_and_compaction, true);
      db_opts.set(&rocksdb::DBOptions::use_direct_reads, true);
    };
    auto set_cf_opts = [] (rocksdb::ColumnFamilyOptions& cf_opts) {
      cf_opts.merge_operator = std::make_shared<core::repository::StringAppender>();
    };
    db = minifi::internal::RocksDatabase::create(set_db_opts, set_cf_opts, dbPath);
    REQUIRE(db->open());
  }

 protected:
  std::string dbPath;
  std::unique_ptr<minifi::internal::RocksDatabase> db;
};

TEST_CASE_METHOD(RocksDBStreamTest, "Verify simple operation") {
  std::string content = "banana";
  minifi::io::RocksDbStream outStream("one", gsl::make_not_null(db.get()), true);
  outStream.write(content);
  const auto second_write_result = outStream.write(content);
  REQUIRE(second_write_result > 0);
  REQUIRE_FALSE(minifi::io::isError(second_write_result));
  minifi::io::RocksDbStream inStream("one", gsl::make_not_null(db.get()));
  std::string str;
  inStream.read(str);
  REQUIRE(str == content);
}

TEST_CASE_METHOD(RocksDBStreamTest, "Write zero bytes") {
  minifi::io::RocksDbStream stream("one", gsl::make_not_null(db.get()), true);

  REQUIRE(stream.write(nullptr, 0) == 0);

  minifi::io::RocksDbStream readonlyStream("two", gsl::make_not_null(db.get()), false);

  REQUIRE(minifi::io::isError(readonlyStream.write(nullptr, 0)));
}

TEST_CASE_METHOD(RocksDBStreamTest, "Read zero bytes") {
  minifi::io::RocksDbStream one("one", gsl::make_not_null(db.get()), true);
  const auto banana_write_result = one.write("banana");
  REQUIRE_FALSE(minifi::io::isError(banana_write_result));
  REQUIRE(banana_write_result > 0);

  minifi::io::RocksDbStream stream("one", gsl::make_not_null(db.get()));

  std::byte fake_buffer[1];
  REQUIRE(stream.read(gsl::make_span(fake_buffer).subspan(0, 0)) == 0);

  minifi::io::RocksDbStream nonExistingStream("two", gsl::make_not_null(db.get()));

  REQUIRE(minifi::io::isError(nonExistingStream.read(gsl::make_span(fake_buffer).subspan(0, 0))));
}
