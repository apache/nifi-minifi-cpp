/**
 * @file TailFile.h
 * TailFile class declaration
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

#pragma once

#include <unordered_map>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"

#include "core/Core.h"
#include "core/Resource.h"
#include "core/logging/LoggerConfiguration.h"
#include "io/CRCStream.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class TailFileState;
class TailFileObject;

class TailFile: public core::Processor {
 public:
  explicit TailFile(std::string name, utils::Identifier uuid = utils::Identifier())
    : core::Processor(name, uuid), logger_(logging::LoggerFactory<TailFile>::getLogger()) {
  }
  virtual ~TailFile() {}
  // Processor Name
  static constexpr char const* ProcessorName = "TailFile";
  // Supported Properties
  static core::Property BaseDirectory;
  static core::Property Mode;
  static core::Property FileName;
  static core::Property RollingFileNamePattern;
  static core::Property StartPosition;
  static core::Property Recursive;
  static core::Property LookupFrequency;
  static core::Property MaximumAge;
  // Supported Relationships
  static core::Relationship Success;

 public:

  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession>  &session) override;
  void initialize() override;

  void recoverState(core::ProcessContext& context);
  void recoverState(core::ProcessContext& context, const std::vector<std::string>& filesToTail, std::unordered_map<std::string, std::string>& map);
  void recoverState(core::ProcessContext& context, const std::unordered_map<std::string, std::string>& stateValues, const std::string& filePath);
  std::vector<std::string> lookup(core::ProcessContext& context);
  std::vector<std::string> TailFile::getFilesToTail(const std::string& baseDir, const std::string& fileRegex, bool isRecursive, uint64_t maxAge);
  void resetState(const std::string& filePath);
  void cleanup();
  void initStates(const std::vector<std::string>& filesToTail, const std::unordered_map<std::string, std::string>& statesMap, bool isCleared, const std::string& startPosition);
  void processTailFile(core::ProcessContext& context, core::ProcessSession& session, const std::string& tailFile);
  bool createReader(std::ifstream& reader, const std::string& filename, uint64_t position = 0);
  bool getReaderSizePosition(uint64_t& size, uint64_t& position, std::ifstream& reader, const std::string& filename);
  void persistState(const std::shared_ptr<TailFileObject>& tailFileObject, core::ProcessContext& context);
  bool recoverRolledFiles(core::ProcessContext& context, core::ProcessSession& session, const std::string& tailFile, uint64_t expectedChecksum, uint64_t timestamp, uint64_t position);
  bool recoverRolledFiles(
    core::ProcessContext& context, core::ProcessSession& session, const std::string& tailFile, std::vector<std::string> rolledOffFiles, uint64_t expectedChecksum, uint64_t timestamp, uint64_t position);
  std::vector<std::string> getRolledOffFiles(core::ProcessContext& context, uint64_t minTimestamp, const std::string& tailFilePath);
  uint64_t calcCRC(std::ifstream& reader, uint64_t size, bool& sizeIsLarge);
  TailFileState consumeFileFully(const std::string& filename, core::ProcessContext& context, core::ProcessSession& session, const std::shared_ptr<TailFileObject>& tailFileObject);
  bool getFileSizeLastModifiedTime(const std::string& filename, uint64_t& size, uint64_t& lastModifiedTime);
  bool getFileSize(const std::string& filename, uint64_t& size);

  // Will be substituted by RocksDB controller service.
  std::unordered_map<std::string, std::string>& getStateMap();
 private:
  // Used by 'getStateMap()' for testing until RocksDB controller service is used instead of 'getStateMap'.
  std::unordered_map<std::string, std::string> stateMap_;

  std::unordered_map<std::string, std::shared_ptr<TailFileObject>> states_;
  std::mutex onTriggerMutex_;
  std::string baseDirectory_;
  std::string fileName_;
  bool recursive_{};
  bool isMultiChanging_{};
  uint64_t maxAge_{};
  uint64_t lastLookup_{};
  std::string startPosition_;
  std::string rollingFileNamePattern_;
  uint64_t lookupFrequency_{};
  std::shared_ptr<logging::Logger> logger_;
  bool stateRecovered_{};
};

REGISTER_RESOURCE(TailFile, "\"Tails\" a file, or a list of files, ingesting data from the file as it is written to the file. The file is expected to be textual."
                  " Data is ingested only when a new line is encountered (carriage return or new-line character or combination). If the file to tail is periodically \"rolled over\","
                  " as is generally the case with log files, an optional Rolling Filename Pattern can be used to retrieve data from files that have rolled over, even if the rollover"
                  " occurred while NiFi was not running (provided that the data still exists upon restart of NiFi). It is generally advisable to set the Run Schedule to a few seconds,"
                  " rather than running with the default value of 0 secs, as this Processor will consume a lot of resources if scheduled very aggressively. At this time, this Processor"
                  " does not support ingesting files that have been compressed when 'rolled over'.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
