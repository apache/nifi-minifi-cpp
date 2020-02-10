/**
 * @file TailFile.cpp
 * TailFile class implementation
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
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>

#include <limits.h>
#ifndef WIN32
#include <dirent.h>
#include <unistd.h>
#endif
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <memory>
#include <algorithm>
#include <sstream>
#include <string>
#include <iostream>
#include "utils/file/FileUtils.h"
#include "utils/file/PathUtils.h"
#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"
#include "utils/RegexUtils.h"
#ifdef HAVE_REGEX_CPP
#include <regex>
#else
#include <regex.h>
#endif
#include "TailFile.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

#ifndef S_ISDIR
#define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#endif

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-compare"
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property TailFile::FileName("File to Tail", "Fully-qualified filename of the file that should be tailed when using single file mode, or a file regex when using multifile mode", "");
core::Property TailFile::StateFile("State File", "Specifies the file that should be used for storing state about"
                                   " what data has been ingested so that upon restart NiFi can resume from where it left off",
                                   "TailFileState");
core::Property TailFile::Delimiter("Input Delimiter", "Specifies the character that should be used for delimiting the data being tailed"
                                   "from the incoming file."
                                   "If none is specified, data will be ingested as it becomes available.",
                                   "");

core::Property TailFile::TailMode(
    core::PropertyBuilder::createProperty("tail-mode", "Tailing Mode")->withDescription(
        "Specifies the tail file mode. In 'Single file' mode only a single file will be watched. "
        "In 'Multiple file' mode a regex may be used. Note that in multiple file mode we will still continue to watch for rollover on the initial set of watched files. "
        "The Regex used to locate multiple files will be run during the schedule phrase. Note that if rotated files are matched by the regex, those files will be tailed.")->isRequired(true)
        ->withAllowableValue<std::string>("Single file")->withAllowableValue("Multiple file")->withDefaultValue("Single file")->build());

core::Property TailFile::BaseDirectory(core::PropertyBuilder::createProperty("tail-base-directory", "Base Directory")->isRequired(false)->build());

core::Relationship TailFile::Success("success", "All files are routed to success");

const char *TailFile::CURRENT_STR = "CURRENT.";
const char *TailFile::POSITION_STR = "POSITION.";

void TailFile::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(FileName);
  properties.insert(StateFile);
  properties.insert(Delimiter);
  properties.insert(TailMode);
  properties.insert(BaseDirectory);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void TailFile::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  std::lock_guard<std::mutex> tail_lock(tail_file_mutex_);

  // can perform these in notifyStop, but this has the same outcome
  tail_states_.clear();
  state_recovered_ = false;

  std::string value;

  if (context->getProperty(Delimiter.getName(), value)) {
    delimiter_ = value;
  }

  std::string mode;
  context->getProperty(TailMode.getName(), mode);

  std::string file = "";
  if (!context->getProperty(FileName.getName(), file)) {
    throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "File to Tail is a required property");
  }
  if (mode == "Multiple file") {
    // file is a regex
    std::string base_dir;
    if (!context->getProperty(BaseDirectory.getName(), base_dir)) {
      throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "Base directory is required for multiple tail mode.");
    }

    auto fileRegexSelect = [&](const std::string& path, const std::string& filename) -> bool {
      if (acceptFile(file, filename)) {
        tail_states_.insert(std::make_pair(filename, TailState {path, filename, 0, 0}));
      }
      return true;
    };

    utils::file::FileUtils::list_dir(base_dir, fileRegexSelect, logger_, false);

  } else {
    std::string fileLocation, fileName;
    if (utils::file::PathUtils::getFileNameAndPath(file, fileLocation, fileName)) {
      tail_states_.insert(std::make_pair(fileName, TailState { fileLocation, fileName, 0, 0 }));
    } else {
      throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "File to tail must be a fully qualified file");
    }
  }
}

bool TailFile::acceptFile(const std::string &fileFilter, const std::string &file) {
  utils::Regex rgx(fileFilter);
  return rgx.match(file);
}

std::string TailFile::trimLeft(const std::string& s) {
  return org::apache::nifi::minifi::utils::StringUtils::trimLeft(s);
}

std::string TailFile::trimRight(const std::string& s) {
  return org::apache::nifi::minifi::utils::StringUtils::trimRight(s);
}

void TailFile::parseStateFileLine(char *buf) {
  char *line = buf;

  logger_->log_trace("Received line %s", buf);

  while ((line[0] == ' ') || (line[0] == '\t'))
    ++line;

  char first = line[0];
  if ((first == '\0') || (first == '#') || (first == '\r') || (first == '\n') || (first == '=')) {
    return;
  }

  char *equal = strchr(line, '=');
  if (equal == NULL) {
    return;
  }

  equal[0] = '\0';
  std::string key = line;

  equal++;
  while ((equal[0] == ' ') || (equal[0] == '\t'))
    ++equal;

  first = equal[0];
  if ((first == '\0') || (first == '\r') || (first == '\n')) {
    return;
  }

  std::string value = equal;
  key = trimRight(key);
  value = trimRight(value);

  if (key == "FILENAME") {
    std::string fileLocation, fileName;
    if (utils::file::PathUtils::getFileNameAndPath(value, fileLocation, fileName)) {
      logger_->log_debug("Received path %s, file %s", fileLocation, fileName);
      tail_states_.insert(std::make_pair(fileName, TailState { fileLocation, fileName, 0, 0 }));
    } else {
      tail_states_.insert(std::make_pair(value, TailState { fileLocation, value, 0, 0 }));
    }
  }
  if (key == "POSITION") {
    // for backwards compatibility
    if (tail_states_.size() != 1) {
      throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "Incompatible state file types");
    }
    const auto position = std::stoull(value);
    logger_->log_debug("Received position %d", position);
    tail_states_.begin()->second.currentTailFilePosition_ = position;
  }
  if (key.find(CURRENT_STR) == 0) {
    const auto file = key.substr(strlen(CURRENT_STR));
    std::string fileLocation, fileName;
    if (utils::file::PathUtils::getFileNameAndPath(value, fileLocation, fileName)) {
      tail_states_[file].path_ = fileLocation;
      tail_states_[file].current_file_name_ = fileName;
    } else {
      throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "State file contains an invalid file name");
    }
  }

  if (key.find(POSITION_STR) == 0) {
    const auto file = key.substr(strlen(POSITION_STR));
    tail_states_[file].currentTailFilePosition_ = std::stoull(value);
  }

  return;
}

bool TailFile::recoverState() {
  std::ifstream file(state_file_.c_str(), std::ifstream::in);
  if (!file.good()) {
    logger_->log_error("load state file failed %s", state_file_);
    return false;
  }
  tail_states_.clear();
  char buf[BUFFER_SIZE];
  for (file.getline(buf, BUFFER_SIZE); file.good(); file.getline(buf, BUFFER_SIZE)) {
    parseStateFileLine(buf);
  }

  /**
   * recover times and validate that we have paths
   */

  for (auto &state : tail_states_) {
    std::string fileLocation, fileName;
    if (!utils::file::PathUtils::getFileNameAndPath(state.second.current_file_name_, fileLocation, fileName) && state.second.path_.empty()) {
      throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "State file does not contain a full path and file name");
    }
    struct stat sb;
    const auto fileFullName = state.second.path_ + utils::file::FileUtils::get_separator() + state.second.current_file_name_;
    if (stat(fileFullName.c_str(), &sb) == 0) {
      state.second.currentTailFileModificationTime_ = ((uint64_t) (sb.st_mtime) * 1000);
    }
  }

  logger_->log_debug("load state file succeeded for %s", state_file_);
  return true;
}

void TailFile::storeState() {
  std::ofstream file(state_file_.c_str());
  if (!file.is_open()) {
    logger_->log_error("store state file failed %s", state_file_);
    return;
  }
  for (const auto &state : tail_states_) {
    file << "FILENAME=" << state.first << "\n";
    file << CURRENT_STR << state.first << "=" << state.second.path_ << utils::file::FileUtils::get_separator() << state.second.current_file_name_ << "\n";
    file << POSITION_STR << state.first << "=" << state.second.currentTailFilePosition_ << "\n";
  }
  file.close();
}

static bool sortTailMatchedFileItem(TailMatchedFileItem i, TailMatchedFileItem j) {
  return (i.modifiedTime < j.modifiedTime);
}
void TailFile::checkRollOver(TailState &file, const std::string &base_file_name) {
  struct stat statbuf;
  std::vector<TailMatchedFileItem> matchedFiles;
  std::string fullPath = file.path_ + utils::file::FileUtils::get_separator() + file.current_file_name_;

  if (stat(fullPath.c_str(), &statbuf) == 0) {
    logger_->log_trace("Searching for files rolled over");
    std::string pattern = file.current_file_name_;
    std::size_t found = file.current_file_name_.find_last_of(".");
    if (found != std::string::npos)
      pattern = file.current_file_name_.substr(0, found);

    // Callback, called for each file entry in the listed directory
    // Return value is used to break (false) or continue (true) listing
    auto lambda = [&](const std::string& path, const std::string& filename) -> bool {
      struct stat sb;
      std::string fileFullName = path + utils::file::FileUtils::get_separator() + filename;
      if ((fileFullName.find(pattern) != std::string::npos) && stat(fileFullName.c_str(), &sb) == 0) {
        uint64_t candidateModTime = ((uint64_t) (sb.st_mtime) * 1000);
        if (candidateModTime >= file.currentTailFileModificationTime_) {
          logging::LOG_TRACE(logger_) << "File " << filename << " (short name " << file.current_file_name_ <<
          ") disk mod time " << candidateModTime << ", struct mod time " << file.currentTailFileModificationTime_ << ", size on disk " << sb.st_size << ", position " << file.currentTailFilePosition_;
          if (filename == file.current_file_name_ && candidateModTime == file.currentTailFileModificationTime_ &&
              sb.st_size == file.currentTailFilePosition_) {
            return true;  // Skip the current file as a candidate in case it wasn't updated
      }
      TailMatchedFileItem item;
      item.fileName = filename;
      item.modifiedTime = ((uint64_t) (sb.st_mtime) * 1000);
      matchedFiles.push_back(item);
    }
  }
  return true;};

    utils::file::FileUtils::list_dir(file.path_, lambda, logger_, false);

    if (matchedFiles.size() < 1) {
      logger_->log_debug("No newer files found in directory!");
      return;
    }

    // Sort the list based on modified time
    std::sort(matchedFiles.begin(), matchedFiles.end(), sortTailMatchedFileItem);
    TailMatchedFileItem item = matchedFiles[0];
    logger_->log_info("TailFile File Roll Over from %s to %s", file.current_file_name_, item.fileName);

    // Going ahead in the file rolled over
    if (file.current_file_name_ != base_file_name) {
      logger_->log_debug("Resetting posotion since %s != %s", base_file_name, file.current_file_name_);
      file.currentTailFilePosition_ = 0;
    }

    file.current_file_name_ = item.fileName;

    storeState();
  }
}

void TailFile::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  std::lock_guard<std::mutex> tail_lock(tail_file_mutex_);
  std::string st_file;
  if (context->getProperty(StateFile.getName(), st_file)) {
    state_file_ = st_file + "." + getUUIDStr();
  }
  if (!this->state_recovered_) {
    state_recovered_ = true;
    // recover the state if we have not done so
    this->recoverState();
  }

  /**
   * iterate over file states. may modify them
   */
  for (auto &state : tail_states_) {
    auto fileLocation = state.second.path_;

    logger_->log_debug("Tailing file %s from %llu", fileLocation, state.second.currentTailFilePosition_);
    checkRollOver(state.second, state.first);
    std::string fullPath = fileLocation + utils::file::FileUtils::get_separator() + state.second.current_file_name_;
    struct stat statbuf;

    logger_->log_debug("Tailing file %s from %llu", fullPath, state.second.currentTailFilePosition_);
    if (stat(fullPath.c_str(), &statbuf) == 0) {
      if ((uint64_t) statbuf.st_size <= state.second.currentTailFilePosition_) {
        logger_->log_trace("Current pos: %llu", state.second.currentTailFilePosition_);
        logger_->log_trace("%s", "there are no new input for the current tail file");
        context->yield();
        return;
      }
      std::size_t found = state.first.find_last_of(".");
      std::string baseName = state.first.substr(0, found);
      std::string extension = state.first.substr(found + 1);

      if (!delimiter_.empty()) {
        char delim = delimiter_.c_str()[0];
        if (delim == '\\') {
          if (delimiter_.size() > 1) {
            switch (delimiter_.c_str()[1]) {
              case 'r':
                delim = '\r';
                break;
              case 't':
                delim = '\t';
                break;
              case 'n':
                delim = '\n';
                break;
              case '\\':
                delim = '\\';
                break;
              default:
                // previous behavior
                break;
            }
          }
        }
        logger_->log_debug("Looking for delimiter 0x%X", delim);
        std::vector<std::shared_ptr<FlowFileRecord>> flowFiles;
        session->import(fullPath, flowFiles, state.second.currentTailFilePosition_, delim);
        logger_->log_info("%u flowfiles were received from TailFile input", flowFiles.size());

        for (auto ffr : flowFiles) {
          logger_->log_info("TailFile %s for %u bytes", state.first, ffr->getSize());
          std::string logName = baseName + "." + std::to_string(state.second.currentTailFilePosition_) + "-" + std::to_string(state.second.currentTailFilePosition_ + ffr->getSize()) + "." + extension;
          ffr->updateKeyedAttribute(PATH, fileLocation);
          ffr->addKeyedAttribute(ABSOLUTE_PATH, fullPath);
          ffr->updateKeyedAttribute(FILENAME, logName);
          session->transfer(ffr, Success);
          state.second.currentTailFilePosition_ += ffr->getSize() + 1;
          storeState();
        }

      } else {
        std::shared_ptr<FlowFileRecord> flowFile = std::static_pointer_cast<FlowFileRecord>(session->create());
        if (flowFile) {
          flowFile->updateKeyedAttribute(PATH, fileLocation);
          flowFile->addKeyedAttribute(ABSOLUTE_PATH, fullPath);
          session->import(fullPath, flowFile, true, state.second.currentTailFilePosition_);
          session->transfer(flowFile, Success);
          logger_->log_info("TailFile %s for %llu bytes", state.first, flowFile->getSize());
          std::string logName = baseName + "." + std::to_string(state.second.currentTailFilePosition_) + "-" + std::to_string(state.second.currentTailFilePosition_ + flowFile->getSize()) + "."
              + extension;
          flowFile->updateKeyedAttribute(FILENAME, logName);
          state.second.currentTailFilePosition_ += flowFile->getSize();
          storeState();
        }
      }
      state.second.currentTailFileModificationTime_ = ((uint64_t) (statbuf.st_mtime) * 1000);
    } else {
      logger_->log_warn("Unable to stat file %s", fullPath);
    }
  }
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif
