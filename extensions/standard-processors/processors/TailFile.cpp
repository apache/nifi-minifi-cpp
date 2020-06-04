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

#include <algorithm>
#include <iostream>
#include <queue>
#include <map>
#include <unordered_map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

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

core::Property TailFile::FileName(
    core::PropertyBuilder::createProperty("File to Tail")
        ->withDescription("Fully-qualified filename of the file that should be tailed when using single file mode, or a file regex when using multifile mode")
        ->isRequired(true)
        ->build());

core::Property TailFile::StateFile(
    core::PropertyBuilder::createProperty("State File")
        ->withDescription("DEPRECATED. Only use it for state migration from the legacy state file.")
        ->isRequired(false)
        ->withDefaultValue<std::string>("TailFileState")
        ->build());

core::Property TailFile::Delimiter(
    core::PropertyBuilder::createProperty("Input Delimiter")
        ->withDescription("Specifies the character that should be used for delimiting the data being tailed"
         "from the incoming file. If none is specified, data will be ingested as it becomes available.")
        ->isRequired(false)
        ->withDefaultValue<std::string>("\\n")
        ->build());

core::Property TailFile::TailMode(
    core::PropertyBuilder::createProperty("tail-mode", "Tailing Mode")
        ->withDescription("Specifies the tail file mode. In 'Single file' mode only a single file will be watched. "
        "In 'Multiple file' mode a regex may be used. Note that in multiple file mode we will still continue to watch for rollover on the initial set of watched files. "
        "The Regex used to locate multiple files will be run during the schedule phrase. Note that if rotated files are matched by the regex, those files will be tailed.")->isRequired(true)
        ->withAllowableValue<std::string>("Single file")->withAllowableValue("Multiple file")->withDefaultValue("Single file")
        ->build());

core::Property TailFile::BaseDirectory(
    core::PropertyBuilder::createProperty("tail-base-directory", "Base Directory")
        ->withDescription("Base directory used to look for files to tail. This property is required when using Multiple file mode.")
        ->isRequired(false)
        ->build());

core::Property TailFile::RecursiveLookup(
    core::PropertyBuilder::createProperty("Recursive lookup")
        ->withDescription("When using Multiple file mode, this property determines whether files are tailed in "
        "child directories of the Base Directory or not.")
        ->isRequired(false)
        ->withDefaultValue<bool>(false)
        ->build());

core::Property TailFile::LookupFrequency(
    core::PropertyBuilder::createProperty("Lookup frequency")
        ->withDescription("When using Multiple file mode, this property specifies the minimum duration "
        "the processor will wait between looking for new files to tail in the Base Directory.")
        ->isRequired(false)
        ->withDefaultValue<core::TimePeriodValue>("10 min")
        ->build());

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
  if (input.empty()) return "";
  if (input[0] != '\\') return std::string{ input[0] };
  if (input.size() == std::size_t{1}) return "\\";
  switch (input[1]) {
    case 'r': return "\r";
    case 't': return "\t";
    case 'n': return "\n";
    default: return std::string{ input[1] };
  }
}

std::map<std::string, TailState> update_keys_in_legacy_states(const std::map<std::string, TailState> &legacy_tail_states) {
  std::map<std::string, TailState> new_tail_states;
  for (const auto &key_value_pair : legacy_tail_states) {
    const TailState &state = key_value_pair.second;
    std::string full_file_name = utils::file::FileUtils::concat_path(state.path_, state.file_name_);
    new_tail_states.emplace(full_file_name, state);
  }
  return new_tail_states;
}

struct TailStateWithMtime {
  using TimePoint = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>;

  TailStateWithMtime(TailState tail_state, TimePoint mtime)
    : tail_state_(std::move(tail_state)), mtime_(mtime) {}

  TailState tail_state_;
  TimePoint mtime_;
};
}  // namespace

void TailFile::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(FileName);
  properties.insert(StateFile);
  properties.insert(Delimiter);
  properties.insert(TailMode);
  properties.insert(BaseDirectory);
  properties.insert(RecursiveLookup);
  properties.insert(LookupFrequency);
  properties.insert(RollingFilenamePattern);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void TailFile::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  std::lock_guard<std::mutex> tail_lock(tail_file_mutex_);

  tail_states_.clear();

  state_manager_ = context->getStateManager();
  if (state_manager_ == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }

  std::string value;

  if (context->getProperty(Delimiter.getName(), value)) {
    delimiter_ = parseDelimiter(value);
  }

  context->getProperty(FileName.getName(), file_to_tail_);

  std::string mode;
  context->getProperty(TailMode.getName(), mode);

  if (mode == "Multiple file") {
    tail_mode_ = Mode::MULTIPLE;

    if (!context->getProperty(BaseDirectory.getName(), base_dir_)) {
      throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "Base directory is required for multiple tail mode.");
    }

    if (utils::file::FileUtils::is_directory(base_dir_.c_str()) == 0) {
      throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "Base directory does not exist or is not a directory");
    }

    context->getProperty(RecursiveLookup.getName(), recursive_lookup_);

    context->getProperty(LookupFrequency.getName(), lookup_frequency_);

    // in multiple mode, we check for new/removed files in every onTrigger

  } else {
    tail_mode_ = Mode::SINGLE;

    std::string path, file_name;
    if (utils::file::PathUtils::getFileNameAndPath(file_to_tail_, path, file_name)) {
      // NOTE: position and checksum will be updated in recoverState() if there is a persisted state for this file
      tail_states_.emplace(file_to_tail_, TailState{path, file_name});
    } else {
      throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "File to tail must be a fully qualified file");
    }
  }

  std::string rolling_filename_pattern_glob;
  context->getProperty(RollingFilenamePattern.getName(), rolling_filename_pattern_glob);
  rolling_filename_pattern_ = utils::file::PathUtils::globToRegex(rolling_filename_pattern_glob);

  recoverState(context);
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
      state.emplace(fileName, TailState{fileLocation, fileName});
    } else {
      state.emplace(value, TailState{fileLocation, value});
    }
  }
  if (key == "POSITION") {
    // for backwards compatibility
    if (tail_states_.size() != std::size_t{1}) {
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
                            getStateFromLegacyStateFile(context, new_tail_states);
  if (!state_load_success) {
    return false;
  }

  logger_->log_debug("load state succeeded");

  if (tail_mode_ == Mode::SINGLE) {
    if (tail_states_.size() == 1) {
      auto state_it = tail_states_.begin();
      const auto it = new_tail_states.find(state_it->first);
      if (it != new_tail_states.end()) {
        state_it->second = it->second;
      }
    } else {
      throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "This should never happen: "
          "in Single file mode, internal state size should be 1, but it is " + std::to_string(tail_states_.size()));
    }
  } else {
    tail_states_ = std::move(new_tail_states);
  }

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
          new_tail_states.emplace(current, TailState{fileLocation, fileName, position, std::chrono::system_clock::time_point{}, checksum});
        } else {
          new_tail_states.emplace(current, TailState{fileLocation, current, position, std::chrono::system_clock::time_point{}, checksum});
        }
      } catch (...) {
        continue;
      }
    }
    for (const auto& s : tail_states_) {
      logger_->log_debug("TailState %s: %s, %s, %llu, %llu",
                         s.first, s.second.path_, s.second.file_name_, s.second.position_, s.second.checksum_);
    }
    return true;
  } else {
    logger_->log_info("Found no stored state");
  }
  return false;
}

bool TailFile::getStateFromLegacyStateFile(const std::shared_ptr<core::ProcessContext>& context,
                                           std::map<std::string, TailState> &new_tail_states) const {
  std::string state_file_name_property;
  context->getProperty(StateFile.getName(), state_file_name_property);
  std::string state_file = state_file_name_property + "." + getUUIDStr();

  std::ifstream file(state_file.c_str(), std::ifstream::in);
  if (!file.good()) {
    logger_->log_info("Legacy state file %s not found (this is OK)", state_file);
    return false;
  }

  std::map<std::string, TailState> legacy_tail_states;
  char buf[BUFFER_SIZE];
  for (file.getline(buf, BUFFER_SIZE); file.good(); file.getline(buf, BUFFER_SIZE)) {
    parseStateFileLine(buf, legacy_tail_states);
  }

  new_tail_states = update_keys_in_legacy_states(legacy_tail_states);
  return true;
}

bool TailFile::storeState(const std::shared_ptr<core::ProcessContext>& context) {
  std::unordered_map<std::string, std::string> state;
  size_t i = 0;
  for (const auto& tail_state : tail_states_) {
    state["file." + std::to_string(i) + ".current"] = tail_state.first;
    state["file." + std::to_string(i) + ".name"] = tail_state.second.file_name_;
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
  logger_->log_debug("Searching for files rolled over; last read time is %llu",
      std::chrono::time_point_cast<std::chrono::seconds>(state.last_read_time_));

  std::size_t last_dot_position = state.file_name_.find_last_of('.');
  std::string base_name = state.file_name_.substr(0, last_dot_position);
  std::string pattern = utils::StringUtils::replaceOne(rolling_filename_pattern_, "${filename}", base_name);

  std::vector<TailStateWithMtime> matched_files_with_mtime;
  auto collect_matching_files = [&](const std::string &path, const std::string &file_name) -> bool {
    if (file_name != state.file_name_ && utils::Regex::matchesFullInput(pattern, file_name)) {
      std::string full_file_name = path + utils::file::FileUtils::get_separator() + file_name;
      TailStateWithMtime::TimePoint mtime{utils::file::FileUtils::last_write_time_point(full_file_name)};
      logger_->log_debug("File %s with mtime %llu matches rolling filename pattern %s", file_name, mtime.time_since_epoch().count(), pattern);
      if (mtime >= std::chrono::time_point_cast<std::chrono::seconds>(state.last_read_time_)) {
        logger_->log_debug("File %s has mtime >= last read time, so we are going to read it", file_name);
        matched_files_with_mtime.emplace_back(TailState{path, file_name}, mtime);
      }
    }
    return true;
  };

  utils::file::FileUtils::list_dir(state.path_, collect_matching_files, logger_, false);

  std::sort(matched_files_with_mtime.begin(), matched_files_with_mtime.end(), [](const TailStateWithMtime &left, const TailStateWithMtime &right) {
    return std::tie(left.mtime_, left.tail_state_.file_name_) <
           std::tie(right.mtime_, right.tail_state_.file_name_);
  });

  if (!matched_files_with_mtime.empty() && state.position_ > 0) {
    TailState &first_rotated_file = matched_files_with_mtime[0].tail_state_;
    std::string full_file_name = first_rotated_file.fileNameWithPath();
    if (utils::file::FileUtils::file_size(full_file_name) >= state.position_) {
      uint64_t checksum = utils::file::FileUtils::computeChecksum(full_file_name, state.position_);
      if (checksum == state.checksum_) {
        first_rotated_file.position_ = state.position_;
        first_rotated_file.checksum_ = state.checksum_;
      }
    }
  }

  std::vector<TailState> matched_files;
  std::transform(matched_files_with_mtime.begin(), matched_files_with_mtime.end(), std::back_inserter(matched_files),
                 [](TailStateWithMtime &tail_state_with_mtime) { return std::move(tail_state_with_mtime.tail_state_); });
  return matched_files;
}

void TailFile::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  std::lock_guard<std::mutex> tail_lock(tail_file_mutex_);

  if (tail_mode_ == Mode::MULTIPLE) {
    if (last_multifile_lookup_ + lookup_frequency_ < std::chrono::steady_clock::now()) {
      logger_->log_debug("Lookup frequency %d ms have elapsed, doing new multifile lookup", lookup_frequency_.count());
      checkForRemovedFiles();
      checkForNewFiles();
      last_multifile_lookup_ = std::chrono::steady_clock::now();
    } else {
      logger_->log_trace("Skipping multifile lookup");
    }
  }

  // iterate over file states. may modify them
  for (auto &state : tail_states_) {
    processFile(context, session, state.first, state.second);
  }

  if (!session->existsFlowFileInRelationship(Success)) {
    yield();
  }
}

void TailFile::processFile(const std::shared_ptr<core::ProcessContext> &context,
                           const std::shared_ptr<core::ProcessSession> &session,
                           const std::string &full_file_name,
                           TailState &state) {
  if (utils::file::FileUtils::file_size(full_file_name) < state.position_) {
    processRotatedFiles(context, session, state);
  }

  processSingleFile(context, session, full_file_name, state);
}

void TailFile::processRotatedFiles(const std::shared_ptr<core::ProcessContext> &context,
                                   const std::shared_ptr<core::ProcessSession> &session,
                                   TailState &state) {
    std::vector<TailState> rotated_file_states = findRotatedFiles(state);
    for (TailState &file_state : rotated_file_states) {
      processSingleFile(context, session, file_state.fileNameWithPath(), file_state);
    }
    state.position_ = 0;
    state.checksum_ = 0;
}

void TailFile::processSingleFile(const std::shared_ptr<core::ProcessContext> &context,
                                 const std::shared_ptr<core::ProcessSession> &session,
                                 const std::string &full_file_name,
                                 TailState &state) {
  std::string fileName = state.file_name_;

  if (utils::file::FileUtils::file_size(full_file_name) == 0u) {
    logger_->log_warn("Unable to read file %s as it does not exist or has size zero", full_file_name);
    return;
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
    session->import(full_file_name, flowFiles, state.position_, delim, true, &checksum);
    logger_->log_info("%u flowfiles were received from TailFile input", flowFiles.size());

    for (auto &ffr : flowFiles) {
      logger_->log_info("TailFile %s for %u bytes", fileName, ffr->getSize());
      std::string logName = baseName + "." + std::to_string(state.position_) + "-" +
                            std::to_string(state.position_ + ffr->getSize() - 1) + "." + extension;
      ffr->updateKeyedAttribute(PATH, state.path_);
      ffr->addKeyedAttribute(ABSOLUTE_PATH, full_file_name);
      ffr->updateKeyedAttribute(FILENAME, logName);
      session->transfer(ffr, Success);
      state.position_ += ffr->getSize();
      state.last_read_time_ = std::chrono::system_clock::now();
      state.checksum_ = checksum;
      storeState(context);
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
      state.last_read_time_ = std::chrono::system_clock::now();
      state.checksum_ = checksum;
      storeState(context);
    }
  }
}

void TailFile::checkForRemovedFiles() {
  std::vector<std::string> file_names_to_remove;

  for (const auto &kv : tail_states_) {
    const std::string &full_file_name = kv.first;
    const TailState &state = kv.second;
    if (utils::file::FileUtils::file_size(state.fileNameWithPath()) == 0u ||
        !utils::Regex::matchesFullInput(file_to_tail_, state.file_name_)) {
      file_names_to_remove.push_back(full_file_name);
    }
  }

  for (const auto &full_file_name : file_names_to_remove) {
    tail_states_.erase(full_file_name);
  }
}

void TailFile::checkForNewFiles() {
  auto add_new_files_callback = [&](const std::string &path, const std::string &file_name) -> bool {
    std::string full_file_name = path + utils::file::FileUtils::get_separator() + file_name;
    if (!containsKey(tail_states_, full_file_name) && utils::Regex::matchesFullInput(file_to_tail_, file_name)) {
      tail_states_.emplace(full_file_name, TailState{path, file_name});
    }
    return true;
  };

  utils::file::FileUtils::list_dir(base_dir_, add_new_files_callback, logger_, recursive_lookup_);
}

std::chrono::milliseconds TailFile::getLookupFrequency() const {
  return lookup_frequency_;
}

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
