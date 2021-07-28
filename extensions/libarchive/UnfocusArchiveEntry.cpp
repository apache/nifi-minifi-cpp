/**
 * @file UnfocusArchiveEntry.cpp
 * UnfocusArchiveEntry class implementation
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
#include "UnfocusArchiveEntry.h"

#include <string.h>
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <set>

#include "archive.h"
#include "archive_entry.h"

#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Relationship UnfocusArchiveEntry::Success("success", "success operational on the flow record");

void UnfocusArchiveEntry::initialize() {
  //! Set the supported properties
  std::set<core::Property> properties;
  setSupportedProperties(properties);
  //! Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void UnfocusArchiveEntry::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  auto flowFile = session->get();

  if (!flowFile) {
    return;
  }

  fileutils::FileManager file_man;
  ArchiveMetadata lensArchiveMetadata;

  // Get lens stack from attribute
  {
    ArchiveStack archiveStack;
    {
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
      } else {
        logger_->log_error("UnfocusArchiveEntry lens metadata not found");
        context->yield();
        return;
      }
    }

    lensArchiveMetadata = archiveStack.pop();
    lensArchiveMetadata.seedTempPaths(&file_man, false);

    {
      std::string stackStr = archiveStack.toJsonString();
      flowFile->setAttribute("lens.archive.stack", stackStr);
    }
  }

  // Export focused entry to tmp file
  for (const auto &entry : lensArchiveMetadata.entryMetadata) {
    if (entry.entryType != AE_IFREG || entry.entrySize == 0) {
      continue;
    }

    if (entry.entryName == lensArchiveMetadata.focusedEntry) {
      logger_->log_debug("UnfocusArchiveEntry exporting focused entry to %s", entry.tmpFileName);
      session->exportContent(entry.tmpFileName, flowFile, false);
    }
  }

  // Restore/export entries from stash, one-by-one, to tmp files
  for (const auto &entry : lensArchiveMetadata.entryMetadata) {
    if (entry.entryType != AE_IFREG || entry.entrySize == 0) {
      continue;
    }

    if (entry.entryName == lensArchiveMetadata.focusedEntry) {
      continue;
    }

    logger_->log_debug("UnfocusArchiveEntry exporting entry %s to %s", entry.stashKey, entry.tmpFileName);
    session->restore(entry.stashKey, flowFile);
    // TODO(calebj) implement copy export/don't worry about multiple claims/optimal efficiency for *now*
    session->exportContent(entry.tmpFileName, flowFile, false);
  }

  if (lensArchiveMetadata.archiveName.empty()) {
    flowFile->removeAttribute("filename");
    flowFile->removeAttribute("path");
    flowFile->removeAttribute("absolute.path");
  } else {
    std::string abs_path = lensArchiveMetadata.archiveName;
    std::size_t found = abs_path.find_last_of("/\\");
    std::string path = abs_path.substr(0, found);
    std::string name = abs_path.substr(found + 1);
    flowFile->setAttribute("filename", name);
    flowFile->setAttribute("path", path);
    flowFile->setAttribute("absolute.path", abs_path);
  }

  // Create archive by restoring each entry in the archive from tmp files
  WriteCallback cb(&lensArchiveMetadata);
  session->write(flowFile, &cb);

  // Transfer to the relationship
  session->transfer(flowFile, Success);
}

UnfocusArchiveEntry::WriteCallback::WriteCallback(ArchiveMetadata *archiveMetadata) {
  logger_ = logging::LoggerFactory<UnfocusArchiveEntry>::getLogger();
  _archiveMetadata = archiveMetadata;
}

typedef struct {
  std::shared_ptr<io::BaseStream> stream;
} UnfocusArchiveEntryWriteData;

la_ssize_t UnfocusArchiveEntry::WriteCallback::write_cb(struct archive *, void *d, const void *buffer, size_t length) {
  auto* const data = static_cast<UnfocusArchiveEntryWriteData *>(d);
  const auto write_ret = data->stream->write(static_cast<const uint8_t*>(buffer), length);
  return io::isError(write_ret) ? -1 : gsl::narrow<la_ssize_t>(write_ret);
}

int64_t UnfocusArchiveEntry::WriteCallback::process(const std::shared_ptr<io::BaseStream>& stream) {
  auto outputArchive = archive_write_new();
  int64_t nlen = 0;

  archive_write_set_format(outputArchive, _archiveMetadata->archiveFormat);

  UnfocusArchiveEntryWriteData data;
  data.stream = stream;

  archive_write_open(outputArchive, &data, ok_cb, write_cb, ok_cb);

  // Iterate entries & write from tmp file to archive
  char buf[8192];
  struct stat st;
  struct archive_entry* entry = nullptr;

  for (const auto &entryMetadata : _archiveMetadata->entryMetadata) {
    entry = archive_entry_new();
    logger_->log_info("UnfocusArchiveEntry writing entry %s", entryMetadata.entryName);

    if (entryMetadata.entryType == AE_IFREG && entryMetadata.entrySize > 0) {
      size_t stat_ok = stat(entryMetadata.tmpFileName.c_str(), &st);
      if (stat_ok != 0) {
        logger_->log_error("Error statting %s: %d", entryMetadata.tmpFileName, stat_ok);
      }
      archive_entry_copy_stat(entry, &st);
    }

    archive_entry_set_filetype(entry, entryMetadata.entryType);
    archive_entry_set_pathname(entry, entryMetadata.entryName.c_str());
    archive_entry_set_perm(entry, entryMetadata.entryPerm);
    archive_entry_set_size(entry, entryMetadata.entrySize);
    archive_entry_set_uid(entry, entryMetadata.entryUID);
    archive_entry_set_gid(entry, entryMetadata.entryGID);
    archive_entry_set_mtime(entry, entryMetadata.entryMTime, gsl::narrow<long>(entryMetadata.entryMTimeNsec));  // NOLINT long comes from libarchive API

    logger_->log_info("Writing %s with type %d, perms %d, size %d, uid %d, gid %d, mtime %d,%d", entryMetadata.entryName, entryMetadata.entryType, entryMetadata.entryPerm,
                      entryMetadata.entrySize, entryMetadata.entryUID, entryMetadata.entryGID, entryMetadata.entryMTime, entryMetadata.entryMTimeNsec);

    archive_write_header(outputArchive, entry);

    // If entry is regular file, copy entry contents
    if (entryMetadata.entryType == AE_IFREG && entryMetadata.entrySize > 0) {
      logger_->log_info("UnfocusArchiveEntry writing %d bytes of "
                        "data from tmp file %s to archive entry %s",
                        st.st_size, entryMetadata.tmpFileName, entryMetadata.entryName);
      std::ifstream ifs(entryMetadata.tmpFileName, std::ifstream::in | std::ios::binary);

      while (ifs.good()) {
        ifs.read(buf, sizeof(buf));
        size_t len = gsl::narrow<size_t>(ifs.gcount());
        int64_t written = archive_write_data(outputArchive, buf, len);
        if (written < 0) {
          logger_->log_error("UnfocusArchiveEntry failed to write data to "
                             "archive entry %s due to error: %s",
                             entryMetadata.entryName, archive_error_string(outputArchive));
        } else {
          nlen += written;
        }
      }

      ifs.close();

      // Remove the tmp file as we are through with it
      std::remove(entryMetadata.tmpFileName.c_str());
    }

    archive_entry_clear(entry);
  }

  archive_write_close(outputArchive);
  archive_entry_free(entry);
  archive_write_free(outputArchive);
  return nlen;
}

REGISTER_RESOURCE(UnfocusArchiveEntry, "Restores a FlowFile which has had an archive entry focused via FocusArchiveEntry to its original state.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
