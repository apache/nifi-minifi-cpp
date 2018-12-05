/**
 * @file ArchiveMetadata.cpp
 * ArchiveMetadata class definition
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

#include "ArchiveMetadata.h"

#include <archive.h>
#include <archive_entry.h>

#include <list>
#include <string>
#include <algorithm>
#include <iostream>

#include "utils/file/FileManager.h"
#include "Exception.h"

using org::apache::nifi::minifi::Exception;
using org::apache::nifi::minifi::ExceptionType;

rapidjson::Value ArchiveEntryMetadata::toJson(rapidjson::Document::AllocatorType &alloc) const {
    rapidjson::Value entryVal(rapidjson::kObjectType);

    rapidjson::Value entryNameVal;
    entryNameVal.SetString(entryName.c_str(), entryName.length());
    entryVal.AddMember("entry_name", entryNameVal, alloc);

    entryVal.AddMember("entry_type", entryType, alloc);
    entryVal.AddMember("entry_perm", entryPerm, alloc);
    entryVal.AddMember("entry_size", entrySize, alloc);
    entryVal.AddMember("entry_uid", entryUID, alloc);
    entryVal.AddMember("entry_gid", entryGID, alloc);
    entryVal.AddMember("entry_mtime", entryMTime, alloc);
    entryVal.AddMember("entry_mtime_nsec", entryMTimeNsec, alloc);

    if (entryType == AE_IFREG) {
        rapidjson::Value stashKeyVal;
        stashKeyVal.SetString(stashKey.c_str(), stashKey.length());
        entryVal.AddMember("stash_key", stashKeyVal, alloc);
    }

    return entryVal;
}

void ArchiveEntryMetadata::loadJson(const rapidjson::Value& entryVal) {
    entryName.assign(entryVal["entry_name"].GetString());
    entryType = entryVal["entry_type"].GetUint64();
    entryPerm = entryVal["entry_perm"].GetUint64();
    entrySize = entryVal["entry_size"].GetUint64();
    entryUID = entryVal["entry_uid"].GetUint64();
    entryGID = entryVal["entry_gid"].GetUint64();
    entryMTime = entryVal["entry_mtime"].GetUint64();
    entryMTimeNsec = entryVal["entry_mtime_nsec"].GetInt64();

    if (entryType == AE_IFREG)
        stashKey.assign(entryVal["stash_key"].GetString());
}

ArchiveEntryMetadata ArchiveEntryMetadata::fromJson(const rapidjson::Value& entryVal) {
    ArchiveEntryMetadata aem;
    aem.loadJson(entryVal);
    return aem;
}

ArchiveEntryIterator ArchiveMetadata::find(const std::string& name) {
    auto targetTest = [&](const ArchiveEntryMetadata& entry) -> bool {
        return entry.entryName == name;
    };

    return std::find_if(entryMetadata.begin(),
                        entryMetadata.end(),
                        targetTest);
}

ArchiveEntryIterator ArchiveMetadata::eraseEntry(ArchiveEntryIterator position) {
    return entryMetadata.erase(position);
}

ArchiveEntryIterator ArchiveMetadata::insertEntry(
    ArchiveEntryIterator position, const ArchiveEntryMetadata& entry) {
    return entryMetadata.insert(position, entry);
}

rapidjson::Value ArchiveMetadata::toJson(rapidjson::Document::AllocatorType &alloc) const {
    rapidjson::Value structVal(rapidjson::kArrayType);

    for (const auto &entry : entryMetadata) {
        structVal.PushBack(entry.toJson(alloc), alloc);
    }

    rapidjson::Value lensVal(rapidjson::kObjectType);

    rapidjson::Value archiveFormatNameVal;
    archiveFormatNameVal.SetString(archiveFormatName.c_str(), archiveFormatName.length());
    lensVal.AddMember("archive_format_name", archiveFormatNameVal, alloc);

    lensVal.AddMember("archive_format", archiveFormat, alloc);
    lensVal.AddMember("archive_structure", structVal, alloc);

    if (!archiveName.empty()) {
        rapidjson::Value archiveNameVal;
        archiveNameVal.SetString(archiveName.c_str(), archiveName.length());
        lensVal.AddMember("archive_name", archiveNameVal, alloc);
    }

   rapidjson::Value focusedEntryVal;
    focusedEntryVal.SetString(focusedEntry.c_str(), focusedEntry.length());
    lensVal.AddMember("focused_entry", focusedEntryVal, alloc);

    return lensVal;
}

ArchiveMetadata ArchiveMetadata::fromJson(const rapidjson::Value& metadataDoc) {
    ArchiveMetadata am;
    am.loadJson(metadataDoc);
    return am;
}

void ArchiveMetadata::loadJson(const rapidjson::Value& metadataDoc) {
    rapidjson::Value::ConstMemberIterator itr = metadataDoc.FindMember("archive_name");
    if (itr != metadataDoc.MemberEnd())
        archiveName.assign(itr->value.GetString());

    archiveFormatName.assign(metadataDoc["archive_format_name"].GetString());
    archiveFormat = metadataDoc["archive_format"].GetUint64();

    focusedEntry = metadataDoc["focused_entry"].GetString();
  
    for (const auto &entryVal : metadataDoc["archive_structure"].GetArray()) {
        entryMetadata.push_back(ArchiveEntryMetadata::fromJson(entryVal));
    }
}

void ArchiveMetadata::seedTempPaths(fileutils::FileManager *file_man, bool keep = false) {
    for (auto& entry : entryMetadata)
        entry.tmpFileName.assign(file_man->unique_file("/tmp/", keep));
}

ArchiveStack ArchiveStack::fromJson(const rapidjson::Value& input) {
    ArchiveStack as;
    as.loadJson(input);
    return as;
}

ArchiveStack ArchiveStack::fromJsonString(const std::string& input) {
    ArchiveStack as;
    as.loadJsonString(input);
    return as;
}

void ArchiveStack::loadJson(const rapidjson::Value& lensStack) {
    for (const auto& metadata : lensStack.GetArray()) {
        stack_.push_back(ArchiveMetadata::fromJson(metadata));
    }
}

void ArchiveStack::loadJsonString(const std::string& input) {
    rapidjson::Document lensStack;
    rapidjson::ParseResult ok = lensStack.Parse(input.c_str());

    if (!ok) {
        std::stringstream ss;
        ss << "Failed to parse archive lens stack from JSON string with reason: "
           << rapidjson::GetParseError_En(ok.Code())
           << " at offset " << ok.Offset();

        throw Exception(ExceptionType::GENERAL_EXCEPTION, ss.str());
    }

    loadJson(lensStack);
}

rapidjson::Document ArchiveStack::toJson() const {
    rapidjson::Document lensStack(rapidjson::kArrayType);
    rapidjson::Document::AllocatorType &alloc = lensStack.GetAllocator();

    for (const auto& metadata : stack_) {
        lensStack.PushBack(metadata.toJson(alloc), alloc);
    }

    return lensStack;
}

std::string ArchiveStack::toJsonString() const {
    rapidjson::Document d = toJson();

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    d.Accept(writer);
    
    std::string jsonString = buffer.GetString();
    return jsonString;
}
