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

#include <string.h>

#include <string>
#include <set>

#include <memory>

#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "Exception.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

std::shared_ptr<utils::IdGenerator> FocusArchiveEntry::id_generator_ = utils::IdGenerator::getIdGenerator();

core::Property FocusArchiveEntry::Path("Path", "The path within the archive to focus (\"/\" to focus the total archive)", "");
core::Relationship FocusArchiveEntry::Success("success", "success operational on the flow record");

void FocusArchiveEntry::initialize() {
  //! Set the supported properties
  std::set<core::Property> properties;
  properties.insert(Path);
  setSupportedProperties(properties);
  //! Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void FocusArchiveEntry::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  auto flowFile = session->get();

  if (!flowFile) {
    return;
  }

  fileutils::FileManager file_man;

  // Extract archive contents
  ArchiveMetadata archiveMetadata;
  context->getProperty(Path.getName(), archiveMetadata.focusedEntry);
  flowFile->getAttribute("filename", archiveMetadata.archiveName);

  ReadCallback cb(this, &file_man, &archiveMetadata);
  session->read(flowFile, &cb);

  // For each extracted entry, import & stash to key
  std::string targetEntryStashKey;
  std::string targetEntry;

  for (auto &entryMetadata : archiveMetadata.entryMetadata) {
    if (entryMetadata.entryType == AE_IFREG) {
      logger_->log_info("FocusArchiveEntry importing %s from %s", entryMetadata.entryName, entryMetadata.tmpFileName);
      session->import(entryMetadata.tmpFileName, flowFile, false, 0);
      utils::Identifier stashKeyUuid = id_generator_->generate();
      logger_->log_debug("FocusArchiveEntry generated stash key %s for entry %s", stashKeyUuid.to_string(), entryMetadata.entryName);
      entryMetadata.stashKey = stashKeyUuid.to_string();

      if (entryMetadata.entryName == archiveMetadata.focusedEntry) {
        targetEntryStashKey = entryMetadata.stashKey;
      }

      // Stash the content
      session->stash(entryMetadata.stashKey, flowFile);
    }
  }

  // Restore target archive entry
  if (targetEntryStashKey != "") {
    session->restore(targetEntryStashKey, flowFile);
  } else {
    logger_->log_warn("FocusArchiveEntry failed to locate target entry: %s",
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
        logger_->log_debug(exception.what());
        context->yield();
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
  session->transfer(flowFile, Success);
}

typedef struct {
  std::shared_ptr<io::BaseStream> stream;
  core::Processor *processor;
  char buf[8196];
} FocusArchiveEntryReadData;

// Read callback which reads from the flowfile stream
la_ssize_t FocusArchiveEntry::ReadCallback::read_cb(struct archive * a, void *d, const void **buf) {
  auto data = static_cast<FocusArchiveEntryReadData *>(d);
  *buf = data->buf;
  size_t read = 0;
  size_t last_read = 0;

  do {
    last_read = data->stream->read(reinterpret_cast<uint8_t *>(data->buf), 8196 - read);
    read += last_read;
  } while (data->processor->isRunning() && last_read > 0 && !io::isError(last_read) && read < 8196);

  if (!data->processor->isRunning()) {
    archive_set_error(a, EINTR, "Processor shut down during read");
    return -1;
  }

  return gsl::narrow<la_ssize_t>(read);
}

int64_t FocusArchiveEntry::ReadCallback::process(const std::shared_ptr<io::BaseStream>& stream) {
  auto inputArchive = archive_read_new();
  struct archive_entry *entry;
  int64_t nlen = 0;

  FocusArchiveEntryReadData data;
  data.stream = stream;
  data.processor = proc_;

  archive_read_support_format_all(inputArchive);
  archive_read_support_filter_all(inputArchive);

  // Read each item in the archive
  int res;

  if ((res = archive_read_open(inputArchive, &data, ok_cb, read_cb, ok_cb))) {
    logger_->log_error("FocusArchiveEntry can't open due to archive error: %s", archive_error_string(inputArchive));
    return nlen;
  }

  while (isRunning()) {
    res = archive_read_next_header(inputArchive, &entry);

    if (res == ARCHIVE_EOF) {
      break;
    }

    if (res < ARCHIVE_OK) {
      logger_->log_error("FocusArchiveEntry can't read header due to archive error: %s", archive_error_string(inputArchive));
      return nlen;
    }

    if (res < ARCHIVE_WARN) {
      logger_->log_warn("FocusArchiveEntry got archive warning while reading header: %s", archive_error_string(inputArchive));
      return nlen;
    }

    auto entryName = archive_entry_pathname(entry);
    (*_archiveMetadata).archiveFormatName.assign(archive_format_name(inputArchive));
    (*_archiveMetadata).archiveFormat = archive_format(inputArchive);

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

    logger_->log_info("FocusArchiveEntry entry type of %s is: %d", entryName, metadata.entryType);
    logger_->log_info("FocusArchiveEntry entry perm of %s is: %d", entryName, metadata.entryPerm);

    // Write content to tmp file
    if (entryType == AE_IFREG) {
      auto tmpFileName = file_man_->unique_file(true);
      metadata.tmpFileName = tmpFileName;
      metadata.entryType = entryType;
      logger_->log_info("FocusArchiveEntry extracting %s to: %s", entryName, tmpFileName);

      auto fd = fopen(tmpFileName.c_str(), "w");

      if (archive_entry_size(entry) > 0) {
#ifdef WIN32
        nlen += archive_read_data_into_fd(inputArchive, _fileno(fd));
#else
        nlen += archive_read_data_into_fd(inputArchive, fileno(fd));
#endif
      }

      fclose(fd);
    }

    (*_archiveMetadata).entryMetadata.push_back(metadata);
  }

  archive_read_close(inputArchive);
  archive_read_free(inputArchive);
  return nlen;
}

FocusArchiveEntry::ReadCallback::ReadCallback(core::Processor *processor, fileutils::FileManager *file_man, ArchiveMetadata *archiveMetadata)
    : file_man_(file_man),
      proc_(processor) {
  logger_ = logging::LoggerFactory<FocusArchiveEntry>::getLogger();
  _archiveMetadata = archiveMetadata;
}

FocusArchiveEntry::ReadCallback::~ReadCallback() = default;

REGISTER_RESOURCE(FocusArchiveEntry, "Allows manipulation of entries within an archive (e.g. TAR) by focusing on one entry within the archive at a time. "
    "When an archive entry is focused, that entry is treated as the content of the FlowFile and may be manipulated independently of the rest of the archive."
    " To restore the FlowFile to its original state, use UnfocusArchiveEntry.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
