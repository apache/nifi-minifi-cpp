// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <filesystem>
#include <map>
#include <vector>
#include <string>

#include "archive_entry.h"

#include "ArchiveCommon.h"

struct TestArchiveEntry {
    const char* content;
    std::string name;
    mode_t type;
    mode_t perms;
    uid_t uid;
    gid_t gid;
    time_t mtime;
    uint32_t mtime_nsec;
    size_t size;
};


using TAE_MAP_T = std::map<std::string, TestArchiveEntry>;
using FN_VEC_T = std::vector<std::string>;

struct OrderedTestArchive {
    TAE_MAP_T map;
    FN_VEC_T order;
};

TAE_MAP_T build_test_archive_map(int, const char* const*, const char* const*);

FN_VEC_T build_test_archive_order(int, const char* const*);

OrderedTestArchive build_ordered_test_archive(int, const char* const*, const char* const*);

void build_test_archive(const std::filesystem::path&, const TAE_MAP_T& entries, FN_VEC_T order = FN_VEC_T());
void build_test_archive(const std::filesystem::path&, const OrderedTestArchive&);

bool check_archive_contents(const std::filesystem::path&, const TAE_MAP_T& entries, bool check_attributes = true, const FN_VEC_T& order = FN_VEC_T());
bool check_archive_contents(const std::filesystem::path&, const OrderedTestArchive&, bool check_attributes = true);
