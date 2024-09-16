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

#include <algorithm>
#include <array>
#include <iostream>
#include <map>
#include <unordered_map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "range/v3/action/sort.hpp"

#include "io/CRCStream.h"
#include "utils/file/FileUtils.h"
#include "utils/file/PathUtils.h"
#include "utils/StringUtils.h"
#include "utils/ProcessorConfigUtils.h"
#include "TextFragmentUtils.h"
#include "TailFile.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::processors {

const char *TailFile::CURRENT_STR = "CURRENT.";
const char *TailFile::POSITION_STR = "POSITION.";

namespace {
template<typename Container, typename Key>
bool containsKey(const Container &container, const Key &key) {
  return container.find(key) != container.end();
}

template <typename Container, typename Key>
int64_t readOptionalInt64(const Container &container, const Key &key) {
  const auto it = container.find(key);
  if (it != container.end()) {
    return std::stoll(it->second);
  } else {
    return 0;
  }
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

std::map<std::filesystem::path, TailState> update_keys_in_legacy_states(const std::map<std::filesystem::path, TailState> &legacy_tail_states) {
  std::map<std::filesystem::path, TailState> new_tail_states;
  for (const auto &key_value_pair : legacy_tail_states) {
    const TailState &state = key_value_pair.second;
    new_tail_states.emplace(state.path_ / state.file_name_, state);
  }
  return new_tail_states;
}

void openFile(const std::filesystem::path& file_path, uint64_t offset, std::ifstream &input_stream, const std::shared_ptr<core::logging::Logger> &logger) {
  logger->log_debug("Opening {}", file_path);
  input_stream.open(file_path, std::fstream::in | std::fstream::binary);
  if (!input_stream.is_open() || !input_stream.good()) {
    input_stream.close();
    throw Exception(FILE_OPERATION_EXCEPTION, "Could not open file: " + file_path.string());
  }
  if (offset != 0U) {
    input_stream.seekg(gsl::narrow<std::ifstream::off_type>(offset), std::ifstream::beg);
    if (!input_stream.good()) {
      logger->log_error("Seeking to {} failed for file {} (does file/filesystem support seeking?)", offset, file_path);
      throw Exception(FILE_OPERATION_EXCEPTION, "Could not seek file " + file_path.string() + " to offset " + std::to_string(offset));
    }
  }
}

constexpr std::size_t BUFFER_SIZE = 4096;

class FileReaderCallback {
 public:
  FileReaderCallback(const std::filesystem::path& file_path,
                     uint64_t offset,
                     char input_delimiter,
                     uint64_t checksum)
    : input_delimiter_(input_delimiter),
      checksum_(checksum) {
    openFile(file_path, offset, input_stream_, logger_);
  }

  int64_t operator()(const std::shared_ptr<io::OutputStream>& output_stream) {
    io::CRCStream<io::OutputStream> crc_stream{gsl::make_not_null(output_stream.get()), checksum_};

    uint64_t num_bytes_written = 0;
    bool found_delimiter = false;

    while (hasMoreToRead() && !found_delimiter) {
      if (begin_ == end_) {
        input_stream_.read(reinterpret_cast<char *>(buffer_.data()), gsl::narrow<std::streamsize>(buffer_.size()));

        const auto num_bytes_read = input_stream_.gcount();
        logger_->log_trace("Read {} bytes of input", std::intmax_t{num_bytes_read});

        begin_ = buffer_.data();
        end_ = begin_ + num_bytes_read;
      }

      char *delimiter_pos = std::find(begin_, end_, input_delimiter_);
      found_delimiter = (delimiter_pos != end_);

      const auto zlen = gsl::narrow<size_t>(std::distance(begin_, delimiter_pos)) + (found_delimiter ? 1 : 0);
      crc_stream.write(reinterpret_cast<uint8_t*>(begin_), zlen);
      num_bytes_written += zlen;
      begin_ += zlen;
    }

    if (found_delimiter) {
      checksum_ = crc_stream.getCRC();
    } else {
      latest_flow_file_ends_with_delimiter_ = false;
    }

    return gsl::narrow<int64_t>(num_bytes_written);
  }

  uint64_t checksum() const {
    return checksum_;
  }

  bool hasMoreToRead() const {
    return begin_ != end_ || input_stream_.good();
  }

  bool useLatestFlowFile() const {
    return latest_flow_file_ends_with_delimiter_;
  }

 private:
  char input_delimiter_;
  uint64_t checksum_;
  std::ifstream input_stream_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<TailFile>::getLogger();

  std::array<char, BUFFER_SIZE> buffer_{};
  char *begin_ = buffer_.data();
  char *end_ = buffer_.data();

  bool latest_flow_file_ends_with_delimiter_ = true;
};

class WholeFileReaderCallback {
 public:
  WholeFileReaderCallback(const std::filesystem::path& file_path,
                          uint64_t offset,
                          uint64_t checksum)
    : checksum_(checksum) {
    openFile(file_path, offset, input_stream_, logger_);
  }

  uint64_t checksum() const {
    return checksum_;
  }

  int64_t operator()(const std::shared_ptr<io::OutputStream>& output_stream) {
    std::array<char, BUFFER_SIZE> buffer{};

    io::CRCStream<io::OutputStream> crc_stream{gsl::make_not_null(output_stream.get()), checksum_};

    uint64_t num_bytes_written = 0;

    while (input_stream_.good()) {
      input_stream_.read(buffer.data(), buffer.size());

      const auto num_bytes_read = input_stream_.gcount();
      logger_->log_trace("Read {} bytes of input", std::intmax_t{num_bytes_read});

      const int len = gsl::narrow<int>(num_bytes_read);

      crc_stream.write(reinterpret_cast<uint8_t*>(buffer.data()), len);
      num_bytes_written += len;
    }

    checksum_ = crc_stream.getCRC();

    return gsl::narrow<int64_t>(num_bytes_written);
  }

 private:
  uint64_t checksum_;
  std::ifstream input_stream_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<TailFile>::getLogger();
};

// This is for backwards compatibility only, as it will accept any string as Input Delimiter while only use the first character from it, which can be confusing
std::optional<char> getDelimiterOld(const std::string& delimiter_str) {
  if (delimiter_str.empty()) return std::nullopt;
  if (delimiter_str[0] != '\\') return delimiter_str[0];
  if (delimiter_str.size() == 1) return '\\';
  switch (delimiter_str[1]) {
    case 'r': return '\r';
    case 't': return '\t';
    case 'n': return '\n';
    default: return delimiter_str[1];
  }
}
}  // namespace

void TailFile::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void TailFile::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  tail_states_.clear();

  state_manager_ = context.getStateManager();
  if (state_manager_ == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }

  if (auto delimiter_str = context.getProperty(Delimiter)) {
    if (auto parsed_delimiter = utils::string::parseCharacter(*delimiter_str)) {
      delimiter_ = *parsed_delimiter;
    } else {
      logger_->log_error("Invalid {}: \"{}\" (it should be a single character, whether escaped or not). Using the first character as the {}",
          TailFile::Delimiter.name, *delimiter_str, TailFile::Delimiter.name);
      delimiter_ = getDelimiterOld(*delimiter_str);
    }
  }

  std::string file_name_str;
  context.getProperty(FileName, file_name_str);

  std::string mode;
  context.getProperty(TailMode, mode);

  if (mode == "Multiple file") {
    tail_mode_ = Mode::MULTIPLE;
    pattern_regex_ = utils::Regex(file_name_str);

    parseAttributeProviderServiceProperty(context);

    if (auto base_dir = context.getProperty(BaseDirectory); !base_dir) {
      throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "Base directory is required for multiple tail mode.");
    } else {
      base_dir_ = std::filesystem::path(*base_dir);
    }

    if (!attribute_provider_service_ && !utils::file::is_directory(base_dir_)) {
      throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "Base directory does not exist or is not a directory");
    }

    context.getProperty(RecursiveLookup, recursive_lookup_);

    if (auto lookup_frequency = context.getProperty<core::TimePeriodValue>(LookupFrequency)) {
      lookup_frequency_ = lookup_frequency->getMilliseconds();
    }

    recoverState(context);

    doMultifileLookup(context);

  } else {
    tail_mode_ = Mode::SINGLE;
    auto file_to_tail = std::filesystem::path(file_name_str);

    if (file_to_tail.has_filename() && file_to_tail.has_parent_path()) {
      // NOTE: position and checksum will be updated in recoverState() if there is a persisted state for this file
      tail_states_.emplace(file_to_tail, TailState{ file_to_tail.parent_path(), file_to_tail.filename()});
    } else {
      throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "File to tail must be a fully qualified file");
    }

    recoverState(context);
  }

  std::string rolling_filename_pattern_glob;
  context.getProperty(RollingFilenamePattern, rolling_filename_pattern_glob);
  rolling_filename_pattern_ = utils::file::globToRegex(rolling_filename_pattern_glob);
  initial_start_position_ = utils::parseEnumProperty<InitialStartPositions>(context, InitialStartPosition);

  uint32_t batch_size = 0;
  if (context.getProperty(BatchSize, batch_size) && batch_size != 0) {
    batch_size_ = batch_size;
  }
}

void TailFile::parseAttributeProviderServiceProperty(core::ProcessContext& context) {
  const auto attribute_provider_service_name = context.getProperty(AttributeProviderService);
  if (!attribute_provider_service_name || attribute_provider_service_name->empty()) {
    return;
  }

  std::shared_ptr<core::controller::ControllerService> controller_service = context.getControllerService(*attribute_provider_service_name);
  if (!controller_service) {
    throw minifi::Exception{ExceptionType::PROCESS_SCHEDULE_EXCEPTION, utils::string::join_pack("Controller service '", *attribute_provider_service_name, "' not found")};
  }

  // we drop ownership of the service here -- in the long term, getControllerService() should return a non-owning pointer or optional reference
  attribute_provider_service_ = dynamic_cast<minifi::controllers::AttributeProviderService*>(controller_service.get());
  if (!attribute_provider_service_) {
    throw minifi::Exception{ExceptionType::PROCESS_SCHEDULE_EXCEPTION, utils::string::join_pack("Controller service '", *attribute_provider_service_name, "' is not an AttributeProviderService")};
  }
}

void TailFile::parseStateFileLine(char *buf, std::map<std::filesystem::path, TailState> &state) const {
  char *line = buf;

  logger_->log_trace("Received line {}", buf);

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
  key = utils::string::trimRight(key);
  value = utils::string::trimRight(value);

  if (key == "FILENAME") {
    std::filesystem::path file_path = value;
    if (file_path.has_filename() && file_path.has_parent_path()) {
      logger_->log_debug("State migration received path {}, file {}", file_path.parent_path(), file_path.filename());
      state.emplace(file_path.filename(), TailState{file_path.parent_path(), file_path.filename()});
    } else {
      state.emplace(value, TailState{file_path.parent_path(), value});
    }
  }
  if (key == "POSITION") {
    // for backwards compatibility
    if (tail_states_.size() != std::size_t{1}) {
      throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "Incompatible state file types");
    }
    const auto position = std::stoull(value);
    logger_->log_debug("Received position {}", position);
    state.begin()->second.position_ = gsl::narrow<uint64_t>(position);
  }
  if (key.find(CURRENT_STR) == 0) {
    const auto file = key.substr(strlen(CURRENT_STR));
    std::filesystem::path file_path = value;
    if (file_path.has_filename() && file_path.has_parent_path()) {
      state[file].path_ = file_path.parent_path();
      state[file].file_name_ = file_path.filename();
    } else {
      throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "State file contains an invalid file name");
    }
  }

  if (key.find(POSITION_STR) == 0) {
    const auto file = key.substr(strlen(POSITION_STR));
    state[file].position_ = std::stoull(value);
  }
}

bool TailFile::recoverState(core::ProcessContext& context) {
  std::map<std::filesystem::path, TailState> new_tail_states;
  bool state_load_success = getStateFromStateManager(new_tail_states) ||
                            getStateFromLegacyStateFile(context, new_tail_states);
  if (!state_load_success) {
    return false;
  }

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

  logState();
  storeState();

  return true;
}

bool TailFile::getStateFromStateManager(std::map<std::filesystem::path, TailState> &new_tail_states) const {
  std::unordered_map<std::string, std::string> state_map;
  if (state_manager_->get(state_map)) {
    for (size_t i = 0U;; ++i) {
      if (state_map.find("file." + std::to_string(i) + ".name") == state_map.end()) {
        break;
      }
      try {
        const std::string& current = state_map.at("file." + std::to_string(i) + ".current");
        uint64_t position = std::stoull(state_map.at("file." + std::to_string(i) + ".position"));
        uint64_t checksum = readOptionalUint64(state_map, "file." + std::to_string(i) + ".checksum");
        std::chrono::file_clock::time_point last_read_time{std::chrono::milliseconds{
            readOptionalInt64(state_map, "file." + std::to_string(i) + ".last_read_time")
        }};

        std::filesystem::path file_path = current;
        if (file_path.has_filename() && file_path.has_parent_path()) {
          logger_->log_debug("Received path {}, file {}", file_path.parent_path(), file_path.filename());
          new_tail_states.emplace(current, TailState{file_path.parent_path(), file_path.filename(), position, last_read_time, checksum});
        } else {
          new_tail_states.emplace(current, TailState{file_path.parent_path(), file_path, position, last_read_time, checksum});
        }
      } catch (...) {
        continue;
      }
    }
    for (const auto& s : tail_states_) {
      logger_->log_debug("TailState {}: {}, {}, {}, {}",
                         s.first.string(), s.second.path_, s.second.file_name_, s.second.position_, s.second.checksum_);
    }
    return true;
  } else {
    logger_->log_info("Found no stored state");
  }
  return false;
}

bool TailFile::getStateFromLegacyStateFile(core::ProcessContext& context,
                                           std::map<std::filesystem::path, TailState> &new_tail_states) const {
  std::string state_file_name_property;
  context.getProperty(StateFile, state_file_name_property);
  std::string state_file = state_file_name_property + "." + getUUIDStr();

  std::ifstream file(state_file.c_str(), std::ifstream::in);
  if (!file.good()) {
    logger_->log_info("Legacy state file {} not found (this is OK)", state_file);
    return false;
  }

  std::map<std::filesystem::path, TailState> legacy_tail_states;
  std::array<char, BUFFER_SIZE> buf{};
  for (file.getline(buf.data(), BUFFER_SIZE); file.good(); file.getline(buf.data(), BUFFER_SIZE)) {
    parseStateFileLine(buf.data(), legacy_tail_states);
  }

  new_tail_states = update_keys_in_legacy_states(legacy_tail_states);
  return true;
}

void TailFile::logState() {
  logger_->log_info("State of the TailFile processor {}:", name_);
  for (const auto& [key, value] : tail_states_) {
    logger_->log_info("key => {{{}}}", value);
  }
}

std::ostream& operator<<(std::ostream &os, const TailState &tail_state) {
  os << "name: " << tail_state.file_name_
      << ", position: " << tail_state.position_
      << ", checksum: " << tail_state.checksum_
      << ", last_read_time: " << tail_state.lastReadTimeInMilliseconds();
  return os;
}

bool TailFile::storeState() {
  std::unordered_map<std::string, std::string> state;
  size_t i = 0;
  for (const auto& tail_state : tail_states_) {
    state["file." + std::to_string(i) + ".current"] = tail_state.first.string();
    state["file." + std::to_string(i) + ".name"] = tail_state.second.file_name_.string();
    state["file." + std::to_string(i) + ".position"] = std::to_string(tail_state.second.position_);
    state["file." + std::to_string(i) + ".checksum"] = std::to_string(tail_state.second.checksum_);
    state["file." + std::to_string(i) + ".last_read_time"] = std::to_string(tail_state.second.lastReadTimeInMilliseconds());
    ++i;
  }
  if (!state_manager_->set(state)) {
    logger_->log_error("Failed to set state");
    return false;
  }
  return true;
}

std::string TailFile::parseRollingFilePattern(const TailState &state) const {
  std::size_t last_dot_position = state.file_name_.string().find_last_of('.');
  std::string base_name = state.file_name_.string().substr(0, last_dot_position);
  return utils::string::replaceOne(rolling_filename_pattern_, "${filename}", base_name);
}

std::vector<TailState> TailFile::findAllRotatedFiles(const TailState &state) const {
  logger_->log_debug("Searching for all files rolled over");

  std::string pattern = parseRollingFilePattern(state);

  std::vector<TailStateWithMtime> matched_files_with_mtime;
  auto collect_matching_files = [&](const std::filesystem::path& path, const std::filesystem::path& file_name) -> bool {
    utils::Regex pattern_regex(pattern);
    if (file_name != state.file_name_ && utils::regexMatch(file_name.string(), pattern_regex)) {
      auto full_file_name = path / file_name;
      TailStateWithMtime::TimePoint mtime{utils::file::last_write_time_point(full_file_name)};
      logger_->log_debug("File {} with mtime {} matches rolling filename pattern {}, so we are reading it", file_name, int64_t{mtime.time_since_epoch().count()}, pattern);
      matched_files_with_mtime.emplace_back(TailState{path, file_name}, mtime);
    }
    return true;
  };

  utils::file::list_dir(state.path_, collect_matching_files, logger_, false);

  return sortAndSkipMainFilePrefix(state, matched_files_with_mtime);
}

std::vector<TailState> TailFile::findRotatedFilesAfterLastReadTime(const TailState &state) const {
  logger_->log_debug("Searching for files rolled over after last read time: {}", state.lastReadTimeInMilliseconds());

  std::string pattern = parseRollingFilePattern(state);

  std::vector<TailStateWithMtime> matched_files_with_mtime;
  auto collect_matching_files = [&](const std::filesystem::path& path, const std::filesystem::path& file_name) -> bool {
    utils::Regex pattern_regex(pattern);
    if (file_name != state.file_name_ && utils::regexMatch(file_name.string(), pattern_regex)) {
      auto full_file_name = path / file_name;
      TailStateWithMtime::TimePoint mtime{utils::file::last_write_time_point(full_file_name)};
      logger_->log_debug("File {} with mtime {} matches rolling filename pattern {}", file_name, int64_t{mtime.time_since_epoch().count()}, pattern);
      if (mtime >= std::chrono::time_point_cast<std::chrono::seconds>(state.last_read_time_)) {
        logger_->log_debug("File {} has mtime >= last read time, so we are going to read it", file_name);
        matched_files_with_mtime.emplace_back(TailState{path, file_name}, mtime);
      }
    }
    return true;
  };

  utils::file::list_dir(state.path_, collect_matching_files, logger_, false);

  return sortAndSkipMainFilePrefix(state, matched_files_with_mtime);
}

std::vector<TailState> TailFile::sortAndSkipMainFilePrefix(const TailState &state, std::vector<TailStateWithMtime>& matched_files_with_mtime) {
  const auto first_by_mtime_then_by_name = [](const auto& left, const auto& right) {
    return std::tie(left.mtime_, left.tail_state_.file_name_) <
           std::tie(right.mtime_, right.tail_state_.file_name_);
  };
  matched_files_with_mtime |= ranges::actions::sort(first_by_mtime_then_by_name);

  if (!matched_files_with_mtime.empty() && state.position_ > 0) {
    TailState &first_rotated_file = matched_files_with_mtime[0].tail_state_;
    auto full_file_name = first_rotated_file.fileNameWithPath();
    if (utils::file::file_size(full_file_name) >= state.position_) {
      uint64_t checksum = utils::file::computeChecksum(full_file_name, state.position_);
      if (checksum == state.checksum_) {
        first_rotated_file.position_ = state.position_;
        first_rotated_file.checksum_ = state.checksum_;
      }
    }
  }

  std::vector<TailState> matched_files;
  matched_files.reserve(matched_files_with_mtime.size());
  std::transform(matched_files_with_mtime.begin(), matched_files_with_mtime.end(), std::back_inserter(matched_files),
                 [](TailStateWithMtime &tail_state_with_mtime) { return std::move(tail_state_with_mtime.tail_state_); });
  return matched_files;
}

void TailFile::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  if (tail_mode_ == Mode::MULTIPLE) {
    if (last_multifile_lookup_ + lookup_frequency_ < std::chrono::steady_clock::now()) {
      logger_->log_debug("Lookup frequency {} have elapsed, doing new multifile lookup", lookup_frequency_);
      doMultifileLookup(context);
    } else {
      logger_->log_trace("Skipping multifile lookup");
    }
  }

  // iterate over file states. may modify them
  for (auto &state : tail_states_) {
    processFile(session, state.first, state.second);
  }

  if (!session.existsFlowFileInRelationship(Success)) {
    yield();
  }

  first_trigger_ = false;
}

bool TailFile::isOldFileInitiallyRead(TailState &state) const {
  // This is our initial processing and no stored state was found
  return first_trigger_ && state.last_read_time_ == std::chrono::file_clock::time_point{};
}

void TailFile::processFile(core::ProcessSession& session,
                           const std::filesystem::path& full_file_name,
                           TailState &state) {
  if (isOldFileInitiallyRead(state)) {
    if (initial_start_position_ == InitialStartPositions::BEGINNING_OF_TIME) {
      processAllRotatedFiles(session, state);
    } else if (initial_start_position_ == InitialStartPositions::CURRENT_TIME) {
      state.position_ = utils::file::file_size(full_file_name);
      state.last_read_time_ = std::chrono::file_clock::now();
      state.checksum_ = utils::file::computeChecksum(full_file_name, state.position_);
      storeState();
      return;
    }
  } else {
    uint64_t fsize = utils::file::file_size(full_file_name);
    if (fsize < state.position_) {
      processRotatedFilesAfterLastReadTime(session, state);
    } else if (fsize == state.position_) {
      logger_->log_trace("Skipping file {} as its size hasn't changed since last read", state.file_name_);
      return;
    }
  }

  processSingleFile(session, full_file_name, state);
  storeState();
}

void TailFile::processRotatedFilesAfterLastReadTime(core::ProcessSession& session, TailState &state) {
  std::vector<TailState> rotated_file_states = findRotatedFilesAfterLastReadTime(state);
  processRotatedFiles(session, state, rotated_file_states);
}

void TailFile::processAllRotatedFiles(core::ProcessSession& session, TailState &state) {
  std::vector<TailState> rotated_file_states = findAllRotatedFiles(state);
  processRotatedFiles(session, state, rotated_file_states);
}

void TailFile::processRotatedFiles(core::ProcessSession& session, TailState &state, std::vector<TailState> &rotated_file_states) {
  for (TailState &file_state : rotated_file_states) {
    processSingleFile(session, file_state.fileNameWithPath(), file_state);
  }
  state.position_ = 0;
  state.checksum_ = 0;
}

void TailFile::processSingleFile(core::ProcessSession& session,
                                 const std::filesystem::path& full_file_name,
                                 TailState &state) {
  auto fileName = state.file_name_;

  if (utils::file::file_size(full_file_name) == 0U) {
    logger_->log_warn("Unable to read file {} as it does not exist or has size zero", full_file_name);
    return;
  }
  logger_->log_debug("Tailing file {} from {}", full_file_name, state.position_);

  std::string baseName = fileName.stem().string();
  std::string extension = fileName.extension().string();
  if (extension.starts_with('.'))
    extension.erase(extension.begin());

  if (delimiter_) {
    logger_->log_trace("Looking for delimiter 0x{:X}", *delimiter_);

    std::size_t num_flow_files = 0;
    FileReaderCallback file_reader{full_file_name, state.position_, *delimiter_, state.checksum_};
    TailState state_copy{state};

    while (file_reader.hasMoreToRead() && (!batch_size_ || *batch_size_ > num_flow_files)) {
      auto flow_file = session.create();
      session.write(flow_file, std::ref(file_reader));

      if (file_reader.useLatestFlowFile()) {
        updateFlowFileAttributes(full_file_name, state_copy, fileName, baseName, extension, flow_file);
        session.transfer(flow_file, Success);
        updateStateAttributes(state_copy, flow_file->getSize(), file_reader.checksum());

        ++num_flow_files;

      } else {
        session.remove(flow_file);
      }
    }

    state = state_copy;
    logger_->log_info("{} flowfiles were received from TailFile input", num_flow_files);

  } else {
    WholeFileReaderCallback file_reader{full_file_name, state.position_, state.checksum_};
    auto flow_file = session.create();
    session.write(flow_file, std::ref(file_reader));

    updateFlowFileAttributes(full_file_name, state, fileName, baseName, extension, flow_file);
    session.transfer(flow_file, Success);
    updateStateAttributes(state, flow_file->getSize(), file_reader.checksum());
  }
}

void TailFile::updateFlowFileAttributes(const std::filesystem::path& full_file_name, const TailState& state,
                                        const std::filesystem::path& fileName, const std::string& baseName,
                                        const std::string& extension,
                                        std::shared_ptr<core::FlowFile> &flow_file) const {
  logger_->log_info("TailFile {} for {} bytes", fileName, flow_file->getSize());
  std::string logName = textfragmentutils::createFileName(baseName, extension, state.position_, flow_file->getSize());
  flow_file->setAttribute(core::SpecialFlowAttribute::PATH, state.path_.string());
  flow_file->addAttribute(core::SpecialFlowAttribute::ABSOLUTE_PATH, full_file_name.string());
  flow_file->setAttribute(core::SpecialFlowAttribute::FILENAME, logName);

  flow_file->setAttribute(textfragmentutils::BASE_NAME_ATTRIBUTE, baseName);
  flow_file->setAttribute(textfragmentutils::POST_NAME_ATTRIBUTE, extension);
  flow_file->setAttribute(textfragmentutils::OFFSET_ATTRIBUTE, std::to_string(state.position_));

  if (extra_attributes_.contains(state.path_.string())) {
    std::string prefix;
    if (attribute_provider_service_) {
      prefix = std::string(attribute_provider_service_->name()) + ".";
    }
    for (const auto& [key, value] : extra_attributes_.at(state.path_.string())) {
      flow_file->setAttribute(prefix + key, value);
    }
  }
}

void TailFile::updateStateAttributes(TailState &state, uint64_t size, uint64_t checksum) {
  state.position_ += size;
  state.last_read_time_ = std::chrono::file_clock::now();
  state.checksum_ = checksum;
}

void TailFile::doMultifileLookup(core::ProcessContext& context) {
  checkForRemovedFiles();
  checkForNewFiles(context);
  last_multifile_lookup_ = std::chrono::steady_clock::now();
}

void TailFile::checkForRemovedFiles() {
  gsl_Expects(pattern_regex_);
  std::vector<std::filesystem::path> file_names_to_remove;

  for (const auto &kv : tail_states_) {
    const auto& full_file_name = kv.first;
    const TailState &state = kv.second;
    if (utils::file::file_size(state.fileNameWithPath()) == 0U ||
        !utils::regexMatch(state.file_name_.string(), *pattern_regex_)) {
      file_names_to_remove.push_back(full_file_name);
    }
  }

  for (const auto &full_file_name : file_names_to_remove) {
    tail_states_.erase(full_file_name);
  }
}

void TailFile::checkForNewFiles(core::ProcessContext& context) {
  gsl_Expects(pattern_regex_);
  auto add_new_files_callback = [&](const std::filesystem::path& path, const std::filesystem::path& file_name) -> bool {
    auto full_file_name = path / file_name;
    if (!containsKey(tail_states_, full_file_name) && utils::regexMatch(file_name.string(), *pattern_regex_)) {
      tail_states_.emplace(full_file_name, TailState{path, file_name});
    }
    return true;
  };

  if (!attribute_provider_service_) {
    utils::file::list_dir(base_dir_, add_new_files_callback, logger_, recursive_lookup_);
    return;
  }

  const auto attribute_maps = attribute_provider_service_->getAttributes();
  if (!attribute_maps) {
    logger_->log_error("Could not get attributes from the Attribute Provider Service");
    return;
  }

  for (const auto& attribute_map : *attribute_maps) {
    std::string base_dir = baseDirectoryFromAttributes(attribute_map, context);
    extra_attributes_[base_dir] = attribute_map;
    utils::file::list_dir(base_dir, add_new_files_callback, logger_, recursive_lookup_);
  }
}

std::string TailFile::baseDirectoryFromAttributes(const controllers::AttributeProviderService::AttributeMap& attribute_map, core::ProcessContext& context) {
  auto flow_file = core::FlowFile::create();
  for (const auto& [key, value] : attribute_map) {
    flow_file->setAttribute(key, value);
  }
  return context.getProperty(BaseDirectory, flow_file.get()).value();
}

std::chrono::milliseconds TailFile::getLookupFrequency() const {
  return lookup_frequency_;
}

REGISTER_RESOURCE(TailFile, Processor);

}  // namespace org::apache::nifi::minifi::processors
