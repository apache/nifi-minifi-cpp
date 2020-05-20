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
#include <sys/stat.h>
#ifndef WIN32
#include <dirent.h>
#endif
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <algorithm>
#include <string>
#include <iostream>
#include "utils/file/FileUtils.h"
#include "utils/file/PathUtils.h"
#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"
#include "utils/RegexUtils.h"
#include "TailFile.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

#ifndef S_ISDIR
#define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#endif

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property TailFile::FileName("File to Tail", "Fully-qualified filename of the file that should be tailed when using single file mode, or a file regex when using multifile mode", "");

core::Property TailFile::StateFile("State File", "DEPRECATED. Only use it for state migration from the legacy state file.",
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

core::Property TailFile::RollingFilenamePattern(
    core::PropertyBuilder::createProperty("Rolling Filename Pattern")
        ->withDescription("If the file to tail \"rolls over\" as would be the case with log files, this filename pattern will be used to "
        "identify files that have rolled over so MiNiFi can read the remaining of the rolled-over file and then continue with the new log file. "
        "This pattern supports the wildcard characters * and ?, it also supports the notation ${filename} to specify a pattern based on the name of the file "
        "(without extension), and will assume that the files that have rolled over live in the same directory as the file being tailed.")
        ->isRequired(false)
        ->withDefaultValue<std::string>("${filename}.*")
        ->build());

core::Relationship TailFile::Success("success", "All files are routed to success");

const char *TailFile::CURRENT_STR = "CURRENT.";
const char *TailFile::POSITION_STR = "POSITION.";

namespace {
  template<typename Container, typename Key>
  bool containsKey(const Container &container, const Key &key) {
    return container.find(key) != container.end();
  }

  template <typename Container, typename Key>
  uint64_t readOptionalUint64(const Container &container, const Key &key) {
    const auto it = container.find(key);
    if (it != container.end()) {
      return std::stoull(it->second);
    } else {
      return 0;
    }
  }

  // the delimiter is the first character of the input, allowing some escape sequences
  std::string parseDelimiter(const std::string &input) {
    if (!input.empty()) {
      if (input[0] == '\\') {
        if (input.size() > (std::size_t) 1) {
          switch (input[1]) {
            case 'r':
              return "\r";
            case 't':
              return "\t";
            case 'n':
              return "\n";
            case '\\':
              return "\\";
            default:
              return input.substr(1, 1);
          }
        } else {
          return "\\";
        }
      } else {
        return input.substr(0, 1);
      }
    } else {
      return "";
    }
  }

}  // namespace

void TailFile::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(FileName);
  properties.insert(StateFile);
  properties.insert(Delimiter);
  properties.insert(TailMode);
  properties.insert(BaseDirectory);
  properties.insert(RollingFilenamePattern);
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

  state_manager_ = context->getStateManager();
  if (state_manager_ == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }

  std::string value;

  if (context->getProperty(Delimiter.getName(), value)) {
    delimiter_ = parseDelimiter(value);
  }

  if (!context->getProperty(FileName.getName(), file_to_tail_)) {
    throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "File to Tail is a required property");
  }

  std::string mode;
  context->getProperty(TailMode.getName(), mode);

  if (mode == "Multiple file") {
    tail_mode_ = Mode::MULTIPLE;

    if (!context->getProperty(BaseDirectory.getName(), base_dir_)) {
      throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "Base directory is required for multiple tail mode.");
    }

    // in multiple mode, we check for new/removed files in every onTrigger

  } else {
    tail_mode_ = Mode::SINGLE;

    std::string fileLocation, fileName;
    if (utils::file::PathUtils::getFileNameAndPath(file_to_tail_, fileLocation, fileName)) {
      tail_states_.emplace(fileName, TailState{fileLocation, fileName, 0, 0, 0, 0});
    } else {
      throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "File to tail must be a fully qualified file");
    }
  }

  std::string rolling_filename_pattern_glob;
  context->getProperty(RollingFilenamePattern.getName(), rolling_filename_pattern_glob);
  rolling_filename_pattern_ = utils::file::PathUtils::globToRegex(rolling_filename_pattern_glob);
}

void TailFile::parseStateFileLine(char *buf, std::map<std::string, TailState> &state) const {
  char *line = buf;

  logger_->log_trace("Received line %s", buf);

  while ((line[0] == ' ') || (line[0] == '\t'))
    ++line;

  char first = line[0];
  if ((first == '\0') || (first == '#') || (first == '\r') || (first == '\n') || (first == '=')) {
    return;
  }

  char *equal = strchr(line, '=');
  if (equal == nullptr) {
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
  key = utils::StringUtils::trimRight(key);
  value = utils::StringUtils::trimRight(value);

  if (key == "FILENAME") {
    std::string fileLocation, fileName;
    if (utils::file::PathUtils::getFileNameAndPath(value, fileLocation, fileName)) {
      logger_->log_debug("State migration received path %s, file %s", fileLocation, fileName);
      state.insert(std::make_pair(fileName, TailState{fileLocation, fileName, 0, 0, 0, 0}));
    } else {
      state.insert(std::make_pair(value, TailState{fileLocation, value, 0, 0, 0, 0}));
    }
  }
  if (key == "POSITION") {
    // for backwards compatibility
    if (tail_states_.size() != (std::size_t) 1) {
      throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "Incompatible state file types");
    }
    const auto position = std::stoull(value);
    logger_->log_debug("Received position %d", position);
    state.begin()->second.position_ = position;
  }
  if (key.find(CURRENT_STR) == 0) {
    const auto file = key.substr(strlen(CURRENT_STR));
    std::string fileLocation, fileName;
    if (utils::file::PathUtils::getFileNameAndPath(value, fileLocation, fileName)) {
      state[file].path_ = fileLocation;
      state[file].file_name_ = fileName;
    } else {
      throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "State file contains an invalid file name");
    }
  }

  if (key.find(POSITION_STR) == 0) {
    const auto file = key.substr(strlen(POSITION_STR));
    state[file].position_ = std::stoull(value);
  }
}

bool TailFile::recoverState(const std::shared_ptr<core::ProcessContext>& context) {
  std::map<std::string, TailState> new_tail_states;
  bool state_load_success = getStateFromStateManager(new_tail_states) ||
                            getStateFromLegacyStateFile(new_tail_states);
  if (!state_load_success) {
    return false;
  }

  logger_->log_debug("load state succeeded");

  tail_states_ = std::move(new_tail_states);

  // Save the state to the state manager
  storeState(context);

  return true;
}

bool TailFile::getStateFromStateManager(std::map<std::string, TailState> &new_tail_states) const {
  std::unordered_map<std::string, std::string> state_map;
  if (state_manager_->get(state_map)) {
    for (size_t i = 0U;; i++) {
      std::string name;
      try {
        name = state_map.at("file." + std::to_string(i) + ".name");
      } catch (...) {
        break;
      }
      try {
        const std::string& current = state_map.at("file." + std::to_string(i) + ".current");
        uint64_t position = std::stoull(state_map.at("file." + std::to_string(i) + ".position"));
        uint64_t checksum = readOptionalUint64(state_map, "file." + std::to_string(i) + ".checksum");

        std::string fileLocation, fileName;
        if (utils::file::PathUtils::getFileNameAndPath(current, fileLocation, fileName)) {
          logger_->log_debug("Received path %s, file %s", fileLocation, fileName);
          new_tail_states.emplace(fileName, TailState{fileLocation, fileName, position, 0, 0, checksum});
        } else {
          new_tail_states.emplace(current, TailState{fileLocation, current, position, 0, 0, checksum});
        }
      } catch (...) {
        continue;
      }
    }
    for (const auto& s : tail_states_) {
      logger_->log_debug("TailState %s: %s, %s, %llu, %llu, %llu",
                         s.first, s.second.path_, s.second.file_name_, s.second.position_,
                         s.second.mtime_, s.second.checksum_);
    }
    return true;
  } else {
    logger_->log_info("Found no stored state");
  }
  return false;
}

bool TailFile::getStateFromLegacyStateFile(std::map<std::string, TailState> &new_tail_states) const {
  std::ifstream file(state_file_.c_str(), std::ifstream::in);
  if (!file.good()) {
    logger_->log_error("load state file failed %s", state_file_);
    return false;
  }
  char buf[BUFFER_SIZE];
  for (file.getline(buf, BUFFER_SIZE); file.good(); file.getline(buf, BUFFER_SIZE)) {
    parseStateFileLine(buf, new_tail_states);
  }
  return true;
}

bool TailFile::storeState(const std::shared_ptr<core::ProcessContext>& context) {
  std::unordered_map<std::string, std::string> state;
  size_t i = 0;
  for (const auto& tail_state : tail_states_) {
    state["file." + std::to_string(i) + ".name"] = tail_state.first;
    state["file." + std::to_string(i) + ".current"] = utils::file::FileUtils::concat_path(tail_state.second.path_,
                                                                                          tail_state.second.file_name_);
    state["file." + std::to_string(i) + ".position"] = std::to_string(tail_state.second.position_);
    state["file." + std::to_string(i) + ".checksum"] = std::to_string(tail_state.second.checksum_);
    ++i;
  }
  if (!state_manager_->set(state)) {
    logger_->log_error("Failed to set state");
    return false;
  }
  return true;
}

std::vector<TailState> TailFile::findRotatedFiles(const TailState &state) const {
  logger_->log_trace("Searching for files rolled over");

  std::size_t last_dot_position = state.file_name_.find_last_of('.');
  std::string base_name = state.file_name_.substr(0, last_dot_position);
  std::string pattern = utils::file::PathUtils::replacePlaceholderWithBaseName(rolling_filename_pattern_, base_name);

  std::vector<TailState> matched_files;
  auto collect_matching_files = [&](const std::string &path, const std::string &file_name) -> bool {
    if (file_name != state.file_name_ && utils::Regex::matchesFullInput(pattern, file_name)) {
      std::string full_file_name = path + utils::file::FileUtils::get_separator() + file_name;
      uint64_t mtime = utils::file::FileUtils::last_write_time(full_file_name);
      if (mtime >= state.timestamp_ / 1000) {
        matched_files.emplace_back(path, file_name, 0, 0, mtime, 0);
      }
    }
    return true;
  };

  utils::file::FileUtils::list_dir(state.path_, collect_matching_files, logger_, false);

  std::sort(matched_files.begin(), matched_files.end(), [](const TailState &left, const TailState &right) {
    return std::tie(left.mtime_, left.file_name_) <
           std::tie(right.mtime_, right.file_name_);
  });

  if (!matched_files.empty() && state.position_ > 0) {
    TailState &first_rotated_file = matched_files[0];
    std::string full_file_name = first_rotated_file.fileNameWithPath();
    if (utils::file::FileUtils::file_size(full_file_name) >= state.position_) {
      uint64_t checksum = utils::file::FileUtils::computeChecksum(full_file_name, state.position_);
      if (checksum == state.checksum_) {
        first_rotated_file.position_ = state.position_;
        first_rotated_file.checksum_ = state.checksum_;
      }
    }
  }

  return matched_files;
}

void TailFile::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  std::lock_guard<std::mutex> tail_lock(tail_file_mutex_);
  std::string st_file;
  if (context->getProperty(StateFile.getName(), st_file)) {
    state_file_ = st_file + "." + getUUIDStr();
  }
  if (!this->state_recovered_) {
    this->recoverState(context);
    state_recovered_ = true;
  }

  if (tail_mode_ == Mode::MULTIPLE) {
    checkForRemovedFiles();
    checkForNewFiles();
  }

  bool did_something = false;

  // iterate over file states. may modify them
  for (auto &state : tail_states_) {
    did_something |= processFile(context, session, state.first, state.second);
  }

  if (!did_something) {
    yield();
  }
}

  bool TailFile::processFile(const std::shared_ptr<core::ProcessContext> &context,
                             const std::shared_ptr<core::ProcessSession> &session,
                             const std::string &fileName,
                             TailState &state) {
    std::string full_file_name = state.fileNameWithPath();

    bool did_something = false;

    if (utils::file::FileUtils::file_size(full_file_name) < state.position_) {
      std::vector<TailState> rotated_file_states = findRotatedFiles(state);
      for (TailState &file_state : rotated_file_states) {
        did_something |= processSingleFile(context, session, file_state.file_name_, file_state);
      }
      state.position_ = 0;
      state.checksum_ = 0;
    }

    did_something |= processSingleFile(context, session, fileName, state);
    return did_something;
  }

  bool TailFile::processSingleFile(const std::shared_ptr<core::ProcessContext> &context,
                                   const std::shared_ptr<core::ProcessSession> &session,
                                   const std::string &fileName,
                                   TailState &state) {
    std::string full_file_name = state.fileNameWithPath();

    if (utils::file::FileUtils::file_size(full_file_name) == 0u) {
      logger_->log_warn("Unable to read file %s as it does not exist or has size zero", full_file_name);
      return false;
    }
    logger_->log_debug("Tailing file %s from %llu", full_file_name, state.position_);

  std::size_t last_dot_position = fileName.find_last_of('.');
  std::string baseName = fileName.substr(0, last_dot_position);
  std::string extension = fileName.substr(last_dot_position + 1);

  if (!delimiter_.empty()) {
    char delim = delimiter_[0];
    logger_->log_trace("Looking for delimiter 0x%X", delim);

    std::vector<std::shared_ptr<FlowFileRecord>> flowFiles;
    uint64_t checksum = state.checksum_;
    session->import(full_file_name, flowFiles, state.position_, delim, &checksum);
    logger_->log_info("%u flowfiles were received from TailFile input", flowFiles.size());

    for (auto &ffr : flowFiles) {
      logger_->log_info("TailFile %s for %u bytes", fileName, ffr->getSize());
      std::string logName = baseName + "." + std::to_string(state.position_) + "-" +
                            std::to_string(state.position_ + ffr->getSize()) + "." + extension;
      ffr->updateKeyedAttribute(PATH, state.path_);
      ffr->addKeyedAttribute(ABSOLUTE_PATH, full_file_name);
      ffr->updateKeyedAttribute(FILENAME, logName);
      session->transfer(ffr, Success);
      state.position_ += ffr->getSize() + 1;
      state.timestamp_ = getTimeMillis();
      state.checksum_ = checksum;
      storeState(context);
    }

    if (!flowFiles.empty()) {
      return true;
    }
  } else {
    std::shared_ptr<FlowFileRecord> flowFile = std::static_pointer_cast<FlowFileRecord>(session->create());
    if (flowFile) {
      flowFile->updateKeyedAttribute(PATH, state.path_);
      flowFile->addKeyedAttribute(ABSOLUTE_PATH, full_file_name);
      uint64_t checksum = state.checksum_;
      session->import(full_file_name, flowFile, true, state.position_, &checksum);
      session->transfer(flowFile, Success);
      logger_->log_info("TailFile %s for %llu bytes", fileName, flowFile->getSize());
      std::string logName = baseName + "." + std::to_string(state.position_) + "-" +
                            std::to_string(state.position_ + flowFile->getSize()) + "."
                            + extension;
      flowFile->updateKeyedAttribute(FILENAME, logName);
      state.position_ += flowFile->getSize();
      state.timestamp_ = getTimeMillis();
      state.checksum_ = checksum;
      storeState(context);
      return true;
    }
  }

    return false;
  }

void TailFile::checkForRemovedFiles() {
    std::vector<std::string> nonexistentFiles;
    for (const auto &kv : tail_states_) {
      const std::string &fileName = kv.first;
      const TailState &state = kv.second;
      if (utils::file::FileUtils::file_size(state.fileNameWithPath()) == 0u) {
        nonexistentFiles.push_back(fileName);
      }
    }

    for (const auto &fileName : nonexistentFiles) {
      tail_states_.erase(fileName);
    }
  }

  void TailFile::checkForNewFiles() {
    auto fileRegexSelect = [&](const std::string &path, const std::string &filename) -> bool {
      if (!containsKey(tail_states_, filename) && utils::Regex::matchesFullInput(file_to_tail_, filename)) {
        tail_states_.emplace(filename, TailState{path, filename, 0, 0, 0, 0});
      }
      return true;
    };

    utils::file::FileUtils::list_dir(base_dir_, fileRegexSelect, logger_, false);
  }
} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
