/**
 * @file PutFile.cpp
 * PutFile class implementation
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

#include "PutFile.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#ifdef WIN32
#include <Windows.h>
#endif
#include "utils/file/FileUtils.h"
#include "utils/gsl.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::processors {

std::shared_ptr<utils::IdGenerator> PutFile::id_generator_ = utils::IdGenerator::getIdGenerator();

void PutFile::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void PutFile::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* /*sessionFactory*/) {
  if (!context->getProperty(ConflictResolution, conflict_resolution_)) {
    logger_->log_error("Conflict Resolution Strategy attribute is missing or invalid");
  }

  std::string value;
  context->getProperty(CreateDirs, value);
  try_mkdirs_ = utils::StringUtils::toBool(value).value_or(true);

  if (context->getProperty(MaxDestFiles, value)) {
    core::Property::StringToInt(value, max_dest_files_);
  }

#ifndef WIN32
  getPermissions(context);
  getDirectoryPermissions(context);
#endif
}

void PutFile::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  if (IsNullOrEmpty(conflict_resolution_)) {
    logger_->log_error("Conflict resolution value is invalid");
    context->yield();
    return;
  }

  std::shared_ptr<core::FlowFile> flowFile = session->get();

  // Do nothing if there are no incoming files
  if (!flowFile) {
    return;
  }

  session->remove(flowFile);

  std::filesystem::path directory;

  if (auto directory_str = context->getProperty(Directory, flowFile)) {
    directory = *directory_str;
  } else {
    logger_->log_error("Directory attribute is missing or invalid");
  }

  if (IsNullOrEmpty(directory)) {
    logger_->log_error("Directory attribute evaluated to invalid value");
    session->transfer(flowFile, Failure);
    return;
  }

  std::string filename;
  flowFile->getAttribute(core::SpecialFlowAttribute::FILENAME, filename);
  auto tmpFile = tmpWritePath(filename, directory);

  logger_->log_debug("PutFile using temporary file %s", tmpFile.string());

  // Determine dest full file paths
  auto destFile = directory / filename;

  logger_->log_debug("PutFile writing file %s into directory %s", filename, directory.string());

  if ((max_dest_files_ != -1) && utils::file::is_directory(directory)) {
    int64_t count = 0;

    // Callback, called for each file entry in the listed directory
    // Return value is used to break (false) or continue (true) listing
    auto lambda = [&count, this](const std::filesystem::path&, const std::filesystem::path&) -> bool {
      return ++count < max_dest_files_;
    };

    utils::file::list_dir(directory, lambda, logger_, false);

    if (count >= max_dest_files_) {
      logger_->log_warn("Routing to failure because the output directory %s has at least %u files, which exceeds the "
                        "configured max number of files", directory.string(), max_dest_files_);
      session->transfer(flowFile, Failure);
      return;
    }
  }

  if (utils::file::exists(destFile)) {
    logger_->log_warn("Destination file %s exists; applying Conflict Resolution Strategy: %s", destFile.string(), conflict_resolution_);

    if (conflict_resolution_ == CONFLICT_RESOLUTION_STRATEGY_REPLACE) {
      putFile(session, flowFile, tmpFile, destFile, directory);
    } else if (conflict_resolution_ == CONFLICT_RESOLUTION_STRATEGY_IGNORE) {
      session->transfer(flowFile, Success);
    } else {
      session->transfer(flowFile, Failure);
    }
  } else {
    putFile(session, flowFile, tmpFile, destFile, directory);
  }
}

std::filesystem::path PutFile::tmpWritePath(const std::filesystem::path& filename, const std::filesystem::path& directory) {
  utils::Identifier tmpFileUuid = id_generator_->generate();
  auto new_filename = std::filesystem::path("." + filename.filename().string());
  new_filename += "." + tmpFileUuid.to_string();
  return (directory / filename.parent_path() / new_filename);
}

bool PutFile::putFile(core::ProcessSession *session,
                      const std::shared_ptr<core::FlowFile>& flowFile,
                      const std::filesystem::path& tmpFile,
                      const std::filesystem::path& destFile,
                      const std::filesystem::path& destDir) {
  if (!utils::file::exists(destDir) && try_mkdirs_) {
    logger_->log_debug("Destination directory does not exist; will attempt to create: %s", destDir.string());
    utils::file::create_dir(destDir, true);
#ifndef WIN32
    if (directory_permissions_.valid()) {
      utils::file::set_permissions(destDir, directory_permissions_.getValue());
    }
#endif
  }

  bool success = false;

  if (flowFile->getSize() > 0) {
    ReadCallback cb(tmpFile, destFile);
    session->read(flowFile, std::ref(cb));
    logger_->log_debug("Committing %s", destFile.string());
    success = cb.commit();
  } else {
    std::ofstream outfile(destFile, std::ios::out | std::ios::binary);
    if (!outfile.good()) {
      logger_->log_error("Failed to create empty file: %s", destFile.string());
    } else {
      success = true;
    }
  }

#ifndef WIN32
  if (permissions_.valid()) {
    utils::file::set_permissions(destFile, permissions_.getValue());
  }
#endif

  if (success) {
    session->transfer(flowFile, Success);
    return true;
  } else {
    session->transfer(flowFile, Failure);
  }
  return false;
}

#ifndef WIN32
void PutFile::getPermissions(core::ProcessContext *context) {
  std::string permissions_str;
  context->getProperty(Permissions, permissions_str);
  if (permissions_str.empty()) {
    return;
  }

  try {
    permissions_.setValue(std::stoi(permissions_str, nullptr, 8));
  } catch(const std::exception&) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Permissions property is invalid");
  }

  if (!permissions_.valid()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Permissions property is invalid: out of bounds");
  }
}

void PutFile::getDirectoryPermissions(core::ProcessContext *context) {
  std::string dir_permissions_str;
  context->getProperty(DirectoryPermissions, dir_permissions_str);
  if (dir_permissions_str.empty()) {
    return;
  }

  try {
    directory_permissions_.setValue(std::stoi(dir_permissions_str, nullptr, 8));
  } catch(const std::exception&) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Directory Permissions property is invalid");
  }

  if (!directory_permissions_.valid()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Directory Permissions property is invalid: out of bounds");
  }
}
#endif

PutFile::ReadCallback::ReadCallback(std::filesystem::path tmp_file, std::filesystem::path dest_file)
    : tmp_file_(std::move(tmp_file)),
      dest_file_(std::move(dest_file)) {
}

// Copy the entire file contents to the temporary file
int64_t PutFile::ReadCallback::operator()(const std::shared_ptr<io::InputStream>& stream) {
  // Copy file contents into tmp file
  write_succeeded_ = false;
  size_t size = 0;
  std::array<std::byte, 1024> buffer{};

  std::ofstream tmp_file_os(tmp_file_, std::ios::out | std::ios::binary);

  do {
    const auto read = stream->read(buffer);
    if (io::isError(read)) return -1;
    if (read == 0) break;
    tmp_file_os.write(reinterpret_cast<char *>(buffer.data()), gsl::narrow<std::streamsize>(read));
    size += read;
  } while (size < stream->size());

  tmp_file_os.close();

  if (tmp_file_os) {
    write_succeeded_ = true;
  }

  return gsl::narrow<int64_t>(size);
}

// Renames tmp file to final destination
// Returns true if commit succeeded
bool PutFile::ReadCallback::commit() {
  bool success = false;

  logger_->log_info("PutFile committing put file operation to %s", dest_file_.string());

  if (write_succeeded_) {
    std::error_code rename_error;
    std::filesystem::rename(tmp_file_, dest_file_, rename_error);
    if (rename_error) {
      logger_->log_info("PutFile commit put file operation to %s failed because std::filesystem::rename call failed", dest_file_.string());
    } else {
      success = true;
      logger_->log_info("PutFile commit put file operation to %s succeeded", dest_file_.string());
    }
  } else {
    logger_->log_error("PutFile commit put file operation to %s failed because write failed", dest_file_.string());
  }

  return success;
}

// Clean up resources
PutFile::ReadCallback::~ReadCallback() {
  // Clean up tmp file, if necessary
  std::filesystem::remove(tmp_file_);
}

REGISTER_RESOURCE(PutFile, Processor);

}  // namespace org::apache::nifi::minifi::processors
