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

#include <chrono>
#include <istream>
#include <optional>
#include <string>
#include <utility>

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::extensions::procfs {

class DiskStatData {
 private:
  DiskStatData() = default;

 public:
  struct MonotonicIncreasingMembers {
    uint64_t reads_completed_;
    uint64_t reads_merged_;
    uint64_t sectors_read_;
    uint64_t milliseconds_spent_reading_;
    uint64_t writes_completed_;
    uint64_t writes_merged_;
    uint64_t sectors_written_;
    uint64_t milliseconds_spent_writing_;
    uint64_t milliseconds_spent_io_;
    uint64_t weighted_milliseconds_spent_io_;

    auto operator<=>(const MonotonicIncreasingMembers&) const = default;
    MonotonicIncreasingMembers operator-(const MonotonicIncreasingMembers& rhs) const;
  };
  static std::optional<std::pair<std::string, DiskStatData>> parseDiskStatLine(std::istream& iss);

  DiskStatData(uint64_t major_device_number, uint64_t minor_device_number, MonotonicIncreasingMembers monotonic_increasing_members, uint64_t ios_in_progress) :
    major_device_number_(major_device_number),
    minor_device_number_(minor_device_number),
    monotonic_increasing_members_(monotonic_increasing_members),
    ios_in_progress_(ios_in_progress) {
  }

  [[nodiscard]] uint64_t getMajorDeviceNumber() const noexcept { return major_device_number_; }
  [[nodiscard]] uint64_t getMinorDeviceNumber() const noexcept { return minor_device_number_; }
  [[nodiscard]] uint64_t getReadsCompleted() const noexcept { return monotonic_increasing_members_.reads_completed_; }
  [[nodiscard]] uint64_t getReadsMerged() const noexcept { return monotonic_increasing_members_.reads_merged_; }
  [[nodiscard]] uint64_t getSectorsRead() const noexcept { return monotonic_increasing_members_.sectors_read_; }
  [[nodiscard]] uint64_t getMillisecondsSpentReading() const noexcept { return monotonic_increasing_members_.milliseconds_spent_reading_; }
  [[nodiscard]] uint64_t getWritesCompleted() const noexcept { return monotonic_increasing_members_.writes_completed_; }
  [[nodiscard]] uint64_t getWritesMerged() const noexcept { return monotonic_increasing_members_.writes_merged_; }
  [[nodiscard]] uint64_t getSectorsWritten() const noexcept { return monotonic_increasing_members_.sectors_written_; }
  [[nodiscard]] uint64_t getMillisecondsSpentWriting() const noexcept { return monotonic_increasing_members_.milliseconds_spent_writing_; }
  [[nodiscard]] uint64_t getIosInProgress() const noexcept { return ios_in_progress_; }
  [[nodiscard]] uint64_t getMillisecondsSpentIo() const noexcept { return monotonic_increasing_members_.milliseconds_spent_io_; }
  [[nodiscard]] uint64_t getWeightedMillisecondsSpentIo() const noexcept { return monotonic_increasing_members_.weighted_milliseconds_spent_io_; }

  [[nodiscard]] const MonotonicIncreasingMembers& getMonotonicIncreasingMembers() const noexcept { return monotonic_increasing_members_; }

  DiskStatData operator-(const DiskStatData& rhs) const;

 private:
  uint64_t major_device_number_;
  uint64_t minor_device_number_;
  MonotonicIncreasingMembers monotonic_increasing_members_;
  uint64_t ios_in_progress_;
};

}  // namespace org::apache::nifi::minifi::extensions::procfs
