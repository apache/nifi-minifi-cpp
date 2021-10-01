/**
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

#include <sstream>
#include <string>
#include <chrono>
#include <optional>
#include "rapidjson/document.h"

namespace org::apache::nifi::minifi::procfs {

class DiskStatData {
  DiskStatData() = default;

 public:
  struct MonotonicIncreasingMembers {
    uint64_t reads_completed_;
    uint64_t reads_merged_;
    uint64_t sectors_read_;
    uint64_t milliseconds_spent_reading_;
    uint64_t writes_completed_;
    uint64_t writes_merges_;
    uint64_t sectors_written_;
    uint64_t milliseconds_spent_writing_;
    uint64_t milliseconds_spent_io_;
    uint64_t weighted_milliseconds_spent_io_;

    bool operator>=(const MonotonicIncreasingMembers& rhs) const {
      return reads_completed_ >= rhs.reads_completed_
             && reads_merged_ >= rhs.reads_merged_
             && sectors_read_ >= rhs.sectors_read_
             && milliseconds_spent_reading_ >= rhs.milliseconds_spent_reading_
             && writes_completed_ >= rhs.writes_completed_
             && writes_merges_ >= rhs.writes_merges_
             && sectors_written_ >= rhs.sectors_written_
             && milliseconds_spent_writing_ >= rhs.milliseconds_spent_writing_
             && milliseconds_spent_io_ >= rhs.milliseconds_spent_io_
             && weighted_milliseconds_spent_io_ >= rhs.weighted_milliseconds_spent_io_;
    }
  };
  static std::optional<DiskStatData> parseDiskStatLine(std::istringstream& iss) {
    DiskStatData disk_stat_data;
    iss >> disk_stat_data.major_device_number_ >> disk_stat_data.minor_device_number_ >> disk_stat_data.disk_name >> disk_stat_data.monotonic_increasing_members_.reads_completed_
        >> disk_stat_data.monotonic_increasing_members_.reads_merged_ >> disk_stat_data.monotonic_increasing_members_.sectors_read_
        >> disk_stat_data.monotonic_increasing_members_.milliseconds_spent_reading_ >> disk_stat_data.monotonic_increasing_members_.writes_completed_
        >> disk_stat_data.monotonic_increasing_members_.writes_merges_ >> disk_stat_data.monotonic_increasing_members_.sectors_written_
        >> disk_stat_data.monotonic_increasing_members_.milliseconds_spent_reading_ >> disk_stat_data.monotonic_increasing_members_.writes_completed_
        >> disk_stat_data.monotonic_increasing_members_.writes_merges_ >> disk_stat_data.monotonic_increasing_members_.sectors_written_
        >> disk_stat_data.monotonic_increasing_members_.milliseconds_spent_writing_ >> disk_stat_data.ios_in_progress_ >> disk_stat_data.ios_in_progress_
        >> disk_stat_data.monotonic_increasing_members_.milliseconds_spent_io_ >> disk_stat_data.monotonic_increasing_members_.weighted_milliseconds_spent_io_;

    if (iss.fail())
      return std::nullopt;
    return disk_stat_data;
  }

  uint64_t getMajorDeviceNumber() const { return major_device_number_; }
  uint64_t getMinorDeviceNumber() const { return minor_device_number_; }
  const std::string& getDiskName() const { return disk_name; }
  uint64_t getReadsCompleted() const { return monotonic_increasing_members_.reads_completed_; }
  uint64_t getReadsMerged() const { return monotonic_increasing_members_.reads_merged_; }
  uint64_t getSectorsRead() const { return monotonic_increasing_members_.sectors_read_; }
  uint64_t getMillisecondsSpentReading() const { return monotonic_increasing_members_.milliseconds_spent_reading_; }
  uint64_t getWritesCompleted() const { return monotonic_increasing_members_.writes_completed_; }
  uint64_t getWritesMerged() const { return monotonic_increasing_members_.writes_merges_; }
  uint64_t getSectorsWritten() const { return monotonic_increasing_members_.sectors_written_; }
  uint64_t getMillisecondsSpentWriting() const { return monotonic_increasing_members_.milliseconds_spent_writing_; }
  uint64_t getIosInProgress() const { return ios_in_progress_; }
  uint64_t getMillisecondsSpentIo() const { return monotonic_increasing_members_.milliseconds_spent_io_; }
  uint64_t getWeightedMillisecondsSpentIo() const { return monotonic_increasing_members_.weighted_milliseconds_spent_io_; }

  const MonotonicIncreasingMembers& getMonotonicIncreasingMembers() const { return monotonic_increasing_members_; }

 private:
  uint64_t major_device_number_;
  uint64_t minor_device_number_;
  std::string disk_name;
  MonotonicIncreasingMembers monotonic_increasing_members_;
  uint64_t ios_in_progress_;
};

class DiskStat {
 public:
  static constexpr const char DEVICE_MAJOR_NUMBER_STR[] = "Major Device Number";
  static constexpr const char DEVICE_MINOR_NUMBER_STR[] = "Minor Device Number";
  static constexpr const char READS_COMPLETED_STR[] = "Reads Completed";
  static constexpr const char READS_MERGED_STR[] = "Reads Merged";
  static constexpr const char SECTORS_READ_STR[] = "Sectors Read";
  static constexpr const char WRITES_COMPLETED_STR[] = "Writes Completed";
  static constexpr const char WRITES_MERGED_STR[] = "Writes Merged";
  static constexpr const char SECTORS_WRITTEN_STR[] = "Sectors Written";
  static constexpr const char IOS_IN_PROGRESS_STR[] = "IO-s in progress";

  DiskStat(DiskStatData disk_stat_data, std::chrono::steady_clock::time_point time_point) : data_(disk_stat_data), time_point_(time_point) {}
  void addToJson(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) const;

  const DiskStatData& getData() const { return data_; }
  const std::chrono::steady_clock::time_point& getTimePoint() const { return time_point_; }
 private:
  DiskStatData data_;
  std::chrono::steady_clock::time_point time_point_;
};


class DiskStatPeriod {
  DiskStatPeriod() = default;

 public:
  static constexpr const char DEVICE_MAJOR_NUMBER_STR[] = "Major Device Number/sec";
  static constexpr const char DEVICE_MINOR_NUMBER_STR[] = "Minor Device Number/sec";
  static constexpr const char READS_COMPLETED_PER_SEC_STR[] = "Reads Completed/sec";
  static constexpr const char READS_MERGED_PER_SEC_STR[] = "Reads Merged/sec";
  static constexpr const char SECTORS_READ_PER_SEC_STR[] = "Sectors Read/sec";
  static constexpr const char WRITES_COMPLETED_PER_SEC_STR[] = "Writes Completed/sec";
  static constexpr const char WRITES_MERGED_PER_SEC_STR[] = "Writes Merged/sec";
  static constexpr const char SECTORS_WRITTEN_PER_SEC_STR[] = "Sectors Written/sec";
  static constexpr const char IOS_IN_PROGRESS_PER_SEC_STR[] = "IO-s in progress";

  static std::optional<DiskStatPeriod> create(const DiskStat& disk_stat_start, const DiskStat& disk_stat_end);

  void addToJson(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) const;

 private:
  uint64_t major_device_number_;
  uint64_t minor_device_number_;
  std::string disk_name_;
  uint64_t reads_completed_;
  uint64_t reads_merged_;
  uint64_t sectors_read_;
  uint64_t writes_completed_;
  uint64_t writes_merges_;
  uint64_t sectors_written_;
  uint64_t ios_in_progress_;
  std::chrono::duration<double> time_duration_;
};
}  // namespace org::apache::nifi::minifi::procfs
