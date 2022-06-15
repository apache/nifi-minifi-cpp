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

#include <fstream>

#include "../TestBase.h"
#include "../Catch.h"
#include "../../extensions/rocksdb-repos/database/RocksDatabase.h"
#include "../../extensions/rocksdb-repos/database/RocksDbInstance.h"
#include "../../extensions/rocksdb-repos/database/ColumnHandle.h"
#include "IntegrationTestUtils.h"
#include "database/StringAppender.h"
#include "../../extensions/rocksdb-repos/encryption/RocksDbEncryptionProvider.h"
#include "../Path.h"

#undef NDEBUG

using core::repository::StringAppender;

struct OpenDatabase {
  std::unique_ptr<rocksdb::DB> impl;
  std::unordered_map<std::string, std::unique_ptr<rocksdb::ColumnFamilyHandle>> columns;
};

struct RocksDBTest : TestController {
  RocksDBTest() {
    LogTestController::getInstance().setTrace<minifi::internal::RocksDatabase>();
    LogTestController::getInstance().setTrace<minifi::internal::RocksDbInstance>();
    LogTestController::getInstance().setTrace<minifi::internal::ColumnHandle>();
    db_dir = createTempDirectory();
  }

  [[nodiscard]] OpenDatabase openDB(const std::vector<std::string>& cf_names) const {
    rocksdb::DB* db_ptr = nullptr;
    std::vector<rocksdb::ColumnFamilyHandle*> cf_handle_ptrs;
    std::vector<rocksdb::ColumnFamilyDescriptor> cf_descs;
    for (auto& cf_name : cf_names) {
      cf_descs.emplace_back(cf_name, rocksdb::ColumnFamilyOptions{});
    }
    auto status = rocksdb::DB::Open(rocksdb::DBOptions{}, db_dir, cf_descs, &cf_handle_ptrs, &db_ptr);
    REQUIRE(status.ok());
    std::unordered_map<std::string, std::unique_ptr<rocksdb::ColumnFamilyHandle>> cf_handles;
    for (auto cf_ptr : cf_handle_ptrs) {
      cf_handles[cf_ptr->GetName()].reset(cf_ptr);
    }
    return {std::unique_ptr<rocksdb::DB>(db_ptr), std::move(cf_handles)};
  }

  std::string db_dir;
};

void new_db_opts(minifi::internal::Writable<rocksdb::DBOptions>& db_opts) {
  db_opts.set(&rocksdb::DBOptions::create_if_missing, true);
}

TEST_CASE_METHOD(RocksDBTest, "Malformed database uri - Missing column name", "[rocksDBTest1]") {
  auto db = minifi::internal::RocksDatabase::create({}, {}, "minifidb://malformed");
  REQUIRE(!db);
  REQUIRE(utils::verifyLogLinePresenceInPollTime(
      std::chrono::seconds{1}, "Couldn't detect the column name in 'minifidb://malformed'"));
}

TEST_CASE_METHOD(RocksDBTest, "Can write to default column", "[rocksDBTest2]") {
  {
    auto db = minifi::internal::RocksDatabase::create(new_db_opts, {}, db_dir);
    auto opendb = db->open();
    opendb->Put(rocksdb::WriteOptions{}, "fruit", "apple");
  }

  auto db = openDB({"default"});

  std::string value;
  db.impl->Get(rocksdb::ReadOptions{}, db.columns["default"].get(), "fruit", &value);
  REQUIRE(value == "apple");
}

TEST_CASE_METHOD(RocksDBTest, "Can write to specific column using the rocksdb uri scheme", "[rocksDBTest3]") {
  {
    auto db = minifi::internal::RocksDatabase::create(new_db_opts, {}, "minifidb://" + db_dir + "/column_one");
    auto opendb = db->open();
    opendb->Put(rocksdb::WriteOptions{}, "fruit", "apple");
  }

  auto db = openDB({"default", "column_one"});

  std::string value;
  db.impl->Get(rocksdb::ReadOptions{}, db.columns["column_one"].get(), "fruit", &value);
  REQUIRE(value == "apple");
}

TEST_CASE_METHOD(RocksDBTest, "Can write to two specific columns at once", "[rocksDBTest4]") {
  {
    auto db1 = minifi::internal::RocksDatabase::create(new_db_opts, {}, "minifidb://" + db_dir + "/column_one");
    auto opendb1 = db1->open();
    opendb1->Put(rocksdb::WriteOptions{}, "fruit", "apple");
    auto db2 = minifi::internal::RocksDatabase::create(new_db_opts, {}, "minifidb://" + db_dir + "/column_two");
    auto opendb2 = db2->open();
    opendb2->Put(rocksdb::WriteOptions{}, "animal", "penguin");
  }

  auto db = openDB({"default", "column_one", "column_two"});

  std::string value;
  db.impl->Get(rocksdb::ReadOptions{}, db.columns["column_one"].get(), "fruit", &value);
  REQUIRE(value == "apple");

  db.impl->Get(rocksdb::ReadOptions{}, db.columns["column_two"].get(), "animal", &value);
  REQUIRE(value == "penguin");
}

TEST_CASE_METHOD(RocksDBTest, "Can write to the default and a specific column at once", "[rocksDBTest5]") {
  {
    auto db1 = minifi::internal::RocksDatabase::create(new_db_opts, {}, "minifidb://" + db_dir + "/column_one");
    auto opendb1 = db1->open();
    opendb1->Put(rocksdb::WriteOptions{}, "fruit", "apple");
    auto db2 = minifi::internal::RocksDatabase::create(new_db_opts, {}, db_dir);
    auto opendb2 = db2->open();
    opendb2->Put(rocksdb::WriteOptions{}, "animal", "penguin");
  }

  auto db = openDB({"default", "column_one"});

  std::string value;
  db.impl->Get(rocksdb::ReadOptions{}, db.columns["column_one"].get(), "fruit", &value);
  REQUIRE(value == "apple");

  db.impl->Get(rocksdb::ReadOptions{}, db.columns["default"].get(), "animal", &value);
  REQUIRE(value == "penguin");
}

TEST_CASE_METHOD(RocksDBTest, "Error is logged if the options are incompatible with an existing column family", "[rocksDBTest6]") {
  auto db = minifi::internal::RocksDatabase::create(new_db_opts, {}, "minifidb://" + db_dir + "/column_one");
  REQUIRE(db->open());
  // implicitly created the "default" column family, but with the default options
  auto cf_opts = [] (minifi::internal::Writable<rocksdb::ColumnFamilyOptions>& cf_opts) {
    cf_opts.set(&rocksdb::ColumnFamilyOptions::merge_operator, std::make_shared<minifi::core::repository::StringAppender>());
  };
  auto default_db = minifi::internal::RocksDatabase::create(new_db_opts, cf_opts, "minifidb://" + db_dir + "/default");
  REQUIRE_FALSE(default_db->open());
  REQUIRE(utils::verifyLogLinePresenceInPollTime(
      std::chrono::seconds{1}, "Requested column 'default' has already been opened using a different configuration"));
}

TEST_CASE_METHOD(RocksDBTest, "Error is logged if different DBOptions are used", "[rocksDBTest7]") {
  auto db_opt_1 = [] (minifi::internal::Writable<rocksdb::DBOptions>& db_opts) {
    db_opts.set(&rocksdb::DBOptions::create_if_missing, true);
    db_opts.set(&rocksdb::DBOptions::manual_wal_flush, false);
  };
  auto db_opt_2 = [] (minifi::internal::Writable<rocksdb::DBOptions>& db_opts) {
    db_opts.set(&rocksdb::DBOptions::create_if_missing, true);
    db_opts.set(&rocksdb::DBOptions::manual_wal_flush, true);
  };
  auto col_1 = minifi::internal::RocksDatabase::create(db_opt_1, {}, "minifidb://" + db_dir + "/column_one");
  REQUIRE(col_1->open());
  auto col_2 = minifi::internal::RocksDatabase::create(db_opt_2, {}, "minifidb://" + db_dir + "/column_two");
  REQUIRE_FALSE(col_2->open());
  REQUIRE(utils::verifyLogLinePresenceInPollTime(
      std::chrono::seconds{1}, "Database '" + db_dir + "' has already been opened using a different configuration"));
}

TEST_CASE_METHOD(RocksDBTest, "Sanity check: merge fails without merge_operator", "[rocksDBTest8]") {
  auto db = minifi::internal::RocksDatabase::create(new_db_opts, {}, "minifidb://" + db_dir + "/col_one");
  REQUIRE(db);

  auto opendb = db->open();

  REQUIRE(opendb->Put({}, "a", "first").ok());
  REQUIRE_FALSE(opendb->Merge({}, "a", "second").ok());
}

TEST_CASE_METHOD(RocksDBTest, "Column options are applied", "[rocksDBTest9]") {
  auto cf_opts = [] (minifi::internal::Writable<rocksdb::ColumnFamilyOptions>& cf_opts) {
    cf_opts.set(&rocksdb::ColumnFamilyOptions::merge_operator, std::make_shared<StringAppender>(), StringAppender::Eq{});
  };
  std::string db_uri;
  SECTION("Named column") {
    db_uri = "minifidb://" + db_dir + "/col_one";
  }
  SECTION("Explicit default column") {
    db_uri = "minifidb://" + db_dir + "/default";
  }
  SECTION("Implicit default column") {
    db_uri = db_dir;
  }
  auto db = minifi::internal::RocksDatabase::create(new_db_opts, cf_opts, db_uri);
  REQUIRE(db);

  auto opendb = db->open();

  REQUIRE(opendb->Put({}, "a", "first").ok());
  REQUIRE(opendb->Merge({}, "a", "second").ok());

  std::string value;
  REQUIRE(opendb->Get({}, "a", &value).ok());
  REQUIRE(value == "firstsecond");
}

minifi::internal::DBOptionsPatch createEncrSetter(const utils::Path& home_dir, const std::string& db_name, const std::string& key_name) {
  auto env = core::repository::createEncryptingEnv(utils::crypto::EncryptionManager{home_dir.str()}, core::repository::DbEncryptionOptions{db_name, key_name});
  REQUIRE(env);
  return [env] (minifi::internal::Writable<rocksdb::DBOptions>& db_opts) {
    db_opts.set(&rocksdb::DBOptions::create_if_missing, true);
    db_opts.set(&rocksdb::DBOptions::env, env.get(), core::repository::EncryptionEq{});
  };
}

void withDefaultEnv(minifi::internal::Writable<rocksdb::DBOptions>& db_opts) {
  db_opts.set(&rocksdb::DBOptions::env, rocksdb::Env::Default());
}

TEST_CASE_METHOD(RocksDBTest, "Error is logged if different encryption keys are used", "[rocksDBTest10]") {
  utils::Path home_dir{createTempDirectory()};
  utils::file::FileUtils::create_dir((home_dir / "conf").str());
  std::ofstream{(home_dir / "conf" / "bootstrap.conf").str()}
    << "encryption.key.one=" << "805D7B95EF44DC27C87FFBC4DFDE376DAE604D55DB2C5496DEEF5236362DE62E" << "\n"
    << "encryption.key.two=" << "905D7B95EF44DC27C87FFBC4DFDE376DAE604D55DB2C5496DEEF5236362DE62E" << "\n";

  auto db_opt_1 = createEncrSetter(home_dir, "one", "encryption.key.one");
  auto col_1 = minifi::internal::RocksDatabase::create(db_opt_1, {}, "minifidb://" + db_dir + "/column_one");
  REQUIRE(col_1->open());

  SECTION("Using the same encryption key is OK") {
    auto db_opt_2 = createEncrSetter(home_dir, "two", "encryption.key.one");
    auto col_2 = minifi::internal::RocksDatabase::create(db_opt_2, {}, "minifidb://" + db_dir + "/column_two");
    REQUIRE(col_2->open());
  }

  SECTION("Using different encryption key") {
    auto db_opt_2 = createEncrSetter(home_dir, "two", "encryption.key.two");
    auto col_2 = minifi::internal::RocksDatabase::create(db_opt_2, {}, "minifidb://" + db_dir + "/column_two");
    REQUIRE_FALSE(col_2->open());
    REQUIRE(utils::verifyLogLinePresenceInPollTime(
        std::chrono::seconds{1}, "Database '" + db_dir + "' has already been opened using a different configuration"));
  }

  SECTION("Using no encryption key") {
    auto col_2 = minifi::internal::RocksDatabase::create(withDefaultEnv, {}, "minifidb://" + db_dir + "/column_two");
    REQUIRE_FALSE(col_2->open());
    REQUIRE(utils::verifyLogLinePresenceInPollTime(
        std::chrono::seconds{1}, "Database '" + db_dir + "' has already been opened using a different configuration"));
  }
}
