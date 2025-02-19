/**
 * @file FocusArchiveEntry.cpp
 * FocusArchiveEntry class implementation
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
#include "FocusArchiveEntry.h"

#include <archive.h>
#include <archive_entry.h>

#include <cstring>

#include <array>
#include <string>

#include <memory>

#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "Exception.h"
#include "SmartArchivePtrs.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::processors {

std::shared_ptr<utils::IdGenerator> FocusArchiveEntry::id_generator_ = utils::IdGenerator::getIdGenerator();

void FocusArchiveEntry::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void FocusArchiveEntry::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  auto flowFile = session.get();

  if (!flowFile) {
    return;
  }

  utils::file::FileManager file_man;

  // Extract archive contents
  ArchiveMetadata archiveMetadata;
  archiveMetadata.focusedEntry = context.getProperty(Path).value_or("");
  flowFile->getAttribute("filename", archiveMetadata.archiveName);

  session.read(flowFile, ReadCallback{this, &file_man, &archiveMetadata});

  // For each extracted entry, import & stash to key
  std::string targetEntryStashKey;
  std::string targetEntry;

  for (auto &entryMetadata : archiveMetadata.entryMetadata) {
    if (entryMetadata.entryType == AE_IFREG) {
      logger_->log_info("FocusArchiveEntry importing {} from {}", entryMetadata.entryName, entryMetadata.tmpFileName);
      session.import(entryMetadata.tmpFileName.string(), flowFile, false, 0);
      utils::Identifier stashKeyUuid = id_generator_->generate();
      logger_->log_debug("FocusArchiveEntry generated stash key {} for entry {}", stashKeyUuid.to_string(), entryMetadata.entryName);
      entryMetadata.stashKey = stashKeyUuid.to_string();

      if (entryMetadata.entryName == archiveMetadata.focusedEntry) {
        targetEntryStashKey = entryMetadata.stashKey;
      }

      // Stash the content
      session.stash(entryMetadata.stashKey, flowFile);
    }
  }

  // Restore target archive entry
  if (!targetEntryStashKey.empty()) {
    session.restore(targetEntryStashKey, flowFile);
  } else {
    logger_->log_warn("FocusArchiveEntry failed to locate target entry: {}",
                      archiveMetadata.focusedEntry.c_str());
  }

  // Set new/updated lens stack to attribute
  {
    ArchiveStack archiveStack;

    std::string existingLensStack;

    if (flowFile->getAttribute("lens.archive.stack", existingLensStack)) {
      logger_->log_info("FocusArchiveEntry loading existing lens context");
      try {
        archiveStack.loadJsonString(existingLensStack);
      } catch (Exception &exception) {
        logger_->log_debug("{}", exception.what());
        context.yield();
        return;
      }
    }

    archiveStack.push(archiveMetadata);

    flowFile->setAttribute("lens.archive.stack", archiveStack.toJsonString());
  }

  // Update filename attribute to that of focused entry
  std::size_t found = archiveMetadata.focusedEntry.find_last_of("/\\");
  std::string path = archiveMetadata.focusedEntry.substr(0, found);
  std::string name = archiveMetadata.focusedEntry.substr(found + 1);
  flowFile->setAttribute("filename", name);
  flowFile->setAttribute("path", path);
  flowFile->setAttribute("absolute.path", archiveMetadata.focusedEntry);

  // Transfer to the relationship
  session.transfer(flowFile, Success);
}

struct FocusArchiveEntryReadData {
  std::shared_ptr<io::InputStream> stream;
  core::Processor *processor = nullptr;
  std::array<std::byte, 8196> buf{};
};

// Read callback which reads from the flowfile stream
la_ssize_t FocusArchiveEntry::ReadCallback::read_cb(struct archive * a, void *d, const void **buf) {
  auto data = static_cast<FocusArchiveEntryReadData *>(d);
  *buf = data->buf.data();
  size_t read = 0;
  size_t last_read = 0;

  do {
    last_read = data->stream->read(data->buf);
    read += last_read;
  } while (data->processor->isRunning() && last_read > 0 && !io::isError(last_read) && read < 8196);

  if (!data->processor->isRunning()) {
    archive_set_error(a, EINTR, "Processor shut down during read");
    return -1;
  }

  return gsl::narrow<la_ssize_t>(read);
}

int64_t FocusArchiveEntry::ReadCallback::operator()(const std::shared_ptr<io::InputStream>& stream) const {
  auto input_archive = processors::archive_read_unique_ptr{archive_read_new()};
  struct archive_entry *entry = nullptr;
  int64_t nlen = 0;

  FocusArchiveEntryReadData data;
  data.stream = stream;
  data.processor = proc_;

  archive_read_support_format_all(input_archive.get());
  archive_read_support_filter_all(input_archive.get());

  // Read each item in the archive
  if (archive_read_open(input_archive.get(), &data, ok_cb, read_cb, ok_cb)) {
    logger_->log_error("FocusArchiveEntry can't open due to archive error: {}", archive_error_string(input_archive.get()));
    return nlen;
  }

  while (proc_->isRunning()) {
    auto res = archive_read_next_header(input_archive.get(), &entry);

    if (res == ARCHIVE_EOF) {
      break;
    }

    if (res < ARCHIVE_OK) {
      logger_->log_error("FocusArchiveEntry can't read header due to archive error: {}", archive_error_string(input_archive.get()));
      return nlen;
    }

    if (res < ARCHIVE_WARN) {
      logger_->log_warn("FocusArchiveEntry got archive warning while reading header: {}", archive_error_string(input_archive.get()));
      return nlen;
    }

    auto entryName = archive_entry_pathname(entry);
    _archiveMetadata->archiveFormatName.assign(archive_format_name(input_archive.get()));
    _archiveMetadata->archiveFormat = archive_format(input_archive.get());

    // Record entry metadata
    auto entryType = archive_entry_filetype(entry);

    ArchiveEntryMetadata metadata;
    metadata.entryName = entryName;
    metadata.entryType = entryType;
    metadata.entryPerm = archive_entry_perm(entry);
    metadata.entrySize = archive_entry_size(entry);
    metadata.entryUID = archive_entry_uid(entry);
    metadata.entryGID = archive_entry_gid(entry);
    metadata.entryMTime = archive_entry_mtime(entry);
    metadata.entryMTimeNsec = archive_entry_mtime_nsec(entry);

    logger_->log_info("FocusArchiveEntry entry type of {} is: {}", entryName, metadata.entryType);
    logger_->log_info("FocusArchiveEntry entry perm of {} is: {}", entryName, metadata.entryPerm);

    // Write content to tmp file
    if (entryType == AE_IFREG) {
      auto tmpFileName = file_man_->unique_file(true);
      metadata.tmpFileName = tmpFileName;
      metadata.entryType = entryType;
      logger_->log_info("FocusArchiveEntry extracting {} to: {}", entryName, tmpFileName);

      auto fd = fopen(tmpFileName.string().c_str(), "w");

      if (archive_entry_size(entry) > 0) {
#ifdef WIN32
        nlen += archive_read_data_into_fd(input_archive.get(), _fileno(fd));
#else
        nlen += archive_read_data_into_fd(input_archive.get(), fileno(fd));
#endif
      }

      (void)fclose(fd);
    }

    _archiveMetadata->entryMetadata.push_back(metadata);
  }

  return nlen;
}

FocusArchiveEntry::ReadCallback::ReadCallback(core::Processor *processor, utils::file::FileManager *file_man, ArchiveMetadata *archiveMetadata)
    : file_man_(file_man),
      proc_(processor),
      _archiveMetadata(archiveMetadata) {
}

REGISTER_RESOURCE(FocusArchiveEntry, Processor);

}  // namespace org::apache::nifi::minifi::processors
