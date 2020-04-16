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
#include <limits>
#include <inttypes.h>

#include <limits.h>
#ifndef WIN32
#include <dirent.h>
#include <unistd.h>
#endif
#include <vector>
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
#include "io/CRCStream.h"

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

template <typename T>
static T Convert(const std::string& s) {
  T value;

  std::istringstream iss(s);
  iss >> value;

  return value;
}

auto Uint64(const std::string& s) {
  return Convert<uint64_t>(s);
}

static const std::string MapPrefix = "file.";

static const std::string FilenameKey = MapPrefix + "filename.";
static const std::string ReaderFilenameKey = MapPrefix + "readerFilename.";
static const std::string PositionKey = MapPrefix + "position.";
static const std::string TimestampKey = MapPrefix + "timestamp.";
static const std::string ChecksumKey = MapPrefix + "checksum.";
static const std::string LengthKey = MapPrefix + "length.";

// Class to hold information about state to allow maintain this state across multiple invocations of the Processor.
class TailFileState {
  std::string filename_;
  std::string readerFilename_;
  uint64_t position_{};
  uint64_t timestamp_{};
  uint64_t length_{};
  uint64_t checksum_{};

public:
  TailFileState() {}

  TailFileState(const std::string& filename, const std::string& readerFilename, uint64_t position, uint64_t timestamp, uint64_t length, uint64_t checksum)
    : filename_(filename), position_(position), length_(length), timestamp_(timestamp), checksum_(checksum) {
  }

  auto filename() const {
    return filename_;
  }

  auto readerFilename() const {
    return readerFilename_;
  }

  auto position() const {
    return position_;
  }

  auto timestamp() const {
    return timestamp_;
  }

  auto length() const {
    return length_;
  }

  auto checksum() const {
    return checksum_;
  }

  auto toStateMap(int index) const {
    const auto strIndex = std::to_string(index);

    return std::unordered_map<std::string, std::string> {
      {FilenameKey, filename_ + strIndex},
      {ReaderFilenameKey, readerFilename_ + strIndex},
      {PositionKey, std::to_string(position_) + strIndex},
      {LengthKey, std::to_string(length_) + strIndex},
      {TimestampKey, std::to_string(timestamp_) + strIndex},
      {ChecksumKey, std::to_string(checksum_) + strIndex}
    };
  }
};

class TailFileObject {
  TailFileState state_;
  uint64_t expectedRecoveryChecksum_{};
  int filenameIndex_{};
  bool tailFileChanged_{ true };

public:
  TailFileObject(int index) {
    filenameIndex_ = index;
  }

  TailFileObject(int index, const TailFileState& fileState, bool tailFileChanged)
    : filenameIndex_(index), tailFileChanged_(true), state_(fileState) {
  }

  TailFileObject(int index, const std::unordered_map<std::string, std::string>& statesMap)
    : filenameIndex_(index), tailFileChanged_(false)
  {
    const auto strIndex = std::to_string(index);

    const auto& filename = statesMap.at(FilenameKey + strIndex);
    const auto& readerFilename = statesMap.at(ReaderFilenameKey + strIndex);
    const auto position = Uint64(statesMap.at(PositionKey + strIndex));
    const auto timestamp = Uint64(statesMap.at(TimestampKey + strIndex));
    const auto length = Uint64(statesMap.at(LengthKey + strIndex));

    state_ = TailFileState(filename, readerFilename, position, timestamp, length, 0);
  }

  auto getFilenameIndex() const {
    return filenameIndex_;
  }

  void setFilenameIndex(int filenameIndex) {
    filenameIndex_ = filenameIndex;
  }

  auto getState() const {
    return state_;
  }

  void setState(const TailFileState& state) {
    state_ = state;
  }

  auto getExpectedRecoveryChecksum() const {
    return expectedRecoveryChecksum_;
  }

  void setExpectedRecoveryChecksum(uint64_t expectedRecoveryChecksum) {
    expectedRecoveryChecksum_ = expectedRecoveryChecksum;
  }

  auto getTailFileChanged() const {
    return tailFileChanged_;
  }

  void setTailFileChanged(bool tailFileChanged) {
    tailFileChanged_ = tailFileChanged;
  }
};


core::Property TailFile::BaseDirectory(
  core::PropertyBuilder::createProperty("tail-base-directory", "Base Directory")->
  withDescription("Base directory used to look for files to tail. This property is required when using Multifile mode.")->
  isRequired(false)->
  supportsExpressionLanguage(true)->
  build());

static const std::string ModeSingleFile = "Single file";
static const std::string ModeMultiplFiles = "Multiple files";

core::Property TailFile::Mode(
  core::PropertyBuilder::createProperty("tail-mode", "Tailing Mode")->
  withDescription("Mode to use: single file will tail only one file, multiple file will look for a list of file. In Multiple mode the Base directory is required.")->
  isRequired(true)->
  withAllowableValues<std::string>({ModeSingleFile, ModeMultiplFiles})->
  withDefaultValue(ModeSingleFile)->
  build());

core::Property TailFile::FileName(
  core::PropertyBuilder::createProperty("File to Tail", "File(s) to Tail")->
  withDescription(
    "Path of the file to tail in case of single file mode.If using multifile mode, regular expression to find files to tail in the base directory. "
    "In case recursivity is set to true, the regular expression will be used to match the path starting from the base directory.")->
  isRequired(true)->
  supportsExpressionLanguage(true)->
  build());

core::Property TailFile::RollingFileNamePattern(
  core::PropertyBuilder::createProperty("Rolling Filename Pattern")->
  withDescription(
    "If the file to tail \"rolls over\" as would be the case with log files, this filename pattern will be used to identify files "
    "that have rolled over so that if Minifi is restarted, and the file has rolled over, it will be able to pick up where it left off. "
    "This pattern supports wildcard characters * and ?, it also supports the notation ${filename} to specify a pattern based on the name of the file (without extension), "
    "and will assume that the files that have rolled over live in the same directory as the file being tailed. The same glob pattern will be used for all files.")->
  isRequired(false)->
  build());

static const std::string StartBeginOfTime = "Beginning of Time";
static const std::string StartBeginOfFile = "Beginning of File";
static const std::string StartCurrentTime = "Current Time";

core::Property TailFile::StartPosition(
  core::PropertyBuilder::createProperty("Initial Start Position")->
  withDescription(
    "When the Processor first begins to tail data, this property specifies where the Processor should begin reading data. "
    "Once data has been ingested from a file, the Processor will continue from the last point from which it has received data.")->
  isRequired(true)->
  withAllowableValues<std::string>({ StartBeginOfTime, StartBeginOfFile, StartCurrentTime})->
  withDefaultValue(StartBeginOfFile)->
  build());

core::Property TailFile::Recursive(
  core::PropertyBuilder::createProperty("tailfile-recursive-lookup", "Recursive lookup")->
  withDescription(
    "When using Multiple files mode, this property defines if files must be listed recursively or not in the base directory.")->
  isRequired(true)->
  withDefaultValue<bool>(false)->
  build());

core::Property TailFile::LookupFrequency(
  core::PropertyBuilder::createProperty("tailfile-lookup-frequency", "Lookup frequency")->
  withDescription(
    "Only used in Multiple files mode and Changing name rolling strategy. "
    "It specifies the minimum duration the processor will wait before listing again the files to tail.")->
  isRequired(false)->
  withDefaultValue<core::TimePeriodValue>("10 min")->
  build());

core::Property TailFile::MaximumAge(
  core::PropertyBuilder::createProperty("tailfile-maximum-age", "Maximum age")->
  withDescription(
    "Only used in Multiple files mode and Changing name rolling strategy. It specifies the necessary "
    "minimum duration to consider that no new messages will be appended in a file regarding its last modification date. "
    "This should not be set too low to avoid duplication of data in case new messages are appended at a lower frequency.")->
  isRequired(false)->
  withDefaultValue<core::TimePeriodValue>("24 hours")->
  build());

core::Relationship TailFile::Success("success", "All files are routed to success");

void TailFile::initialize() {
  setSupportedProperties({BaseDirectory, Mode, FileName, RollingFileNamePattern, StartPosition, Recursive, LookupFrequency, MaximumAge});

  setSupportedRelationships({Success});
}

void TailFile::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  context->getProperty(BaseDirectory.getName(), baseDirectory_);
  if (!baseDirectory_.empty()) {
    if (!utils::file::FileUtils::is_directory(baseDirectory_.c_str())) {
      throw minifi::Exception(PROCESSOR_EXCEPTION, "TailFile: BaseDirectory property is invalid.");
    }
  }

  std::string mode;
  context->getProperty(Mode.getName(), mode);
  isMultiChanging_ = ModeMultiplFiles == mode;

  context->getProperty(FileName.getName(), fileName_);
  if (fileName_.empty()) {
    throw minifi::Exception(PROCESSOR_EXCEPTION, "TailFile: FileName property is empty.");
  }

  context->getProperty(RollingFileNamePattern.getName(), rollingFileNamePattern_);
  context->getProperty(StartPosition.getName(), startPosition_);
  context->getProperty(Recursive.getName(), recursive_);

  std::string lookupFrequency;
  context->getProperty(LookupFrequency.getName(), lookupFrequency);
  core::TimeUnit unitLookupFrequency;
  if (!core::Property::StringToTime(lookupFrequency, lookupFrequency_, unitLookupFrequency) && 
      !core::Property::ConvertTimeUnitToMS(lookupFrequency_, unitLookupFrequency, lookupFrequency_)) {
    throw minifi::Exception(PROCESSOR_EXCEPTION, "TailFile: LookupFrequency property is invalid.");
  }

  std::string maxAge;
  context->getProperty(MaximumAge.getName(), maxAge);
  core::TimeUnit unitMaxAge;
  if (!core::Property::StringToTime(maxAge, maxAge_, unitMaxAge) && !core::Property::ConvertTimeUnitToMS(maxAge_, unitMaxAge, maxAge_)) {
    throw minifi::Exception(PROCESSOR_EXCEPTION, "TailFile: LookupFrequency property is invalid.");
  }
  // maxAge_ in seconds.
  maxAge_ /= 1000;
}

std::vector<std::string> TailFile::lookup(core::ProcessContext &context) {
  lastLookup_ = getTimeMillis();

  return isMultiChanging_ ? getFilesToTail(baseDirectory_, fileName_, recursive_, maxAge_) : std::vector<std::string>{fileName_};
}

void TailFile::recoverState(core::ProcessContext& context) {
  const auto filesToTail = lookup(context);

  auto& stateMap = getStateMap();

  initStates(filesToTail, stateMap, stateMap.empty(), startPosition_);
  recoverState(context, filesToTail, stateMap);
}

void TailFile::initStates(const std::vector<std::string>& filesToTail, const std::unordered_map<std::string, std::string>& statesMap, bool isCleared, const std::string& startPosition) {
  auto fileIndex = 0u;

  if (isCleared) {
    states_.clear();
  } else {
    // We have to deal with the case where Minifi has been restarted. In this case 'states' object is empty but the statesMap is not. 
    // So we have to put back the files we already know about in 'states' object before doing the recovery.
    if (states_.empty() && !statesMap.empty()) {
      for (const auto it: statesMap) {
        const auto& key = it.first;
        // If 'key' startsWith 'FilenameKey'.
        if (!key.rfind(FilenameKey, 0)) {
          const auto index = Convert<int>(key.substr(FilenameKey.size()));
          states_.insert({ it.second, std::make_shared<TailFileObject>(index, statesMap) });
        }
      }
    }

    // First, we remove the files that are no longer present.
    for (auto it = states_.begin(); it != states_.end(); ) {
      const auto& file = it->first;
      if (filesToTail.end() == std::find(filesToTail.begin(), filesToTail.end(), file)) {
        it = states_.erase(it);
      } else {
        ++it;
      }
    }

    // Then we need to get the highest ID used so far to be sure we don't mix different files in case we add new files to tail.
    for (auto it = states_.begin(); it != states_.end(); ++it) {
      const auto filenameIndex = it->second->getFilenameIndex();
      if (fileIndex <= filenameIndex) {
        fileIndex = filenameIndex + 1;
      }
    }
  }

  for (const auto& filename: filesToTail) {
    if (isCleared || !states_.count(filename)) {
      states_.insert({filename, std::make_shared<TailFileObject>(fileIndex, TailFileState(filename, "", 0, 0, 0, 0), true)});

      fileIndex++;
    }
  }
}

void TailFile::recoverState(core::ProcessContext& context, const std::vector<std::string>& filesToTail, std::unordered_map<std::string, std::string>& map) {
  for (const auto& file: filesToTail) {
    recoverState(context, map, file);
  }
}

// List the files to tail according to the given base directory and using the user-provided regular expression.
std::vector<std::string> TailFile::getFilesToTail(const std::string& baseDir, const std::string& fileRegex, bool isRecursive, uint64_t maxAge) {
  const auto files = utils::file::FileUtils::list_dir_all(baseDir, logger_, isRecursive);

  std::string fullRegex = baseDir;
  if (*baseDir.rbegin() != utils::file::FileUtils::get_separator()) {
    fullRegex += utils::file::FileUtils::get_separator();
  }
  fullRegex += fileRegex;

  std::regex rgx(fullRegex);

  std::vector<std::string> ret;

  for (const auto& fileInfo : files) {
    const auto path = fileInfo.first + utils::file::FileUtils::get_separator() + fileInfo.second;

    if (!std::regex_match(path, rgx)) {
      continue;
    }

    const auto lastModified = utils::file::FileUtils::last_write_time(path);
    if (!lastModified) {
      logger_->log_warn("'last_write_time' for '%s'", path.c_str());
      continue;
    }

    if (isMultiChanging_) {
      if (std::time(0) - lastModified < maxAge_) {
        ret.push_back(path);
      }
    } else {
      ret.push_back(path);
    }
  }

  return ret;
}

// Updates member variables to reflect the "expected recovery checksum" and seek to the appropriate location in the tailed file, 
// updating our checksum, so that we are ready to proceed with the 'onTrigger'.
void TailFile::recoverState(core::ProcessContext& context, const std::unordered_map<std::string, std::string>& stateValues, const std::string& filePath) {
  const auto index = std::to_string(states_.at(filePath)->getFilenameIndex());

  const auto filenameKeyIndex = FilenameKey + index;
  const auto readerFilenameKeyIndex = ReaderFilenameKey + index;
  const auto positionKeyIndex = PositionKey + index;
  const auto timestampKeyIndex = TimestampKey + index;
  const auto lengthKeyIndex = LengthKey + index;
  const auto checksumKeyIndex = ChecksumKey + index;

  for (const auto& key : {checksumKeyIndex, filenameKeyIndex, readerFilenameKeyIndex, positionKeyIndex, timestampKeyIndex, lengthKeyIndex}) {
    if (!stateValues.count(key)) {
      resetState(filePath);
      return;
    }
  }

  const auto storedStateFilename = stateValues.at(filenameKeyIndex);
  const auto readerFilename = stateValues.at(readerFilenameKeyIndex);
  auto position = Uint64(stateValues.at(positionKeyIndex));
  const auto timestamp = Uint64(stateValues.at(timestampKeyIndex));
  const auto length = Uint64(stateValues.at(lengthKeyIndex));
  const auto checksumValue = Uint64(stateValues.at(checksumKeyIndex));

  if (filePath != storedStateFilename) {
    resetState(filePath);
    return;
  }

  auto& tfo = states_.at(filePath);

  tfo->setExpectedRecoveryChecksum(checksumValue);

  // We have an expected checksum and the currently configured filename is the same as the state file.
  // We need to check if the existing file is the same as the one referred to in the state file based on the checksum.
  uint64_t fileSize{};
  if (!getFileSize(storedStateFilename, fileSize)) {
    logger_->log_error("!getFileSize file %s", storedStateFilename.c_str());
    resetState(filePath);
    return;
  }
  std::ifstream file(storedStateFilename.c_str(), std::ifstream::in);
  if (!file) {
    logger_->log_error("load state file failed %s", storedStateFilename);
    resetState(filePath);
    return;
  }

  std::string readerFileName;

  if (fileSize >= position) {
    bool positionIsLarge = false;
    const auto checksumResult = calcCRC(file, tfo->getState().position(), positionIsLarge);
    if (positionIsLarge) {
      logger_->log_debug(
        "When recovering state, file being tailed has less data than was stored in the state. Assuming rollover. Will begin tailing current file from beginning.");
    }

    if (checksumResult == tfo->getExpectedRecoveryChecksum()) {
      // Checksums match. This means that we want to resume reading from where we left off. So we will populate the reader object so that it will be used in onTrigger. 
      // If the checksums do not match, SET 'position' = 0, so that the next call to onTrigger will result in a new Reader being created and starting at the beginning of the file.
      logger_->log_debug("When recovering state, checksum of tailed file matches the stored checksum. Will resume where left off.");
      readerFileName = storedStateFilename;
    } else {
      // We don't seek the reader to the position, so our reader will start at beginning of file.
      position = 0;
      logger_->log_debug("When recovering state, checksum of tailed file does not match the stored checksum. Will begin tailing current file from beginning.");
    }
  } else {
    // Fewer bytes than our position, so we know we weren't already reading from this file. Keep reader at a position of 0.
    logger_->log_debug(
      "When recovering state, existing file to tail is only %" PRIu64 " bytes but position flag is %" PRIu64 ", this indicates that the file has rotated. "
      "Will begin tailing current file from beginning.", fileSize, position);
  }

  tfo->setState(TailFileState(filePath, readerFileName, position, timestamp, length, 0));
}

void TailFile::cleanup() {
  for (auto it : states_) {
    auto& tfo = it.second;

    const auto state = tfo->getState();
    tfo->setState(TailFileState(state.filename(), state.readerFilename(), state.position(), state.timestamp(), state.length(), state.checksum()));
  }
}

void TailFile::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  std::unique_lock<std::mutex> lock(onTriggerMutex_, std::try_to_lock);
  if (!lock.owns_lock()) {
    logger_->log_warn("'onTrigger' is called before previous 'onTrigger' call is finished.");
    context->yield();
    return;
  }

  if (isMultiChanging_) {
    auto timeSinceLastLookup = getTimeMillis() - lastLookup_;
    if (timeSinceLastLookup > lookupFrequency_) {
      try {
        initStates(lookup(*context), getStateMap(), false, startPosition_);
      }
      catch (std::ifstream::failure e) {
        logger_->log_error("TailFile::onTrigger '%s'", e.what());
        throw;
      }
    }
  }

  if (states_.empty()) {
    context->yield();
    return;
  }

  for (const auto it : states_) {
    processTailFile(*context, *session, it.first);
  }
}

void TailFile::resetState(const std::string& filePath) {
  auto& tfo = states_.at(filePath);

  tfo->setExpectedRecoveryChecksum(0);
  tfo->setState(TailFileState(filePath, "", 0, 0, 0, 0));
}

bool TailFile::getReaderSizePosition(uint64_t& size, uint64_t& position, std::ifstream& reader, const std::string& filename) {
  int64_t readerPosition = reader.tellg();
  if (-1 == readerPosition) {
    logger_->log_error("Reader '%s', 'tellg()' error.", filename.c_str());
    return false;
  }

  reader.seekg(0, std::ios_base::end);
  int64_t readerSize = reader.tellg();
  if (-1 == readerSize) {
    logger_->log_error("Reader '%s', 'tellg()' error.", filename.c_str());
    return false;
  }

  reader.seekg(readerPosition);

  size = readerSize;
  position = readerPosition;

  return true;
}

struct WriteCallback : public OutputStreamCallback {
  WriteCallback(std::ifstream& reader, const std::string& readerFilename, uint64_t& checksum, uint64_t& newPosition, std::shared_ptr<logging::Logger>& logger)
    : reader_(reader), readerFilename_(readerFilename), checksum_(checksum), newPosition_(newPosition), logger_(logger) {
  }

  int64_t process(std::shared_ptr<io::BaseStream> stream) {
    return readLines(stream);
  }

  // Read new lines from the given reader, copying it to the given Output Stream.
  // The Checksum is used in order to later determine whether or not data has been consumed.
  int64_t readLines(std::shared_ptr<io::BaseStream> stream) {
    auto writeToStream = [&stream](const std::vector<uint8_t>& line) {
      return stream->writeData(const_cast<uint8_t*>(&line[0]), line.size());
    };

    int64_t ret = 0;

    uint64_t pos = reader_.tellg();

    logger_->log_debug("Reading '%s' lines starting at position %" PRIu64 ".", readerFilename_.c_str(), pos);

    newPosition_ = pos;

    uint64_t linesRead{};
    bool seenCR = false;

    std::vector<uint8_t> line;

    std::vector<char> buf(102400);
    bool endOfReader = false;
    do {
      if (endOfReader = !reader_.read(buf.data(), buf.size())) {
        buf.resize(reader_.gcount());
      }

      for (auto i = 0; i < buf.size(); i++) {
        uint8_t c = buf[i];
        switch (c) {
          case '\n': {
            seenCR = false;
            line.push_back(c);
            ret += writeToStream(line);
            checksum_ = crc32(checksum_, &c, 1);
            line.clear();
            newPosition_ = pos + i + 1;
            linesRead++;
          }
          break;

          case '\r': {
            seenCR = true;
            line.push_back(c);
          }
          break;

          default: {
            if (seenCR) {
              seenCR = false;
              ret += writeToStream(line);
              checksum_ = crc32(checksum_, &c, 1);
              linesRead++;
              line.clear();
              newPosition_ = pos + i;
            }

            line.push_back(c);
          }
        }
      }

      pos = reader_.tellg();
    } while (!endOfReader);

    if (newPosition_ < reader_.tellg()) {
      logger_->log_debug("Read '%s', %" PRIu64 " lines; repositioning reader from %" PRIu64 "to %" PRIu64 ".", readerFilename_.c_str(), linesRead, pos, newPosition_);

      // Ensure we can re-read if necessary.
      reader_.seekg(newPosition_);
    }

    return ret;
  }

  std::ifstream& reader_;
  const std::string& readerFilename_;
  uint64_t& checksum_;
  uint64_t& newPosition_;
  std::shared_ptr<logging::Logger>& logger_;
};


void TailFile::processTailFile(core::ProcessContext& context, core::ProcessSession& session, const std::string& tailFile) {
  // If user changes the file that is being tailed, we need to consume the already-rolled-over data according to the Initial Start Position property.

  bool rolloverOccurred{};

  auto tfo = states_.at(tailFile);

  if (tfo->getTailFileChanged()) {
    rolloverOccurred = false;

    if (startPosition_ == StartBeginOfTime) {
      recoverRolledFiles(context, session, tailFile, tfo->getExpectedRecoveryChecksum(), tfo->getState().timestamp(), tfo->getState().position());
    } else if (startPosition_ == StartCurrentTime) {
      cleanup();
      tfo->setState(TailFileState(tailFile, "", 0, 0, 0, 0));
    } else {
      uint64_t fileSize{};
      uint64_t fileLastModifiedTime{};
      if (!getFileSizeLastModifiedTime(tailFile, fileSize, fileLastModifiedTime)) {
        logger_->log_error("!getFileSizeLastModifiedTime file %s", tailFile.c_str());
        context.yield();
        return;
      }

      std::ifstream reader;
      if (!createReader(reader, tailFile)) {
        context.yield();
        return;
      }

      const auto position = fileSize;
      const auto timestamp = fileLastModifiedTime + 1;

      bool sizeIsLarge{};
      const auto checksumResult = calcCRC(reader, position, sizeIsLarge);
      
      cleanup();

      tfo->setState(TailFileState(tailFile, tailFile, position, timestamp, position, checksumResult));
    }

    tfo->setTailFileChanged(false);
  } else {
    // Recover any data that may have rolled over since the last time that this processor ran.
    // If expectedRecoveryChecksum != null, that indicates that this is the first iteration since processor was started, 
    // so use whatever checksum value was present when the state was last persisted. 
    // In this case, we must then null out the value so that the next iteration won't keep using the "recovered" value. 
    // If the value is null, then we know that either the processor has already recovered that data, or there was no state persisted. 
    // In either case, use whatever checksum value is currently in the state.

    auto expectedChecksumValue = tfo->getExpectedRecoveryChecksum();
    if (!expectedChecksumValue) {
      expectedChecksumValue = tfo->getState().checksum();
    }

    rolloverOccurred = recoverRolledFiles(context, session, tailFile, expectedChecksumValue, tfo->getState().timestamp(), tfo->getState().position());
    tfo->setExpectedRecoveryChecksum(0);
  }

  // Initialize local variables from state object; this is done so that we can easily change the values throughout
  // the onTrigger method and then create a new state object after we finish processing the files.
  const auto state = tfo->getState();
  const auto filename = state.filename();
  auto readerFilename = state.readerFilename();
  auto checksum = state.checksum();
  auto position = state.position();
  auto timestamp = state.timestamp();
  auto length = state.length();

  std::ifstream reader;

  auto fileExists = false;

  uint64_t fileSize{};
  uint64_t fileLastModifiedTime{};

  // Create a reader if necessary.
  if (readerFilename.empty()) {
    readerFilename = tailFile;

    if (!(fileExists = createReader(reader, readerFilename, position) && getFileSizeLastModifiedTime(readerFilename, fileSize, fileLastModifiedTime))) {
      context.yield();
      return;
    }
  } else {
    fileExists = createReader(reader, readerFilename, position) && getFileSizeLastModifiedTime(readerFilename, fileSize, fileLastModifiedTime);
  }

  const auto startTime = getTimeMillis(); 

  // Check if file has rotated.
  // We determine that the file has rotated if any of the following conditions are met:
  // 1. 'rolloverOccured' == true, which indicates that we have found a new file matching the rollover pattern.
  // 2. The file was modified after the timestamp in our state, AND the file is smaller than we expected. This satisfies
  //    the case where we are tailing File A, and that file is then renamed (say to B) and a new file named A is created
  //    and is written to. In such a case, File A may have a file size smaller than we have in our state, so we know that
  //    it rolled over.
  // 3. The File Channel that we have indicates that the size of the file is different than file.length() indicates, AND
  //    the File Channel also indicates that we have read all data in the file. This case may also occur in the same scenario
  //    as #2, above. In this case, the File Channel is pointing to File A, but the 'file' object is pointing to File B. They
  //    both have the same name but are different files. As a result, once we have consumed all data from the File Channel,
  //    we want to roll over and consume data from the new file.

  bool fileSizeEqPosition = false;

  if (fileExists) {
    auto rotated = rolloverOccurred;

    if (!rotated) {
      if (length > fileSize) {
        rotated = true;
      } else {
        uint64_t readerSize{};
        uint64_t readerPosition{};
        if (getReaderSizePosition(readerSize, readerPosition, reader, readerFilename)) {
          if (readerSize == readerPosition && readerSize != fileSize) {
            rotated = true;
          }
        } else {
          logger_->log_warn(
            "Failed to determined the reader '%s' size when determining if the file has rolled over. Will assume that the file being tailed has not rolled over.", readerFilename.c_str());
        }
        if (readerSize == readerPosition && readerSize != fileSize) {
          rotated = true;
        }
      }
    }

    if (rotated) {
      // Since file has rotated, we set position to 0.
      reader.seekg(0);

      position = 0;
      checksum = 0;
    }

    fileSizeEqPosition = fileSize == position;
  }

  if (fileSizeEqPosition || !fileExists) {
    // No data to consume so rather than continually running, yield to allow other processors to use the thread.
    logger_->log_debug("No data to consume; created no FlowFiles");
    tfo->setState(TailFileState(tailFile, readerFilename, position, timestamp, length, checksum));
    persistState(tfo, context);
    context.yield();
    return;
  }

  // If there is data to consume, read as much as we can and stream it to a new FlowFile.
  auto flowFile = session.create();

  uint64_t newPosition{};
  WriteCallback writer(reader, readerFilename, checksum, newPosition, logger_);
  session.write(flowFile, &writer);

  // If there ended up being no data, just remove the FlowFile.
  if (flowFile->getSize() == 0) {
    session.remove(flowFile);
    logger_->log_debug("No data to consume; removed created FlowFile");
  } else {
    // Determine filename for FlowFile by using <base filename of log file>.<initial offset>-<final offset>.<extension>.
    auto tailFilename = readerFilename;
    auto rposDot = tailFilename.rfind('.');
    
    std::string flowFileName = 
      (rposDot == std::string::npos)
        ? tailFilename + "." + std::to_string(position) + "-" + std::to_string(newPosition)
        : tailFilename.substr(0, rposDot) + "." + std::to_string(position) + "-" + std::to_string(newPosition) + tailFilename.substr(rposDot);

    session.putAttribute(flowFile, FlowAttributeKey(FILENAME), flowFileName);
    session.putAttribute(flowFile, FlowAttributeKey(MIME_TYPE), "text/plain");
    session.putAttribute(flowFile, "tailfile.original.path", tailFilename);

    session.getProvenanceReporter()->receive(flowFile, "file:/" + readerFilename, getUUIDStr(), "FlowFile contains bytes " + std::to_string(flowFile->getSize()), getTimeMillis() - startTime);
    session.transfer(flowFile, Success);

    position = newPosition;

    // Set timestamp to the latest of when the file was modified and the current timestamp stored in the state.
    // We do this because when we read a file that has been rolled over, we set the state to 1 millisecond later than the last mod date
    // in order to avoid ingesting that file again. If we then read from this file during the same second (or millisecond, 
    // depending on the operating system file last mod precision), then we could set the timestamp to a smaller value, 
    // which could result in reading in the rotated file a second time.

    timestamp = max(timestamp, fileLastModifiedTime);
    length = fileSize;
  }

  // Create a new state object to represent our current position, timestamp, etc.
  tfo->setState(TailFileState(tailFile, readerFilename, position, timestamp, length, checksum));

  // We must commit session before persisting state in order to avoid data loss on restart
  session.commit();
  persistState(tfo, context);
}


// Returns a list of all Files that match the following criteria:
// Filename matches the Rolling Filename Pattern.
// Filename does not match the actual file being tailed.
// The Last Modified Time on the file is equal to or later than the given minimum timestamp.
//
// The List that is returned will be ordered by file timestamp, providing oldest file first.
std::vector<std::string> TailFile::getRolledOffFiles(core::ProcessContext& context, uint64_t minTimestamp, const std::string& tailFilePath) {
  if (rollingFileNamePattern_.empty()) {
    return {};
  }

  const auto posSlash = tailFilePath.find_last_of("/\\");
  std::string directory =
    (posSlash == std::string::npos)
    ? tailFilePath.substr(0, posSlash)
    : ".";

  const auto rposDot = tailFilePath.rfind('.');
  std::string beforeDot =
    (rposDot == std::string::npos)
    ? tailFilePath
    : tailFilePath.substr(0, rposDot);

  auto rollingPattern = rollingFileNamePattern_;
  rollingPattern = utils::StringUtils::replaceAll(rollingPattern, "${filename}", beforeDot);

  std::vector<std::string> rolledOffFiles;

  const std::regex rgx(rollingPattern);
  const auto files = utils::file::FileUtils::list_dir_all(directory, logger_, false);
  for (const auto& file : files) {
    const auto filePath = file.first + file.second;
    if (std::regex_search(filePath, rgx)) {
      const auto lastModified = utils::file::FileUtils::last_write_time(filePath);

      if (lastModified < minTimestamp) {
        continue;
      }
      if (filePath == tailFilePath) {
        continue;
      }

      rolledOffFiles.emplace_back(filePath);
    }
  }

  // Sort files based on last modified timestamp. If same timestamp, use filename as a secondary sort, as often files that are rolled over 
  // are given a naming scheme that is lexicographically sort in the same order as the timestamp, such as yyyy-MM-dd-HH-mm-ss.
  std::sort(
    rolledOffFiles.begin(),
    rolledOffFiles.end(),
    [](const std::string& file1, const std::string& file2) {
      const auto time1 = utils::file::FileUtils::last_write_time(file1);
      const auto time2 = utils::file::FileUtils::last_write_time(file2);

      return (time1 == time2) ? file1 < file2 : time1 < time2;
    }
  );

  return rolledOffFiles;
}

void TailFile::persistState(const std::shared_ptr<TailFileObject>& tfo, core::ProcessContext& context) {
  auto& stateMap = getStateMap();

  const auto state = tfo->getState().toStateMap(tfo->getFilenameIndex());

  for (auto const& el : state) {
    stateMap.insert(el);
  }
}

bool TailFile::createReader(std::ifstream& reader, const std::string& filename, uint64_t position) {
  reader.open(filename.c_str(), std::ifstream::in);
  if (!reader) {
    logger_->log_warn("Unable to open file '%s'.", filename.c_str());
    return false;
  }

  logger_->log_debug("Created reader '%s'.", filename.c_str());

  uint64_t readerSize{};
  uint64_t readerPosition{};
  if (!getReaderSizePosition(readerSize, readerPosition, reader, filename)) {
    return false;
  }

  if (position >= readerSize) {
    logger_->log_warn("Reader '%s' position %" PRIu64 ">= file size %" PRId64 ".", filename.c_str(), position, readerSize);
    return false;
  }

  reader.seekg(position);

  return true;
}


// Finds any files that have rolled over and have not yet been ingested by this Processor. 
// Each of these files that is found will be ingested as its own FlowFile. 
// If a file is found that has been partially ingested, the rest of the file will be ingested 
// as a single FlowFile but the data that already has been ingested will not be ingested again.
bool TailFile::recoverRolledFiles(core::ProcessContext& context, core::ProcessSession& session, const std::string& tailFile, uint64_t expectedChecksum, uint64_t timestamp, uint64_t position) {
  // Find all files that match our rollover pattern, if any, and order them based on their timestamp and filename.
  // Ignore any file that has a timestamp earlier than the state that we have persisted. If we were reading from
  // a file when we stopped running, then that file that we were reading from should be the first file in this list,
  // assuming that the file still exists on the file system.
  const auto rolledOffFiles = getRolledOffFiles(context, timestamp, tailFile);
  return recoverRolledFiles(context, session, tailFile, rolledOffFiles, expectedChecksum, timestamp, position);
}

// Finds any files that have rolled over and have not yet// been ingested by this Processor.
// Each of these files that is found will be ingested as its own FlowFile.If a file is found that has been partially ingested, 
// the rest of the file will be ingested as a single FlowFile but the data that already has been ingested will not be ingested again.
bool TailFile::recoverRolledFiles(
  core::ProcessContext& context, core::ProcessSession& session, const std::string& tailFile, std::vector<std::string> rolledOffFiles, uint64_t expectedChecksum, uint64_t timestamp, uint64_t position) {
  if (rolledOffFiles.empty()) {
    return false;
  }

  logger_->log_debug("Recovering Rolled Off Files; total number of files rolled off %zu", rolledOffFiles.size());

  auto tfo = states_.at(tailFile);

  // For first file that we find, it may or may not be the file that we were last reading from.
  // As a result, we have to read up to the position we stored, while calculating the checksum. If the checksums match,
  // then we know we've already processed this file. If the checksums do not match, 
  // then we have notprocessed this file and we need to seek back to position 0 and ingest the entire file.
  // For all other files that have been rolled over, we need to just ingest the entire file.
  if (expectedChecksum != 0 && rolledOffFiles[0].size() >= position) {
    const auto firstFile = rolledOffFiles[0];

    const auto startTime = getTimeMillis();

    if (position > 0) {
      std::ifstream file(firstFile.c_str(), std::ifstream::in);
      if (!file) {
        logger_->log_error("load state file failed %s", firstFile);
        return false;
      }

      bool sizeIsLarge{};
      const auto checksumResult = calcCRC(file, position, sizeIsLarge);
      if (checksumResult == expectedChecksum) {
        logger_->log_debug("Checksum for '%s' matched expected checksum. Will skip first %" PRIu64 " bytes", firstFile.c_str(), position);

        uint64_t fileSize{};
        uint64_t fileLastModifiedTime{};
        if (!getFileSizeLastModifiedTime(firstFile, fileSize, fileLastModifiedTime)) {
          logger_->log_error("'stat' file %s", firstFile.c_str());
          return {};
        }

        // This is the same file that we were reading when we shutdown. Start reading from this point on.
        rolledOffFiles.erase(rolledOffFiles.begin(), rolledOffFiles.begin() + 1);
        auto flowFile = session.create();
        session.import(firstFile, flowFile, true, 0);
        if (!flowFile->getSize()) {
          session.remove(flowFile);
          cleanup();

          // Use a timestamp of lastModified + 1 so that we do not ingest this file again.
          tfo->setState(TailFileState(tailFile, "", 0, fileLastModifiedTime + 1, fileSize, 0));
        } else {
          session.putAttribute(flowFile, FlowAttributeKey(FILENAME), firstFile);
          session.putAttribute(flowFile, FlowAttributeKey(MIME_TYPE), "text/plain");
          session.putAttribute(flowFile, "tailfile.original.path", tailFile);

          session.getProvenanceReporter()->receive(flowFile, "file:/" + firstFile, getUUIDStr(), "FlowFile contains bytes " + std::to_string(fileSize), getTimeMillis() - startTime);
          session.transfer(flowFile, Success);
          logger_->log_debug("Created flowFile from rolled over file '%s' and routed to success", firstFile.c_str());

          cleanup();

          // Use a timestamp of lastModified() + 1 so that we do not ingest this file again.
          tfo->setState(TailFileState(tailFile, "", 0, fileLastModifiedTime + 1, fileSize, 0));

          // must ensure that we do session.commit() before persisting state in order to avoid data loss.
          session.commit();
          persistState(tfo, context);
        }
      } else {
        logger_->log_debug(
          "Checksum for '%s' did not match expected checksum. Checksum for file was %" PRIu64 "but expected %" PRIu64 ". Will consume entire file.",
          firstFile.c_str(), checksumResult, expectedChecksum);
      }
    }
  } 

  // For each file that we found that matches our Rollover Pattern, and has a last modified date later than the timestamp
  // that we recovered from the state file, we need to consume the entire file. The only exception to this is the file that
  // we were reading when we last stopped, as it may already have been partially consumed. That is taken care of in the above block of code.
  for (const auto& file: rolledOffFiles) {
    tfo->setState(consumeFileFully(file, context, session, tfo));
  }

  return true;
}

// Creates a new FlowFile that contains the entire contents of the given file and transfers that FlowFile to success.
// This method will commit the given session and emit an appropriate Provenance Event.
TailFileState TailFile::consumeFileFully(const std::string& filename, core::ProcessContext& context, core::ProcessSession& session, const std::shared_ptr<TailFileObject>& tfo) {
  const auto startTime = getTimeMillis();

  auto flowFile = session.create();
  session.import(filename, flowFile, true, 0);

  if (!flowFile->getSize()) {
    session.remove(flowFile);
  } else {
    session.putAttribute(flowFile, FlowAttributeKey(FILENAME), filename);
    session.putAttribute(flowFile, FlowAttributeKey(MIME_TYPE), "text/plain");
    session.putAttribute(flowFile, "tailfile.original.path", tfo->getState().filename());

    session.getProvenanceReporter()->receive(flowFile, "file:/" + filename, getUUIDStr(), "FlowFile contains bytes " + std::to_string(flowFile->getSize()), getTimeMillis() - startTime);
    session.transfer(flowFile, Success);

    logger_->log_debug("Created flowFile from '%s'and routed to success", filename.c_str());

    cleanup();

    uint64_t fileSize{};
    uint64_t fileLastModifiedTime{};
    if (!getFileSizeLastModifiedTime(filename, fileSize, fileLastModifiedTime)) {
      logger_->log_error("'stat' file %s", filename.c_str());
      return {};
    }

    // Use a timestamp of lastModified() + 1 so that we do not ingest this file again.
    tfo->setState(TailFileState(fileName_, "", 0, fileLastModifiedTime + 1, fileSize, 0));

    // Must ensure that we do session.commit() before persisting state in order to avoid data loss.
    session.commit();
    persistState(tfo, context);
  }

  return tfo->getState();
}

uint64_t TailFile::calcCRC(std::ifstream& reader, uint64_t size, bool& sizeIsLarge) {
  sizeIsLarge = false;

  io::BaseStream base;
  io::CRCStream<io::BaseStream> streamCRC(&base);

  uint64_t count{};

  std::vector<char> buf(10240);
  bool allDataRead = false;
  do {
    if (count + buf.size() > size) {
      allDataRead = true;
      buf.resize(size - count);
    }

    if (sizeIsLarge = !reader.read(buf.data(), buf.size())) {
      buf.resize(reader.gcount());
    }

    count += buf.size();

    streamCRC.writeData(reinterpret_cast<uint8_t*>(buf.data()), buf.size());
  } while (!sizeIsLarge && !allDataRead);
}

bool TailFile::getFileSizeLastModifiedTime(const std::string& filename, uint64_t& size, uint64_t& lastModifiedTime) {
  struct stat fileInfo;
  if (stat(filename.c_str(), &fileInfo)) {
    size = 0;
    lastModifiedTime = 0;
    return false;
  }

  size = fileInfo.st_size;
  lastModifiedTime = fileInfo.st_mtime;
  return true;
}

bool TailFile::getFileSize(const std::string& filename, uint64_t& size) {
  struct stat fileInfo;
  if (stat(filename.c_str(), &fileInfo)) {
    size = 0;
    return false;
  }

  size = fileInfo.st_size;
  return true;
}

std::unordered_map<std::string, std::string>& TailFile::getStateMap() {
  return stateMap_;
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
