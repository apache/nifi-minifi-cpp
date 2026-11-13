// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "../RocksDbStream.h"
#include "../DatabaseContentRepository.h"
#include "../database/StringAppender.h"

class RocksDBStreamTest : TestController {
 public:
  RocksDBStreamTest() {
    dbPath = createTempDirectory().string();
    auto set_db_opts = [] (minifi::internal::Writable<rocksdb::DBOptions>& db_opts) {
      db_opts.set(&rocksdb::DBOptions::create_if_missing, true);
      db_opts.set(&rocksdb::DBOptions::use_direct_io_for_flush_and_compaction, true);
      db_opts.set(&rocksdb::DBOptions::use_direct_reads, true);
    };
    auto set_cf_opts = [] (rocksdb::ColumnFamilyOptions& cf_opts) {
      cf_opts.merge_operator = std::make_shared<core::repository::StringAppender>();
    };
    db = minifi::internal::RocksDatabase::create(set_db_opts, set_cf_opts, dbPath, {});
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
  inStream.read(str, minifi::io::LengthPrefixSize::_16BIT, 100);
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

  std::array<std::byte, 1> fake_buffer{};
  REQUIRE(stream.read(std::span(fake_buffer).subspan(0, 0)) == 0);

  minifi::io::RocksDbStream nonExistingStream("two", gsl::make_not_null(db.get()));

  REQUIRE(minifi::io::isError(nonExistingStream.read(std::span(fake_buffer).subspan(0, 0))));
}
