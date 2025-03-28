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
#include "utils/file/FileUtils.h"
#include "utils/file/FileWriterCallback.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/gsl.h"
#include "core/Resource.h"
#include "core/ProcessContext.h"

namespace org::apache::nifi::minifi::processors {

std::shared_ptr<utils::IdGenerator> PutFile::id_generator_ = utils::IdGenerator::getIdGenerator();

void PutFile::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void PutFile::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  conflict_resolution_strategy_ = utils::parseEnumProperty<FileExistsResolutionStrategy>(context, ConflictResolution);
  try_mkdirs_ = utils::parseBoolProperty(context, CreateDirs);
  if (auto max_dest_files = utils::parseOptionalI64Property(context, MaxDestFiles); max_dest_files && *max_dest_files > 0) {
    max_dest_files_ = gsl::narrow_cast<uint64_t>(*max_dest_files);
  }

#ifndef WIN32
  getPermissions(context);
  getDirectoryPermissions(context);
#endif
}

std::optional<std::filesystem::path> PutFile::getDestinationPath(core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file) {
  std::filesystem::path directory;
  if (auto directory_str = context.getProperty(Directory, flow_file.get()); directory_str && !directory_str->empty()) {
    directory = *directory_str;
  } else {
    logger_->log_error("Directory attribute evaluated to invalid value");
    return std::nullopt;
  }
  auto file_name_str = flow_file->getAttribute(core::SpecialFlowAttribute::FILENAME).value_or(flow_file->getUUIDStr());

  return directory / file_name_str;
}

bool PutFile::directoryIsFull(const std::filesystem::path& directory) const {
  return max_dest_files_ && utils::file::countNumberOfFiles(directory) >= *max_dest_files_;
}

void PutFile::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  std::shared_ptr<core::FlowFile> flow_file = session.get();

  // Do nothing if there are no incoming files
  if (!flow_file) {
    return;
  }

  auto dest_path = getDestinationPath(context, flow_file);
  if (!dest_path) {
    return session.transfer(flow_file, Failure);
  }

  logger_->log_trace("PutFile writing file {} into directory {}", dest_path->filename(), dest_path->parent_path());

  if (directoryIsFull(dest_path->parent_path())) {
    logger_->log_warn("Routing to failure because the output directory {} has at least {} files, which exceeds the "
                      "configured max number of files", dest_path->parent_path(), *max_dest_files_);
    return session.transfer(flow_file, Failure);
  }

  if (utils::file::exists(*dest_path)) {
    logger_->log_info("Destination file {} exists; applying Conflict Resolution Strategy: {}", dest_path->string(), magic_enum::enum_name(conflict_resolution_strategy_));
    if (conflict_resolution_strategy_ == FileExistsResolutionStrategy::fail) {
      return session.transfer(flow_file, Failure);
    } else if (conflict_resolution_strategy_ == FileExistsResolutionStrategy::ignore) {
      return session.transfer(flow_file, Success);
    }
  }

  putFile(session, flow_file, *dest_path);
}

void PutFile::prepareDirectory(const std::filesystem::path& directory_path) const {
  if (!utils::file::exists(directory_path) && try_mkdirs_) {
    logger_->log_debug("Destination directory does not exist; will attempt to create: {}", directory_path);
    utils::file::create_dir(directory_path, true);
#ifndef WIN32
    if (directory_permissions_.valid()) {
      utils::file::set_permissions(directory_path, directory_permissions_.getValue());
    }
#endif
  }
}

void PutFile::putFile(core::ProcessSession& session,
                      const std::shared_ptr<core::FlowFile>& flow_file,
                      const std::filesystem::path& dest_file) {
  prepareDirectory(dest_file.parent_path());

  bool success = false;

  utils::FileWriterCallback file_writer_callback(dest_file, logger_.get());
  auto read_result = session.read(flow_file, std::ref(file_writer_callback));
  if (io::isError(read_result)) {
    logger_->log_error("Failed to write to {}", dest_file);
    success = false;
  } else {
    success = file_writer_callback.commit();
  }

#ifndef WIN32
  if (permissions_.valid()) {
    utils::file::set_permissions(dest_file, permissions_.getValue());
  }
#endif

  session.transfer(flow_file, success ? Success : Failure);
}

#ifndef WIN32
void PutFile::getPermissions(const core::ProcessContext& context) {
  const std::string permissions_str = context.getProperty(Permissions).value_or("");
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

void PutFile::getDirectoryPermissions(const core::ProcessContext& context) {
  const std::string dir_permissions_str = context.getProperty(DirectoryPermissions).value_or("");
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

REGISTER_RESOURCE(PutFile, Processor);

}  // namespace org::apache::nifi::minifi::processors
