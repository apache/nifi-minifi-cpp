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

#include "processors/PutFile.h"
#include "utils/file/FileUtils.h"
#include <sys/stat.h>
#include <uuid/uuid.h>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <set>
#ifdef WIN32
#include <Windows.h>
#endif

#ifndef S_ISDIR
#define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#endif

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

core::Relationship PutFile::Success("success", "All files are routed to success");
core::Relationship PutFile::Failure("failure", "Failed files (conflict, write failure, etc.) are transferred to failure");

void PutFile::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(Directory);
  properties.insert(ConflictResolution);
  properties.insert(CreateDirs);
  properties.insert(MaxDestFiles);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  relationships.insert(Failure);
  setSupportedRelationships(relationships);
}

void PutFile::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) {
  if (!context->getProperty(ConflictResolution.getName(), conflict_resolution_)) {
    logger_->log_error("Conflict Resolution Strategy attribute is missing or invalid");
  }

  std::string value;
  context->getProperty(CreateDirs.getName(), value);
  utils::StringUtils::StringToBool(value, try_mkdirs_);

  if (context->getProperty(MaxDestFiles.getName(), value)) {
    core::Property::StringToInt(value, max_dest_files_);
  }
}

void PutFile::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  if (IsNullOrEmpty(conflict_resolution_)) {
    logger_->log_error("Conflict resolution value is invalid");
    context->yield();
    return;
  }

  std::shared_ptr<FlowFileRecord> flowFile = std::static_pointer_cast<FlowFileRecord>(session->get());

  // Do nothing if there are no incoming files
  if (!flowFile) {
    return;
  }

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
  flowFile->getKeyedAttribute(FILENAME, filename);
  std::string tmpFile = tmpWritePath(filename, directory);

  logger_->log_debug("PutFile using temporary file %s", tmpFile);

  // Determine dest full file paths
  std::stringstream destFileSs;
  destFileSs << directory << "/" << filename;
  std::string destFile = destFileSs.str();

  logger_->log_debug("PutFile writing file %s into directory %s", filename, directory);

  // If file exists, apply conflict resolution strategy
  struct stat statResult;

  if ((max_dest_files_ != -1) && (stat(directory.c_str(), &statResult) == 0)) {
    // something exists at directory path
    if (S_ISDIR(statResult.st_mode)) {
      // it's a directory, count the files
      int64_t ct = 0;
#ifndef WIN32
      DIR *myDir = opendir(directory.c_str());
      if (!myDir) {
        logger_->log_warn("Could not open %s", directory);
        session->transfer(flowFile, Failure);
        return;
      }
      struct dirent* entry = nullptr;

      while ((entry = readdir(myDir)) != nullptr) {
        if ((strcmp(entry->d_name, ".") != 0) && (strcmp(entry->d_name, "..") != 0)) {
          ct++;
          if (ct >= max_dest_files_) {
            logger_->log_warn("Routing to failure because the output directory %s has at least %u files, which exceeds the "
                              "configured max number of files",
                              directory, max_dest_files_);
            session->transfer(flowFile, Failure);
            closedir(myDir);
            return;
          }
        }
      }
      closedir(myDir);
#else
      HANDLE hFind;
      WIN32_FIND_DATA FindFileData;

      if ((hFind = FindFirstFile(directory.c_str(), &FindFileData)) != INVALID_HANDLE_VALUE) {
        do {
          if ((strcmp(FindFileData.cFileName, ".") != 0) && (strcmp(FindFileData.cFileName, "..") != 0)) {
            ct++;
            if (ct >= max_dest_files_) {
              logger_->log_warn("Routing to failure because the output directory %s has at least %u files, which exceeds the "
                  "configured max number of files", directory, max_dest_files_);
              session->transfer(flowFile, Failure);
              FindClose(hFind);
              return;
            }
          }
        }while (FindNextFile(hFind, &FindFileData));
        FindClose(hFind);
      }
#endif
    }
  }

  if (stat(destFile.c_str(), &statResult) == 0) {
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
  utils::Identifier tmpFileUuid;
  id_generator_->generate(tmpFileUuid);
  std::stringstream tmpFileSs;
  tmpFileSs << directory;
  auto lastSeparatorPos = filename.find_last_of("/");

  if (lastSeparatorPos == std::string::npos) {
    tmpFileSs << "/." << filename;
  } else {
    tmpFileSs << "/" << filename.substr(0, lastSeparatorPos) << "/." << filename.substr(lastSeparatorPos + 1);
  }

  tmpFileSs << "." << tmpFileUuid.to_string();
  std::string tmpFile = tmpFileSs.str();
  return tmpFile;
}

bool PutFile::putFile(core::ProcessSession *session, std::shared_ptr<FlowFileRecord> flowFile, const std::string &tmpFile, const std::string &destFile, const std::string &destDir) {
  struct stat dir_stat;

  if (stat(destDir.c_str(), &dir_stat) && try_mkdirs_) {
    // Attempt to create directories in file's path
    std::stringstream dir_path_stream;

    logger_->log_debug("Destination directory does not exist; will attempt to create: ", destDir);
    size_t i = 0;
    auto pos = destFile.find('/');

    while (pos != std::string::npos) {
      auto dir_path_component = destFile.substr(i, pos - i);
      dir_path_stream << dir_path_component;
      auto dir_path = dir_path_stream.str();

      if (!dir_path_component.empty()) {
        logger_->log_debug("Attempting to create directory if it does not already exist: %s", dir_path);
        utils::file::FileUtils::create_dir(dir_path);
        dir_path_stream << '/';
      } else if (pos == 0) {
        // Support absolute paths
        dir_path_stream << '/';
      }

      i = pos + 1;
      pos = destFile.find('/', pos + 1);
    }
  }

  ReadCallback cb(tmpFile, destFile);
  session->read(flowFile, &cb);

  logger_->log_debug("Committing %s", destFile);
  if (cb.commit()) {
    session->transfer(flowFile, Success);
    return true;
  } else {
    session->transfer(flowFile, Failure);
  }
  return false;
}

PutFile::ReadCallback::ReadCallback(const std::string &tmp_file, const std::string &dest_file)
    : tmp_file_(tmp_file),
      dest_file_(dest_file),
      logger_(logging::LoggerFactory<PutFile::ReadCallback>::getLogger()) {
}

// Copy the entire file contents to the temporary file
int64_t PutFile::ReadCallback::process(std::shared_ptr<io::BaseStream> stream) {
  // Copy file contents into tmp file
  write_succeeded_ = false;
  size_t size = 0;
  uint8_t buffer[1024];

  std::ofstream tmp_file_os(tmp_file_);

  do {
    int read = stream->read(buffer, 1024);

    if (read < 0) {
      return -1;
    }

    if (read == 0) {
      break;
    }

    tmp_file_os.write(reinterpret_cast<char *>(buffer), read);
    size += read;
  } while (size < stream->getSize());

  tmp_file_os.close();

  if (tmp_file_os) {
    write_succeeded_ = true;
  }

  return size;
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
  unlink(tmp_file_.c_str());
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
