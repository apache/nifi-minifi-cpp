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

#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>

#include "SmartArchivePtrs.h"

#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/ConfigurationUtils.h"
#include "minifi-cpp/utils/gsl.h"

namespace {
inline constexpr auto BUFFER_SIZE = org::apache::nifi::minifi::utils::configuration::DEFAULT_BUFFER_SIZE;
}  // namespace

namespace org::apache::nifi::minifi::processors {

void UnfocusArchiveEntry::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void UnfocusArchiveEntry::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  auto flowFile = session.get();

  if (!flowFile) {
    return;
  }

  utils::file::FileManager file_man;
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
          logger_->log_debug("{}", exception.what());
          context.yield();
          return;
        }
      } else {
        logger_->log_error("UnfocusArchiveEntry lens metadata not found");
        context.yield();
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
      logger_->log_debug("UnfocusArchiveEntry exporting focused entry to {}", entry.tmpFileName);
      session.exportContent(entry.tmpFileName.string(), flowFile, false);
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

    logger_->log_debug("UnfocusArchiveEntry exporting entry {} to {}", entry.stashKey, entry.tmpFileName);
    session.restore(entry.stashKey, flowFile);
    // TODO(calebj) implement copy export/don't worry about multiple claims/optimal efficiency for *now*
    session.exportContent(entry.tmpFileName.string(), flowFile, false);
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
  session.write(flowFile, std::cref(cb));

  // Transfer to the relationship
  session.transfer(flowFile, Success);
}

UnfocusArchiveEntry::WriteCallback::WriteCallback(ArchiveMetadata *archiveMetadata)
    : _archiveMetadata(archiveMetadata) {
}

struct UnfocusArchiveEntryWriteData {
  std::shared_ptr<io::OutputStream> stream;
};

la_ssize_t UnfocusArchiveEntry::WriteCallback::write_cb(struct archive *, void *d, const void *buffer, size_t length) {
  auto* const data = static_cast<UnfocusArchiveEntryWriteData *>(d);
  const auto write_ret = data->stream->write(static_cast<const uint8_t*>(buffer), length);
  return io::isError(write_ret) ? -1 : gsl::narrow<la_ssize_t>(write_ret);
}

int64_t UnfocusArchiveEntry::WriteCallback::operator()(const std::shared_ptr<io::OutputStream>& stream) const {
  UnfocusArchiveEntryWriteData data;
  data.stream = stream;
  auto output_archive = archive_write_unique_ptr{archive_write_new()};
  int64_t nlen = 0;

  archive_write_set_format(output_archive.get(), _archiveMetadata->archiveFormat);

  archive_write_open(output_archive.get(), &data, nullptr, write_cb, nullptr);  // data must outlive the archive because it writes during free

  // Iterate entries & write from tmp file to archive
  std::array<char, BUFFER_SIZE> buf{};
  struct stat st{};
  auto entry = archive_entry_unique_ptr{archive_entry_new()};

  for (const auto &entryMetadata : _archiveMetadata->entryMetadata) {
    logger_->log_info("UnfocusArchiveEntry writing entry {}", entryMetadata.entryName);

    if (entryMetadata.entryType == AE_IFREG && entryMetadata.entrySize > 0) {
      size_t stat_ok = stat(entryMetadata.tmpFileName.string().c_str(), &st);
      if (stat_ok != 0) {
        logger_->log_error("Error statting {}: {}", entryMetadata.tmpFileName, std::system_category().default_error_condition(errno).message());
      }
      archive_entry_copy_stat(entry.get(), &st);
    }

    archive_entry_set_filetype(entry.get(), entryMetadata.entryType);
    archive_entry_set_pathname(entry.get(), entryMetadata.entryName.c_str());
    archive_entry_set_perm(entry.get(), entryMetadata.entryPerm);
    archive_entry_set_size(entry.get(), gsl::narrow<la_int64_t>(entryMetadata.entrySize));
    archive_entry_set_uid(entry.get(), entryMetadata.entryUID);
    archive_entry_set_gid(entry.get(), entryMetadata.entryGID);
    archive_entry_set_mtime(entry.get(), entryMetadata.entryMTime, gsl::narrow<long>(entryMetadata.entryMTimeNsec));  // NOLINT long comes from libarchive API

    logger_->log_info("Writing {} with type {}, perms {}, size {}, uid {}, gid {}, mtime {},{}", entryMetadata.entryName, entryMetadata.entryType, entryMetadata.entryPerm,
                      entryMetadata.entrySize, entryMetadata.entryUID, entryMetadata.entryGID, entryMetadata.entryMTime, entryMetadata.entryMTimeNsec);

    archive_write_header(output_archive.get(), entry.get());

    // If entry is regular file, copy entry contents
    if (entryMetadata.entryType == AE_IFREG && entryMetadata.entrySize > 0) {
      logger_->log_info("UnfocusArchiveEntry writing {} bytes of "
                        "data from tmp file {} to archive entry {}",
                        st.st_size, entryMetadata.tmpFileName.string(), entryMetadata.entryName);
      std::ifstream ifs(entryMetadata.tmpFileName, std::ifstream::in | std::ios::binary);

      while (ifs.good()) {
        ifs.read(buf.data(), buf.size());
        auto len = gsl::narrow<size_t>(ifs.gcount());
        int64_t written = archive_write_data(output_archive.get(), buf.data(), len);
        if (written < 0) {
          logger_->log_error("UnfocusArchiveEntry failed to write data to "
                             "archive entry %s due to error: %s",
                             entryMetadata.entryName, archive_error_string(output_archive.get()));
        } else {
          nlen += written;
        }
      }

      ifs.close();

      // Remove the tmp file as we are through with it
      std::filesystem::remove(entryMetadata.tmpFileName);
    }

    archive_entry_clear(entry.get());
  }

  return nlen;
}

REGISTER_RESOURCE(UnfocusArchiveEntry, Processor);

}  // namespace org::apache::nifi::minifi::processors
