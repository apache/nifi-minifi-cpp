/**
 * @file ArchiveTests.h
 * Archive test declarations
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

#ifndef ARCHIVE_TESTS_H
#define ARCHIVE_TESTS_H

#include <map>
#include <vector>
#include <string>

#include <archive.h>
#include <archive_entry.h>

typedef struct {
    const char* content;
    std::string name;
    mode_t type;
    mode_t perms;
    uid_t uid;
    gid_t gid;
    time_t mtime;
    uint32_t mtime_nsec;
    size_t size;
} TestArchiveEntry;


typedef std::map<std::string, TestArchiveEntry> TAE_MAP_T;
typedef std::vector<std::string> FN_VEC_T;

typedef struct {
    TAE_MAP_T map;
    FN_VEC_T order;
} OrderedTestArchive;

TAE_MAP_T build_test_archive_map(int, const char**, const char**);

FN_VEC_T build_test_archive_order(int, const char**);

OrderedTestArchive build_ordered_test_archive(int, const char**, const char**);

void build_test_archive(std::string, TAE_MAP_T entries, FN_VEC_T order = FN_VEC_T());
void build_test_archive(std::string, OrderedTestArchive);

bool check_archive_contents(std::string, TAE_MAP_T entries, bool check_attributes=true, FN_VEC_T order = FN_VEC_T());
bool check_archive_contents(std::string, OrderedTestArchive, bool check_attributes=true);

#endif