/**
 * @file ArchiveTests.cpp
 * Archive test definitions
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
#include "ArchiveTests.h"
#include <set>
#include <algorithm>
#include <string>
#include <utility>

#include <archive.h>
#include <archive_entry.h>
#include "../../TestBase.h"

TAE_MAP_T build_test_archive_map(int NUM_FILES, const char** FILE_NAMES, const char** FILE_CONTENT) {
  TAE_MAP_T test_entries;

  for (int i = 0; i < NUM_FILES; i++) {
    std::string name { FILE_NAMES[i] };
    TestArchiveEntry entry;

    entry.name = name;
    entry.content = FILE_CONTENT[i];
    entry.size = strlen(FILE_CONTENT[i]);
    entry.type = AE_IFREG;
    entry.perms = 0765;
    entry.uid = 12;
    entry.gid = 34;
    entry.mtime = time(nullptr);
    entry.mtime_nsec = 3;

    test_entries[name] = entry;
  }

  return test_entries;
}

FN_VEC_T build_test_archive_order(int NUM_FILES, const char** FILE_NAMES) {
  FN_VEC_T ret;
  for (int i = 0; i < NUM_FILES; i++)
    ret.push_back(FILE_NAMES[i]);
  return ret;
}

OrderedTestArchive build_ordered_test_archive(int NUM_FILES, const char** FILE_NAMES, const char** FILE_CONTENT) {
  OrderedTestArchive ret;
  ret.map = build_test_archive_map(NUM_FILES, FILE_NAMES, FILE_CONTENT);
  ret.order = build_test_archive_order(NUM_FILES, FILE_NAMES);
  return ret;
}

void build_test_archive(std::string path, TAE_MAP_T entries, FN_VEC_T order) {
  std::cout << "Creating " << path << std::endl;
  archive * test_archive = archive_write_new();

  archive_write_set_format_ustar(test_archive);
  archive_write_open_filename(test_archive, path.c_str());
  struct archive_entry* entry = archive_entry_new();

  if (order.empty()) {  // Use map sort order
    for (auto &kvp : entries)
      order.push_back(kvp.first);
  }

  for (std::string name : order) {
    TestArchiveEntry test_entry = entries.at(name);

    std::cout << "Adding entry: " << name << std::endl;

    archive_entry_set_filetype(entry, test_entry.type);
    archive_entry_set_pathname(entry, test_entry.name.c_str());
    archive_entry_set_size(entry, test_entry.size);
    archive_entry_set_perm(entry, test_entry.perms);
    archive_entry_set_uid(entry, test_entry.uid);
    archive_entry_set_gid(entry, test_entry.gid);
    archive_entry_set_mtime(entry, test_entry.mtime, test_entry.mtime_nsec);

    archive_write_header(test_archive, entry);
    archive_write_data(test_archive, test_entry.content, test_entry.size);

    archive_entry_clear(entry);
  }

  archive_entry_free(entry);
  archive_write_close(test_archive);
}

void build_test_archive(std::string path, OrderedTestArchive ordered_archive) {
  build_test_archive(path, ordered_archive.map, ordered_archive.order);
}

bool check_archive_contents(std::string path, TAE_MAP_T entries, bool check_attributes, FN_VEC_T order) {
  FN_VEC_T read_names;
  FN_VEC_T extra_names;
  bool ok = true;
  struct archive *a = archive_read_new();
  struct archive_entry *entry;

  archive_read_support_format_all(a);
  archive_read_support_filter_all(a);

  int r = archive_read_open_filename(a, path.c_str(), 16384);

  if (r != ARCHIVE_OK) {
    std::cout << "Unable to open archive " << path << " for checking!" << std::endl;
    return false;
  }

  while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
    std::string name { archive_entry_pathname(entry) };
    auto it = entries.find(name);
    if (it == entries.end()) {
      extra_names.push_back(name);
    } else {
      read_names.push_back(name);
      TestArchiveEntry test_entry = it->second;
      size_t size = archive_entry_size(entry);

      std::cout << "Checking archive entry: " << name << std::endl;

      REQUIRE(size == test_entry.size);

      if (size > 0) {
        int rlen, nlen = 0;
        const char* buf[size];
        bool read_ok = true;

        for (;;) {
          rlen = archive_read_data(a, buf, size);
          nlen += rlen;
          if (rlen == 0)
            break;
          if (rlen < 0) {
            std::cout << "FAIL: Negative size read?" << std::endl;
            read_ok = false;
            break;
          }
        }

        if (read_ok) {
          REQUIRE(nlen == size);
          REQUIRE(memcmp(buf, test_entry.content, size) == 0);
        }
      }

      REQUIRE(archive_entry_filetype(entry) == test_entry.type);

      if (check_attributes) {
        REQUIRE(archive_entry_uid(entry) == test_entry.uid);
        REQUIRE(archive_entry_gid(entry) == test_entry.gid);
        REQUIRE(archive_entry_perm(entry) == test_entry.perms);
        REQUIRE(archive_entry_mtime(entry) == test_entry.mtime);
      }
    }
  }

  archive_read_close(a);
  archive_read_free(a);

  if (!extra_names.empty()) {
    ok = false;
    std::cout << "Extra files found: ";
    for (std::string filename : extra_names)
      std::cout << filename << " ";
    std::cout << std::endl;
  }

  REQUIRE(extra_names.empty());

  if (!order.empty()) {
    REQUIRE(order.size() == entries.size());
  }

  if (!order.empty()) {
    REQUIRE(read_names == order);
  } else {
    std::set<std::string> read_names_set(read_names.begin(), read_names.end());
    std::set<std::string> test_file_entries_set;
    std::transform(entries.begin(), entries.end(), std::inserter(test_file_entries_set, test_file_entries_set.end()), [](std::pair<std::string, TestArchiveEntry> p) {return p.first;});

    REQUIRE(read_names_set == test_file_entries_set);
  }

  return ok;
}

bool check_archive_contents(std::string path, OrderedTestArchive archive, bool check_attributes) {
  return check_archive_contents(path, archive.map, check_attributes, archive.order);
}
