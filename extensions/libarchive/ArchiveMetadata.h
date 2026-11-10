// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#ifndef EXTENSIONS_LIBARCHIVE_ARCHIVEMETADATA_H_
#define EXTENSIONS_LIBARCHIVE_ARCHIVEMETADATA_H_

#include <list>
#include <vector>
#include <string>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"

#include "ArchiveCommon.h"
#include "utils/file/FileManager.h"

class ArchiveEntryMetadata {
 public:
  std::string entryName;
  mode_t entryType;
  mode_t entryPerm;
  uid_t entryUID;
  gid_t entryGID;
  uint64_t entryMTime;
  uint64_t entryMTimeNsec;
  uint64_t entrySize;

  std::filesystem::path tmpFileName;
  std::string stashKey;

  inline rapidjson::Value toJson(rapidjson::Document::AllocatorType &alloc) const;
  static inline ArchiveEntryMetadata fromJson(const rapidjson::Value&);

 private:
  inline void loadJson(const rapidjson::Value&);
};

using ArchiveEntryIterator = typename std::list<ArchiveEntryMetadata>::iterator;

class ArchiveMetadata {
 public:
  std::string archiveName;
  std::string archiveFormatName;
  int archiveFormat;
  std::list<ArchiveEntryMetadata> entryMetadata;

  std::string focusedEntry;

  ArchiveEntryIterator find(const std::string& name);
  ArchiveEntryIterator eraseEntry(ArchiveEntryIterator position);
  ArchiveEntryIterator insertEntry(ArchiveEntryIterator position, const ArchiveEntryMetadata& entry);

  void seedTempPaths(org::apache::nifi::minifi::utils::file::FileManager* file_man, bool keep);

  rapidjson::Value toJson(rapidjson::Document::AllocatorType &alloc) const;
  static ArchiveMetadata fromJson(const rapidjson::Value&);

 private:
  void loadJson(const rapidjson::Value&);
};

class ArchiveStack {
 public:
  static ArchiveStack fromJsonString(const std::string& input);
  static ArchiveStack fromJson(const rapidjson::Value& input);
  void push(const ArchiveMetadata& metadata) { stack_.push_back(metadata); }
  ArchiveMetadata pop() { auto x = top(); stack_.pop_back(); return x; }
  ArchiveMetadata top() const { return stack_.back(); }
  void loadJson(const rapidjson::Value& lensStack);
  void loadJsonString(const std::string& input);
  std::string toJsonString() const;
  rapidjson::Document toJson() const;

 private:
  std::vector<ArchiveMetadata> stack_;
};

#endif  // EXTENSIONS_LIBARCHIVE_ARCHIVEMETADATA_H_
