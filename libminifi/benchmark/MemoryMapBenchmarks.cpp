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

#include <benchmark/benchmark.h>
#include "../test/TestBase.h"
#include "ResourceClaim.h"
#include "core/Core.h"
#include "repository/FileSystemRepository.h"
#include "repository/VolatileContentRepository.h"
#include "properties/Configure.h"

#ifdef ENABLE_ROCKSDB_BENCHMARKS
#include "DatabaseContentRepository.h"
#endif  // ENABLE_ROCKSDB_BENCHMARKS

template<class T>
class MemoryMapBMFixture : public benchmark::Fixture {
 public:
  void SetUp(const ::benchmark::State &state) {
    test_controller_ = std::make_shared<TestController>();
    repo_ = std::make_shared<T>();
    conf_ = std::make_shared<minifi::Configure>();
    char format[] = "/tmp/testRepo.XXXXXX";
    dir_ = std::string(test_controller_->createTempDirectory(format));
    test_file_ = dir_ + "/testfile";
    claim_ = std::make_shared<minifi::ResourceClaim>(test_file_, repo_);
  }

  void TearDown(const ::benchmark::State &state) {
  }

  void init_db_repo() {
    conf_->set(minifi::Configure::nifi_dbcontent_repository_directory_default, dir_);
    init_repo();
  }

  void init_repo() {
    repo_->initialize(conf_);
  }

  void set_test_input(size_t size, char c) {
    test_string_ = "";
    test_string_.resize(size, c);
    auto mm = repo_->mmap(claim_, test_string_.length(), false);
    mm->resize(test_string_.length());
    memcpy(mm->getData(), &test_string_[0], test_string_.length());
  }

  void set_test_expected_output(size_t size, char c) {
    expected_string_ = "";
    expected_string_.resize(size, c);
  }

  void validate_string(const char *read_string) {
    if (strncmp(read_string, expected_string_.c_str(), expected_string_.length()) != 0) {
      throw std::runtime_error("string read failed");
    }
  }

  void validate_byte(size_t pos, const char b) {
    if (b != expected_string_[pos]) {
      throw std::runtime_error("byte read failed");
    }
  }

  /**
   * Get deterministic random points to access. Alternates between positions relative to start & end of file so as to not be sequential.
   * @return set of random points
   */
  std::vector<size_t> random_points() {
    std::vector<size_t> p;

    for (size_t i = 0; i < test_string_.length() / 2; i += test_string_.length() / 100) {
      p.push_back(i);
      p.push_back(test_string_.length() - 1);
    }

    return p;
  }

  std::shared_ptr<minifi::Configure> conf_;
  std::shared_ptr<TestController> test_controller_;
  std::shared_ptr<T> repo_;
  std::shared_ptr<minifi::ResourceClaim> claim_;
  std::string test_file_;
  std::string test_string_;
  std::string expected_string_;
  std::string dir_;
};

typedef MemoryMapBMFixture<core::repository::FileSystemRepository> FSMemoryMapBMFixture;
typedef MemoryMapBMFixture<core::repository::VolatileContentRepository> VolatileMemoryMapBMFixture;

#ifdef ENABLE_ROCKSDB_BENCHMARKS 
typedef MemoryMapBMFixture<core::repository::DatabaseContentRepository> DatabaseMemoryMapBMFixture;
#endif  // ENABLE_ROCKSDB_BENCHMARKS

template<class T>
void mmap_read(T *fixture, benchmark::State &st) {
  for (auto _ : st) {
    auto mm = fixture->repo_->mmap(fixture->claim_, fixture->test_string_.length(), true);
    fixture->validate_string(reinterpret_cast<const char *>(mm->getData()));
  }

  fixture->repo_->stop();
}

template<class T>
void mmap_read_random(T *fixture, benchmark::State &st) {
  auto r = fixture->random_points();
  auto mm = fixture->repo_->mmap(fixture->claim_, fixture->test_string_.length(), true);
  for (auto _ : st) {
    auto data = reinterpret_cast<char *>(mm->getData());
    for (size_t p : r) {
      fixture->validate_byte(p, data[p]);
    }
  }

  fixture->repo_->stop();
}

template<class T>
void mmap_write_read(T *fixture, benchmark::State &st) {
  for (auto _ : st) {
    fixture->repo_->remove(fixture->claim_);
    auto mm = fixture->repo_->mmap(fixture->claim_, fixture->test_string_.length(), false);
    memcpy(mm->getData(), &(fixture->expected_string_[0]), fixture->test_string_.length());
    fixture->validate_string(reinterpret_cast<const char *>(mm->getData()));
  }

  fixture->repo_->stop();
}

template<class T>
void cb_read(T *fixture, benchmark::State &st) {
  for (auto _ : st) {
    auto rs = fixture->repo_->read(fixture->claim_);
    std::vector<uint8_t> buf;
    rs->readData(buf, fixture->test_string_.length() + 1);
    fixture->validate_string(reinterpret_cast<const char *>(&buf[0]));
  }

  fixture->repo_->stop();
}

template<class T>
void cb_read_random(T *fixture, benchmark::State &st) {
  auto r = fixture->random_points();
  auto rs = fixture->repo_->read(fixture->claim_);
  for (auto _ : st) {
    for (size_t p : r) {
      rs->seek(p);
      char b;
      rs->read(b);
      fixture->validate_byte(p, b);
    }
  }

  fixture->repo_->stop();
}

template <class T>
void cb_write_read(T *fixture, benchmark::State &st) {
  for (auto _ : st) {
    {
      fixture->repo_->remove(fixture->claim_);
      auto ws = fixture->repo_->write(fixture->claim_, false);
      ws->write(reinterpret_cast<uint8_t *>(&(fixture->expected_string_[0])), fixture->test_string_.length());
    }

    auto rs = fixture->repo_->read(fixture->claim_);
    std::vector<uint8_t> buf;
    rs->readData(buf, fixture->test_string_.length() + 1);
    fixture->validate_string(reinterpret_cast<const char *>(&buf[0]));
  }


  fixture->repo_->stop();
}

BENCHMARK_F(FSMemoryMapBMFixture, MemoryMap_FileSystemRepository_Read_Tiny)(benchmark::State &st) {
  init_repo();
  set_test_input(10, 'x');
  set_test_expected_output(10, 'x');
  mmap_read<FSMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(FSMemoryMapBMFixture, Callback_FileSystemRepository_Read_Tiny)(benchmark::State &st) {
  init_repo();
  set_test_input(10, 'x');
  set_test_expected_output(10, 'x');
  cb_read<FSMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(FSMemoryMapBMFixture, MemoryMap_FileSystemRepository_WriteRead_Tiny)(benchmark::State &st) {
  init_repo();
  set_test_input(10, 'x');
  set_test_expected_output(10, 'y');
  mmap_write_read<FSMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(FSMemoryMapBMFixture, Callback_FileSystemRepository_WriteRead_Tiny)(benchmark::State &st) {
  init_repo();
  set_test_input(10, 'x');
  set_test_expected_output(10, 'y');
  cb_write_read<FSMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(FSMemoryMapBMFixture, MemoryMap_FileSystemRepository_Read_Small)(benchmark::State &st) {
  init_repo();
  set_test_input(131072, 'x');
  set_test_expected_output(131072, 'x');
  mmap_read<FSMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(FSMemoryMapBMFixture, Callback_FileSystemRepository_Read_Small)(benchmark::State &st) {
  init_repo();
  set_test_input(131072, 'x');
  set_test_expected_output(131072, 'x');
  cb_read<FSMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(FSMemoryMapBMFixture, MemoryMap_FileSystemRepository_WriteRead_Small)(benchmark::State &st) {
  init_repo();
  set_test_input(131072, 'x');
  set_test_expected_output(131072, 'y');
  mmap_write_read<FSMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(FSMemoryMapBMFixture, Callback_FileSystemRepository_WriteRead_Small)(benchmark::State &st) {
  init_repo();
  set_test_input(131072, 'x');
  set_test_expected_output(131072, 'y');
  cb_write_read<FSMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(FSMemoryMapBMFixture, MemoryMap_FileSystemRepository_Read_Large)(benchmark::State &st) {
  init_repo();
  set_test_input(33554432, 'x');
  set_test_expected_output(33554432, 'x');
  mmap_read<FSMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(FSMemoryMapBMFixture, Callback_FileSystemRepository_Read_Large)(benchmark::State &st) {
  init_repo();
  set_test_input(33554432, 'x');
  set_test_expected_output(33554432, 'x');
  cb_read<FSMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(FSMemoryMapBMFixture, MemoryMap_FileSystemRepository_WriteRead_Large)(benchmark::State &st) {
  init_repo();
  set_test_input(33554432, 'x');
  set_test_expected_output(33554432, 'y');
  mmap_write_read<FSMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(FSMemoryMapBMFixture, Callback_FileSystemRepository_WriteRead_Large)(benchmark::State &st) {
  init_repo();
  set_test_input(33554432, 'x');
  set_test_expected_output(33554432, 'y');
  cb_write_read<FSMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(FSMemoryMapBMFixture, MemoryMap_FileSystemRepository_RandomRead_Large)(benchmark::State &st) {
  init_repo();
  set_test_input(33554432, 'x');
  set_test_expected_output(33554432, 'x');
  mmap_read_random<FSMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(FSMemoryMapBMFixture, Callback_FileSystemRepository_RandomRead_Large)(benchmark::State &st) {
  init_repo();
  set_test_input(33554432, 'x');
  set_test_expected_output(33554432, 'x');
  cb_read_random<FSMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(VolatileMemoryMapBMFixture, MemoryMap_VolatileRepository_Read_Tiny)(benchmark::State &st) {
  init_repo();
  set_test_input(10, 'x');
  set_test_expected_output(10, 'x');
  mmap_read<VolatileMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(VolatileMemoryMapBMFixture, Callback_VolatileRepository_Read_Tiny)(benchmark::State &st) {
  init_repo();
  set_test_input(10, 'x');
  set_test_expected_output(10, 'x');
  cb_read<VolatileMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(VolatileMemoryMapBMFixture, MemoryMap_VolatileRepository_WriteRead_Tiny)(benchmark::State &st) {
  init_repo();
  set_test_input(10, 'x');
  set_test_expected_output(10, 'y');
  mmap_write_read<VolatileMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(VolatileMemoryMapBMFixture, Callback_VolatileRepository_WriteRead_Tiny)(benchmark::State &st) {
  init_repo();
  set_test_input(10, 'x');
  set_test_expected_output(10, 'y');
  cb_write_read<VolatileMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(VolatileMemoryMapBMFixture, MemoryMap_VolatileRepository_Read_Small)(benchmark::State &st) {
  init_repo();
  set_test_input(131072, 'x');
  set_test_expected_output(131072, 'x');
  mmap_read<VolatileMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(VolatileMemoryMapBMFixture, Callback_VolatileRepository_Read_Small)(benchmark::State &st) {
  init_repo();
  set_test_input(131072, 'x');
  set_test_expected_output(131072, 'x');
  cb_read<VolatileMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(VolatileMemoryMapBMFixture, MemoryMap_VolatileRepository_WriteRead_Small)(benchmark::State &st) {
  init_repo();
  set_test_input(131072, 'x');
  set_test_expected_output(131072, 'y');
  mmap_write_read<VolatileMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(VolatileMemoryMapBMFixture, Callback_VolatileRepository_WriteRead_Small)(benchmark::State &st) {
  init_repo();
  set_test_input(131072, 'x');
  set_test_expected_output(131072, 'y');
  cb_write_read<VolatileMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(VolatileMemoryMapBMFixture, MemoryMap_VolatileRepository_Read_Large)(benchmark::State &st) {
  init_repo();
  set_test_input(33554432, 'x');
  set_test_expected_output(33554432, 'x');
  mmap_read<VolatileMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(VolatileMemoryMapBMFixture, Callback_VolatileRepository_Read_Large)(benchmark::State &st) {
  init_repo();
  set_test_input(33554432, 'x');
  set_test_expected_output(33554432, 'x');
  cb_read<VolatileMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(VolatileMemoryMapBMFixture, MemoryMap_VolatileRepository_WriteRead_Large)(benchmark::State &st) {
  init_repo();
  set_test_input(33554432, 'x');
  set_test_expected_output(33554432, 'y');
  mmap_write_read<VolatileMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(VolatileMemoryMapBMFixture, Callback_VolatileRepository_WriteRead_Large)(benchmark::State &st) {
  init_repo();
  set_test_input(33554432, 'x');
  set_test_expected_output(33554432, 'y');
  cb_write_read<VolatileMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(VolatileMemoryMapBMFixture, MemoryMap_VolatileRepository_RandomRead_Large)(benchmark::State &st) {
  init_repo();
  set_test_input(33554432, 'x');
  set_test_expected_output(33554432, 'x');
  mmap_read_random<VolatileMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(VolatileMemoryMapBMFixture, Callback_VolatileRepository_RandomRead_Large)(benchmark::State &st) {
  init_repo();
  set_test_input(33554432, 'x');
  set_test_expected_output(33554432, 'x');
  cb_read_random<VolatileMemoryMapBMFixture>(this, st);
}

#ifdef ENABLE_ROCKSDB_BENCHMARKS 

BENCHMARK_F(DatabaseMemoryMapBMFixture, MemoryMap_DatabaseRepository_Read_Tiny)(benchmark::State &st) {
  init_db_repo();
  set_test_input(10, 'x');
  set_test_expected_output(10, 'x');
  mmap_read<DatabaseMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(DatabaseMemoryMapBMFixture, Callback_DatabaseRepository_Read_Tiny)(benchmark::State &st) {
  init_db_repo();
  set_test_input(10, 'x');
  set_test_expected_output(10, 'x');
  cb_read<DatabaseMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(DatabaseMemoryMapBMFixture, MemoryMap_DatabaseRepository_WriteRead_Tiny)(benchmark::State &st) {
  init_db_repo();
  set_test_input(10, 'x');
  set_test_expected_output(10, 'y');
  mmap_write_read<DatabaseMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(DatabaseMemoryMapBMFixture, Callback_DatabaseRepository_WriteRead_Tiny)(benchmark::State &st) {
  init_db_repo();
  set_test_input(10, 'x');
  set_test_expected_output(10, 'y');
  cb_write_read<DatabaseMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(DatabaseMemoryMapBMFixture, MemoryMap_DatabaseRepository_Read_Small)(benchmark::State &st) {
  init_db_repo();
  set_test_input(131072, 'x');
  set_test_expected_output(131072, 'x');
  mmap_read<DatabaseMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(DatabaseMemoryMapBMFixture, Callback_DatabaseRepository_Read_Small)(benchmark::State &st) {
  init_db_repo();
  set_test_input(131072, 'x');
  set_test_expected_output(131072, 'x');
  cb_read<DatabaseMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(DatabaseMemoryMapBMFixture, MemoryMap_DatabaseRepository_WriteRead_Small)(benchmark::State &st) {
  init_db_repo();
  set_test_input(131072, 'x');
  set_test_expected_output(131072, 'y');
  mmap_write_read<DatabaseMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(DatabaseMemoryMapBMFixture, Callback_DatabaseRepository_WriteRead_Small)(benchmark::State &st) {
  init_db_repo();
  set_test_input(131072, 'x');
  set_test_expected_output(131072, 'y');
  cb_write_read<DatabaseMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(DatabaseMemoryMapBMFixture, MemoryMap_DatabaseRepository_Read_Large)(benchmark::State &st) {
  init_db_repo();
  set_test_input(33554432, 'x');
  set_test_expected_output(33554432, 'x');
  mmap_read<DatabaseMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(DatabaseMemoryMapBMFixture, Callback_DatabaseRepository_Read_Large)(benchmark::State &st) {
  init_db_repo();
  set_test_input(33554432, 'x');
  set_test_expected_output(33554432, 'x');
  cb_read<DatabaseMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(DatabaseMemoryMapBMFixture, MemoryMap_DatabaseRepository_WriteRead_Large)(benchmark::State &st) {
  init_db_repo();
  set_test_input(33554432, 'x');
  set_test_expected_output(33554432, 'y');
  mmap_write_read<DatabaseMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(DatabaseMemoryMapBMFixture, Callback_DatabaseRepository_WriteRead_Large)(benchmark::State &st) {
  init_db_repo();
  set_test_input(33554432, 'x');
  set_test_expected_output(33554432, 'y');
  cb_write_read<DatabaseMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(DatabaseMemoryMapBMFixture, MemoryMap_DatabaseRepository_RandomRead_Large)(benchmark::State &st) {
  init_db_repo();
  set_test_input(33554432, 'x');
  set_test_expected_output(33554432, 'x');
  mmap_read_random<DatabaseMemoryMapBMFixture>(this, st);
}

BENCHMARK_F(DatabaseMemoryMapBMFixture, Callback_DatabaseRepository_RandomRead_Large)(benchmark::State &st) {
  init_db_repo();
  set_test_input(33554432, 'x');
  set_test_expected_output(33554432, 'x');
  cb_read_random<DatabaseMemoryMapBMFixture>(this, st);
}

#endif  // ENABLE_ROCKSDB_BENCHMARKS

BENCHMARK_MAIN();
