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

#include "DiskStat.h"
#include <utility>
#include "minifi-cpp/utils/gsl.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::extensions::procfs {

DiskStatData::MonotonicIncreasingMembers DiskStatData::MonotonicIncreasingMembers::operator-(const MonotonicIncreasingMembers& rhs) const {
  return MonotonicIncreasingMembers {
    .reads_completed_ = reads_completed_ - rhs.reads_completed_,
    .reads_merged_ = reads_merged_ - rhs.reads_merged_,
    .sectors_read_ = sectors_read_ - rhs.sectors_read_,
    .milliseconds_spent_reading_ = milliseconds_spent_reading_ - rhs.milliseconds_spent_reading_,
    .writes_completed_ = writes_completed_ - rhs.writes_completed_,
    .writes_merged_ = writes_merged_ - rhs.writes_merged_,
    .sectors_written_ = sectors_written_ - rhs.sectors_written_,
    .milliseconds_spent_writing_ = milliseconds_spent_writing_ - rhs.milliseconds_spent_writing_,
    .milliseconds_spent_io_ = milliseconds_spent_io_ - rhs.milliseconds_spent_io_,
    .weighted_milliseconds_spent_io_ = weighted_milliseconds_spent_io_ - rhs.weighted_milliseconds_spent_io_
  };
}

std::optional<std::pair<std::string, DiskStatData>> DiskStatData::parseDiskStatLine(std::istream& iss) {
  DiskStatData disk_stat_data{};
  std::string disk_name;
  iss >> disk_stat_data.major_device_number_
      >> disk_stat_data.minor_device_number_
      >> disk_name
      >> disk_stat_data.monotonic_increasing_members_.reads_completed_
      >> disk_stat_data.monotonic_increasing_members_.reads_merged_
      >> disk_stat_data.monotonic_increasing_members_.sectors_read_
      >> disk_stat_data.monotonic_increasing_members_.milliseconds_spent_reading_
      >> disk_stat_data.monotonic_increasing_members_.writes_completed_
      >> disk_stat_data.monotonic_increasing_members_.writes_merged_
      >> disk_stat_data.monotonic_increasing_members_.sectors_written_
      >> disk_stat_data.monotonic_increasing_members_.milliseconds_spent_writing_
      >> disk_stat_data.ios_in_progress_
      >> disk_stat_data.monotonic_increasing_members_.milliseconds_spent_io_
      >> disk_stat_data.monotonic_increasing_members_.weighted_milliseconds_spent_io_;

  if (iss.fail())
    return std::nullopt;
  return std::make_pair(disk_name, disk_stat_data);
}

DiskStatData DiskStatData::operator-(const DiskStatData& rhs) const {
  DiskStatData result{};
  gsl_Expects(major_device_number_ == rhs.major_device_number_);
  gsl_Expects(minor_device_number_ == rhs.minor_device_number_);
  gsl_Expects(monotonic_increasing_members_ >= rhs.monotonic_increasing_members_);
  result.major_device_number_ = major_device_number_;
  result.minor_device_number_ = minor_device_number_;
  result.monotonic_increasing_members_ = monotonic_increasing_members_ - rhs.monotonic_increasing_members_;
  result.ios_in_progress_ = ios_in_progress_;
  return result;
}

}  // namespace org::apache::nifi::minifi::extensions::procfs
