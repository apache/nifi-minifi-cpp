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
#include <iostream>

namespace org::apache::nifi::minifi::procfs {

void DiskStat::addToJson(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) const {
  rapidjson::Value disk_name_json(data_.getDiskName().c_str(), data_.getDiskName().length(), alloc);
  body.AddMember(disk_name_json, rapidjson::kObjectType, alloc);
  rapidjson::Value& device_stat_json = body[data_.getDiskName().c_str()];
  device_stat_json.AddMember(DEVICE_MAJOR_NUMBER_STR, data_.getMajorDeviceNumber(), alloc);
  device_stat_json.AddMember(DEVICE_MINOR_NUMBER_STR, data_.getMinorDeviceNumber(), alloc);
  device_stat_json.AddMember(READS_COMPLETED_STR, data_.getReadsCompleted(), alloc);
  device_stat_json.AddMember(READS_MERGED_STR, data_.getReadsMerged(), alloc);
  device_stat_json.AddMember(SECTORS_READ_STR, data_.getSectorsRead(), alloc);
  device_stat_json.AddMember(WRITES_COMPLETED_STR, data_.getWritesCompleted(), alloc);
  device_stat_json.AddMember(WRITES_MERGED_STR, data_.getWritesMerged(), alloc);
  device_stat_json.AddMember(SECTORS_WRITTEN_STR, data_.getSectorsWritten(), alloc);
  device_stat_json.AddMember(IOS_IN_PROGRESS_STR, data_.getIosInProgress(), alloc);
}

std::optional<DiskStatPeriod> DiskStatPeriod::create(const DiskStat& disk_stat_start, const DiskStat& disk_stat_end) {
  if (disk_stat_end.getData().getDiskName() != disk_stat_end.getData().getDiskName())
    return std::nullopt;
  if (disk_stat_end.getData().getMajorDeviceNumber() != disk_stat_start.getData().getMajorDeviceNumber())
    return std::nullopt;
  if (disk_stat_end.getData().getMinorDeviceNumber() != disk_stat_start.getData().getMinorDeviceNumber())
    return std::nullopt;
  if (!(disk_stat_end.getTimePoint() > disk_stat_start.getTimePoint()))
    return std::nullopt;
  if (!(disk_stat_end.getData().getMonotonicIncreasingMembers() >= disk_stat_start.getData().getMonotonicIncreasingMembers()))
    return std::nullopt;

  DiskStatPeriod disk_stat_period;
  disk_stat_period.major_device_number_ = disk_stat_end.getData().getMajorDeviceNumber();
  disk_stat_period.minor_device_number_ = disk_stat_end.getData().getMinorDeviceNumber();
  disk_stat_period.disk_name_ = disk_stat_end.getData().getDiskName();

  disk_stat_period.reads_completed_ = disk_stat_end.getData().getReadsCompleted() - disk_stat_start.getData().getReadsCompleted();
  disk_stat_period.reads_merged_ = disk_stat_end.getData().getReadsMerged() - disk_stat_start.getData().getReadsMerged();
  disk_stat_period.sectors_read_ = disk_stat_end.getData().getSectorsRead() - disk_stat_start.getData().getSectorsRead();
  disk_stat_period.writes_completed_ = disk_stat_end.getData().getWritesCompleted() - disk_stat_start.getData().getWritesCompleted();
  disk_stat_period.writes_merges_ = disk_stat_end.getData().getWritesMerged() - disk_stat_start.getData().getWritesMerged();
  disk_stat_period.sectors_written_ = disk_stat_end.getData().getSectorsWritten() - disk_stat_start.getData().getSectorsWritten();

  disk_stat_period.ios_in_progress_ = disk_stat_end.getData().getIosInProgress();
  disk_stat_period.time_duration_ = disk_stat_end.getTimePoint()-disk_stat_start.getTimePoint();

  return disk_stat_period;
}

void DiskStatPeriod::addToJson(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) const {
  rapidjson::Value disk_name_json(disk_name_.c_str(), disk_name_.length(), alloc);
  body.AddMember(disk_name_json, rapidjson::kObjectType, alloc);
  rapidjson::Value& device_stat_json = body[disk_name_.c_str()];
  device_stat_json.AddMember(DEVICE_MAJOR_NUMBER_STR, major_device_number_, alloc);
  device_stat_json.AddMember(DEVICE_MINOR_NUMBER_STR, minor_device_number_, alloc);
  device_stat_json.AddMember(READS_COMPLETED_PER_SEC_STR, reads_completed_/time_duration_.count(), alloc);
  device_stat_json.AddMember(READS_MERGED_PER_SEC_STR, reads_merged_/time_duration_.count(), alloc);
  device_stat_json.AddMember(SECTORS_READ_PER_SEC_STR, sectors_read_/time_duration_.count(), alloc);
  device_stat_json.AddMember(WRITES_COMPLETED_PER_SEC_STR, writes_completed_/time_duration_.count(), alloc);
  device_stat_json.AddMember(WRITES_MERGED_PER_SEC_STR, writes_merges_/time_duration_.count(), alloc);
  device_stat_json.AddMember(SECTORS_WRITTEN_PER_SEC_STR, sectors_written_/time_duration_.count(), alloc);
  device_stat_json.AddMember(IOS_IN_PROGRESS_PER_SEC_STR, ios_in_progress_, alloc);
}

}  // namespace org::apache::nifi::minifi::procfs
