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
#include <stdio.h>
#include <uuid/uuid.h>
#include <sstream>
#include <string>
#include <iostream>
#include <memory>
#include <set>
#include <fstream>
#include "io/validation.h"
#include "utils/StringUtils.h"
#include "utils/TimeUtil.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property PutFile::Directory("Output Directory",
                                  "The output directory to which to put files",
                                  ".");
core::Property PutFile::ConflictResolution(
    "Conflict Resolution Strategy",
    "Indicates what should happen when a file with the same name already exists in the output directory",
    CONFLICT_RESOLUTION_STRATEGY_FAIL);

core::Relationship PutFile::Success("success",
                                    "All files are routed to success");
core::Relationship PutFile::Failure(
    "failure",
    "Failed files (conflict, write failure, etc.) are transferred to failure");

void PutFile::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(Directory);
  properties.insert(ConflictResolution);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  relationships.insert(Failure);
  setSupportedRelationships(relationships);
}

void PutFile::onSchedule(core::ProcessContext *context,
                         core::ProcessSessionFactory *sessionFactory) {
  if (!context->getProperty(Directory.getName(), directory_)) {
    logger_->log_error("Directory attribute is missing or invalid");
  }

  if (!context->getProperty(ConflictResolution.getName(),
                            conflict_resolution_)) {
    logger_->log_error(
        "Conflict Resolution Strategy attribute is missing or invalid");
  }
}

void PutFile::onTrigger(core::ProcessContext *context,
                        core::ProcessSession *session) {
  if (IsNullOrEmpty(directory_) || IsNullOrEmpty(conflict_resolution_)) {
    context->yield();
    return;
  }

  std::shared_ptr<FlowFileRecord> flowFile = std::static_pointer_cast<
      FlowFileRecord>(session->get());

  // Do nothing if there are no incoming files
  if (!flowFile) {
    return;
  }

  std::string filename;
  flowFile->getKeyedAttribute(FILENAME, filename);

  // Generate a safe (universally-unique) temporary filename on the same partition
  char tmpFileUuidStr[37];
  uuid_t tmpFileUuid;
  uuid_generate(tmpFileUuid);
  uuid_unparse_lower(tmpFileUuid, tmpFileUuidStr);
  std::stringstream tmpFileSs;
  tmpFileSs << directory_ << "/." << filename << "." << tmpFileUuidStr;
  std::string tmpFile = tmpFileSs.str();
  logger_->log_info("PutFile using temporary file %s", tmpFile.c_str());

  // Determine dest full file paths
  std::stringstream destFileSs;
  destFileSs << directory_ << "/" << filename;
  std::string destFile = destFileSs.str();

  logger_->log_info("PutFile writing file %s into directory %s",
                    filename.c_str(), directory_.c_str());

  // If file exists, apply conflict resolution strategy
  struct stat statResult;

  if (stat(destFile.c_str(), &statResult) == 0) {
    logger_->log_info(
        "Destination file %s exists; applying Conflict Resolution Strategy: %s",
        destFile.c_str(), conflict_resolution_.c_str());

    if (conflict_resolution_ == CONFLICT_RESOLUTION_STRATEGY_REPLACE) {
      putFile(session, flowFile, tmpFile, destFile);
    } else if (conflict_resolution_ == CONFLICT_RESOLUTION_STRATEGY_IGNORE) {
      session->transfer(flowFile, Success);
    } else {
      session->transfer(flowFile, Failure);
    }
  } else {
    putFile(session, flowFile, tmpFile, destFile);
  }
}

bool PutFile::putFile(core::ProcessSession *session,
                      std::shared_ptr<FlowFileRecord> flowFile,
                      const std::string &tmpFile, const std::string &destFile) {
  ReadCallback cb(tmpFile, destFile);
  session->read(flowFile, &cb);

  if (cb.commit()) {
    session->transfer(flowFile, Success);
    return true;
  } else {
    session->transfer(flowFile, Failure);
  }
  return false;
}

PutFile::ReadCallback::ReadCallback(const std::string &tmpFile,
                                    const std::string &destFile)
    : _tmpFile(tmpFile),
      _tmpFileOs(tmpFile),
      _destFile(destFile),
      logger_(logging::LoggerFactory<PutFile::ReadCallback>::getLogger()) {
}

// Copy the entire file contents to the temporary file
void PutFile::ReadCallback::process(std::ifstream *stream) {
  // Copy file contents into tmp file
  _writeSucceeded = false;
  _tmpFileOs << stream->rdbuf();
  _writeSucceeded = true;
}

// Renames tmp file to final destination
// Returns true if commit succeeded
bool PutFile::ReadCallback::commit() {
  bool success = false;

  logger_->log_info("PutFile committing put file operation to %s",
                    _destFile.c_str());

  if (_writeSucceeded) {
    _tmpFileOs.close();

    if (rename(_tmpFile.c_str(), _destFile.c_str())) {
      logger_->log_info(
          "PutFile commit put file operation to %s failed because rename() call failed",
          _destFile.c_str());
    } else {
      success = true;
      logger_->log_info("PutFile commit put file operation to %s succeeded",
                        _destFile.c_str());
    }
  } else {
    logger_->log_error(
        "PutFile commit put file operation to %s failed because write failed",
        _destFile.c_str());
  }

  return success;
}

// Clean up resources
PutFile::ReadCallback::~ReadCallback() {
  // Close tmp file
  _tmpFileOs.close();

  // Clean up tmp file, if necessary
  unlink(_tmpFile.c_str());
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
