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
#include "../../extensions/rocksdb-repos/RocksDbStream.h"
#include "../../extensions/rocksdb-repos/DatabaseContentRepository.h"

class RocksDBStreamTest : TestController {
 public:
  RocksDBStreamTest() {
    char format[] = "/var/tmp/testdb.XXXXXX";
    dbPath = createTempDirectory(format);
    rocksdb::Options options;
    options.create_if_missing = true;
    options.use_direct_io_for_flush_and_compaction = true;
    options.use_direct_reads = true;
    options.merge_operator = std::make_shared<core::repository::StringAppender>();
    options.error_if_exists = false;
    options.max_successive_merges = 0;
    db = utils::make_unique<minifi::internal::RocksDatabase>(options, dbPath);
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
  REQUIRE(outStream.write(content) > 0);
  minifi::io::RocksDbStream inStream("one", gsl::make_not_null(db.get()));
  std::string str;
  inStream.read(str);
  REQUIRE(str == content);
}

TEST_CASE_METHOD(RocksDBStreamTest, "Write zero bytes") {
  minifi::io::RocksDbStream stream("one", gsl::make_not_null(db.get()), true);

  REQUIRE(stream.write(nullptr, 0) == 0);

  minifi::io::RocksDbStream readonlyStream("two", gsl::make_not_null(db.get()), false);

  REQUIRE(readonlyStream.write(nullptr, 0) == -1);
}

TEST_CASE_METHOD(RocksDBStreamTest, "Read zero bytes") {
  minifi::io::RocksDbStream one("one", gsl::make_not_null(db.get()), true);
  REQUIRE(one.write("banana") > 0);

  minifi::io::RocksDbStream stream("one", gsl::make_not_null(db.get()));

  REQUIRE(stream.read(nullptr, 0) == 0);

  minifi::io::RocksDbStream nonExistingStream("two", gsl::make_not_null(db.get()));

  REQUIRE(minifi::io::isError(nonExistingStream.read(nullptr, 0)));
}
