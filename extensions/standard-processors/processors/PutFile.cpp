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
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <set>
#include <utility>
#ifdef WIN32
#include <Windows.h>
#endif
#include "utils/file/FileUtils.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

std::shared_ptr<utils::IdGenerator> PutFile::id_generator_ = utils::IdGenerator::getIdGenerator();

core::Property PutFile::Directory(
    core::PropertyBuilder::createProperty("Directory")->withDescription("The output directory to which to put files")->supportsExpressionLanguage(true)->withDefaultValue(".")->build());

core::Property PutFile::ConflictResolution(
    core::PropertyBuilder::createProperty("Conflict Resolution Strategy")->withDescription("Indicates what should happen when a file with the same name already exists in the output directory")
        ->withAllowableValue<std::string>(CONFLICT_RESOLUTION_STRATEGY_FAIL)->withAllowableValue(CONFLICT_RESOLUTION_STRATEGY_IGNORE)->withAllowableValue(CONFLICT_RESOLUTION_STRATEGY_REPLACE)
        ->withDefaultValue(CONFLICT_RESOLUTION_STRATEGY_FAIL)->build());

core::Property PutFile::CreateDirs("Create Missing Directories", "If true, then missing destination directories will be created. "
                                   "If false, flowfiles are penalized and sent to failure.",
                                   "true", true, "", { "Directory" }, { });

core::Property PutFile::MaxDestFiles(
    core::PropertyBuilder::createProperty("Maximum File Count")->withDescription("Specifies the maximum number of files that can exist in the output directory")->withDefaultValue<int>(-1)->build());

#ifndef WIN32
core::Property PutFile::Permissions(
    core::PropertyBuilder::createProperty("Permissions")
      ->withDescription("Sets the permissions on the output file to the value of this attribute. "
                        "Must be an octal number (e.g. 644 or 0755). Not supported on Windows systems.")
      ->build());
core::Property PutFile::DirectoryPermissions(
    core::PropertyBuilder::createProperty("Directory Permissions")
      ->withDescription("Sets the permissions on the directories being created if 'Create Missing Directories' property is set. "
                        "Must be an octal number (e.g. 644 or 0755). Not supported on Windows systems.")
      ->build());
#endif

core::Relationship PutFile::Success("success", "All files are routed to success");
core::Relationship PutFile::Failure("failure", "Failed files (conflict, write failure, etc.) are transferred to failure");

void PutFile::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(Directory);
  properties.insert(ConflictResolution);
  properties.insert(CreateDirs);
  properties.insert(MaxDestFiles);
#ifndef WIN32
  properties.insert(Permissions);
  properties.insert(DirectoryPermissions);
#endif
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  relationships.insert(Failure);
  setSupportedRelationships(relationships);
}

void PutFile::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* /*sessionFactory*/) {
  if (!context->getProperty(ConflictResolution.getName(), conflict_resolution_)) {
    logger_->log_error("Conflict Resolution Strategy attribute is missing or invalid");
  }

  std::string value;
  context->getProperty(CreateDirs.getName(), value);
  try_mkdirs_ = utils::StringUtils::toBool(value).value_or(true);

  if (context->getProperty(MaxDestFiles.getName(), value)) {
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

  std::string directory;

  if (!context->getProperty(Directory, directory, flowFile)) {
    logger_->log_error("Directory attribute is missing or invalid");
  }

  if (IsNullOrEmpty(directory)) {
    logger_->log_error("Directory attribute evaluated to invalid value");
    session->transfer(flowFile, Failure);
    return;
  }

  std::string filename;
  flowFile->getAttribute(core::SpecialFlowAttribute::FILENAME, filename);
  std::string tmpFile = tmpWritePath(filename, directory);

  logger_->log_debug("PutFile using temporary file %s", tmpFile);

  // Determine dest full file paths
  std::stringstream destFileSs;
  destFileSs << directory << utils::file::FileUtils::get_separator() << filename;
  std::string destFile = destFileSs.str();

  logger_->log_debug("PutFile writing file %s into directory %s", filename, directory);

  if ((max_dest_files_ != -1) && utils::file::FileUtils::is_directory(directory.c_str())) {
    int64_t count = 0;

    // Callback, called for each file entry in the listed directory
    // Return value is used to break (false) or continue (true) listing
    auto lambda = [&count, this](const std::string&, const std::string&) -> bool {
      return ++count < max_dest_files_;
    };

    utils::file::FileUtils::list_dir(directory, lambda, logger_, false);

    if (count >= max_dest_files_) {
      logger_->log_warn("Routing to failure because the output directory %s has at least %u files, which exceeds the "
                        "configured max number of files", directory, max_dest_files_);
      session->transfer(flowFile, Failure);
      return;
    }
  }

  if (utils::file::exists(destFile)) {
    logger_->log_warn("Destination file %s exists; applying Conflict Resolution Strategy: %s", destFile, conflict_resolution_);

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

std::string PutFile::tmpWritePath(const std::string &filename, const std::string &directory) const {
  utils::Identifier tmpFileUuid = id_generator_->generate();
  std::stringstream tmpFileSs;
  tmpFileSs << directory;
  auto lastSeparatorPos = filename.find_last_of(utils::file::FileUtils::get_separator());

  if (lastSeparatorPos == std::string::npos) {
    tmpFileSs << utils::file::FileUtils::get_separator() << "." << filename;
  } else {
    tmpFileSs << utils::file::FileUtils::get_separator() << filename.substr(0, lastSeparatorPos) << utils::file::FileUtils::get_separator() << "." << filename.substr(lastSeparatorPos + 1);
  }

  tmpFileSs << "." << tmpFileUuid.to_string();
  std::string tmpFile = tmpFileSs.str();
  return tmpFile;
}

bool PutFile::putFile(core::ProcessSession *session, std::shared_ptr<core::FlowFile> flowFile, const std::string &tmpFile, const std::string &destFile, const std::string &destDir) {
  if (!utils::file::exists(destDir) && try_mkdirs_) {
    // Attempt to create directories in file's path
    std::stringstream dir_path_stream;

    logger_->log_debug("Destination directory does not exist; will attempt to create: ", destDir);
    size_t i = 0;
    auto pos = destFile.find(utils::file::FileUtils::get_separator());

    while (pos != std::string::npos) {
      auto dir_path_component = destFile.substr(i, pos - i);
      dir_path_stream << dir_path_component;
      auto dir_path = dir_path_stream.str();

      if (!dir_path_component.empty()) {
        logger_->log_debug("Attempting to create directory if it does not already exist: %s", dir_path);
        if (!utils::file::FileUtils::exists(dir_path)) {
          utils::file::FileUtils::create_dir(dir_path, false);
#ifndef WIN32
          if (directory_permissions_.valid()) {
            utils::file::FileUtils::set_permissions(dir_path, directory_permissions_.getValue());
          }
#endif
        }

        dir_path_stream << utils::file::FileUtils::get_separator();
      } else if (pos == 0) {
        // Support absolute paths
        dir_path_stream << utils::file::FileUtils::get_separator();
      }

      i = pos + 1;
      pos = destFile.find(utils::file::FileUtils::get_separator(), pos + 1);
    }
  }

  bool success = false;

  if (flowFile->getSize() > 0) {
    ReadCallback cb(tmpFile, destFile);
    session->read(flowFile, &cb);
    logger_->log_debug("Committing %s", destFile);
    success = cb.commit();
  } else {
    std::ofstream outfile(destFile, std::ios::out | std::ios::binary);
    if (!outfile.good()) {
      logger_->log_error("Failed to create empty file: %s", destFile);
    } else {
      success = true;
    }
  }

#ifndef WIN32
  if (permissions_.valid()) {
    utils::file::FileUtils::set_permissions(destFile, permissions_.getValue());
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
  context->getProperty(Permissions.getName(), permissions_str);
  if (permissions_str.empty()) {
    return;
  }

  try {
    permissions_.setValue(std::stoi(permissions_str, 0, 8));
  } catch(const std::exception&) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Permissions property is invalid");
  }

  if (!permissions_.valid()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Permissions property is invalid: out of bounds");
  }
}

void PutFile::getDirectoryPermissions(core::ProcessContext *context) {
  std::string dir_permissions_str;
  context->getProperty(DirectoryPermissions.getName(), dir_permissions_str);
  if (dir_permissions_str.empty()) {
    return;
  }

  try {
    directory_permissions_.setValue(std::stoi(dir_permissions_str, 0, 8));
  } catch(const std::exception&) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Directory Permissions property is invalid");
  }

  if (!directory_permissions_.valid()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Directory Permissions property is invalid: out of bounds");
  }
}
#endif

PutFile::ReadCallback::ReadCallback(std::string tmp_file, std::string dest_file)
    : tmp_file_(std::move(tmp_file)),
      dest_file_(std::move(dest_file)) {
}

// Copy the entire file contents to the temporary file
int64_t PutFile::ReadCallback::process(const std::shared_ptr<io::BaseStream>& stream) {
  // Copy file contents into tmp file
  write_succeeded_ = false;
  size_t size = 0;
  uint8_t buffer[1024];

  std::ofstream tmp_file_os(tmp_file_, std::ios::out | std::ios::binary);

  do {
    const auto read = stream->read(buffer, 1024);
    if (io::isError(read)) return -1;
    if (read == 0) break;
    tmp_file_os.write(reinterpret_cast<char *>(buffer), gsl::narrow<std::streamsize>(read));
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

  logger_->log_info("PutFile committing put file operation to %s", dest_file_);

  if (write_succeeded_) {
    if (rename(tmp_file_.c_str(), dest_file_.c_str())) {
      logger_->log_info("PutFile commit put file operation to %s failed because rename() call failed", dest_file_);
    } else {
      success = true;
      logger_->log_info("PutFile commit put file operation to %s succeeded", dest_file_);
    }
  } else {
    logger_->log_error("PutFile commit put file operation to %s failed because write failed", dest_file_);
  }

  return success;
}

// Clean up resources
PutFile::ReadCallback::~ReadCallback() {
  // Clean up tmp file, if necessary
  std::remove(tmp_file_.c_str());
}

REGISTER_RESOURCE(PutFile, "Writes the contents of a FlowFile to the local file system");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
