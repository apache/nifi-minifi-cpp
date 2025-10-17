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
#include <cstdint>
#include <concepts>
#include <functional>
#include <string_view>

#include "CpuStat.h"
#include "DiskStat.h"
#include "MemInfo.h"
#include "NetDev.h"
#include "ProcessStat.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::extensions::procfs {

template<class Serializer>
requires std::invocable<Serializer, const char*, uint64_t>
void SerializeCPUStatData(const CpuStatData& cpu_stat_data,
                          Serializer serializer) {
  std::invoke(serializer, "user time", cpu_stat_data.getUser().count());
  std::invoke(serializer, "nice time", cpu_stat_data.getNice().count());
  std::invoke(serializer, "system time", cpu_stat_data.getSystem().count());
  std::invoke(serializer, "idle time", cpu_stat_data.getIdle().count());
  std::invoke(serializer, "io wait time", cpu_stat_data.getIoWait().count());
  std::invoke(serializer, "irq time", cpu_stat_data.getIrq().count());
  std::invoke(serializer, "soft irq time", cpu_stat_data.getSoftIrq().count());
  std::invoke(serializer, "steal time", cpu_stat_data.getSteal().count());
  std::invoke(serializer, "guest time", cpu_stat_data.getGuest().count());
  std::invoke(serializer, "guest nice time", cpu_stat_data.getGuestNice().count());
}

template<class Serializer>
requires std::invocable<Serializer, const char*, double>
void SerializeNormalizedCPUStat(const CpuStatData& cpu_stat_data,
                                Serializer serializer) {
  gsl_Expects(cpu_stat_data.getTotal() > 0ms);
  std::invoke(serializer, "user time %", cpu_stat_data.getUser()/cpu_stat_data.getTotal());
  std::invoke(serializer, "nice time %", cpu_stat_data.getNice()/cpu_stat_data.getTotal());
  std::invoke(serializer, "system time %", cpu_stat_data.getSystem()/cpu_stat_data.getTotal());
  std::invoke(serializer, "idle time %", cpu_stat_data.getIdle()/cpu_stat_data.getTotal());
  std::invoke(serializer, "io wait time %", cpu_stat_data.getIoWait()/cpu_stat_data.getTotal());
  std::invoke(serializer, "irq time %", cpu_stat_data.getIrq()/cpu_stat_data.getTotal());
  std::invoke(serializer, "soft irq %", cpu_stat_data.getSoftIrq()/cpu_stat_data.getTotal());
  std::invoke(serializer, "steal time %", cpu_stat_data.getSteal()/cpu_stat_data.getTotal());
  std::invoke(serializer, "guest time %", cpu_stat_data.getGuest()/cpu_stat_data.getTotal());
  std::invoke(serializer, "guest nice time %", cpu_stat_data.getGuestNice()/cpu_stat_data.getTotal());
}

template<class Serializer>
requires std::invocable<Serializer, const char*, uint64_t>
void SerializeDiskStatData(const DiskStatData& disk_stat_data,
                           Serializer serializer) {
  std::invoke(serializer, "Major Device Number", disk_stat_data.getMajorDeviceNumber());
  std::invoke(serializer, "Minor Device Number", disk_stat_data.getMinorDeviceNumber());
  std::invoke(serializer, "Reads Completed", disk_stat_data.getReadsCompleted());
  std::invoke(serializer, "Reads Merged", disk_stat_data.getReadsMerged());
  std::invoke(serializer, "Sectors Read", disk_stat_data.getSectorsRead());
  std::invoke(serializer, "Writes Completed", disk_stat_data.getWritesCompleted());
  std::invoke(serializer, "Writes Merged", disk_stat_data.getWritesMerged());
  std::invoke(serializer, "Sectors Written", disk_stat_data.getSectorsWritten());
  std::invoke(serializer, "IOs in progress", disk_stat_data.getIosInProgress());
}

template<class Serializer>
requires std::invocable<Serializer, const char*, double>
void SerializeDiskStatDataPerSec(const DiskStatData& disk_stat_data,
                                 const std::chrono::duration<double> duration,
                                 Serializer serializer) {
  gsl_Expects(duration > 0ms);
  std::invoke(serializer, "Major Device Number", disk_stat_data.getMajorDeviceNumber());
  std::invoke(serializer, "Minor Device Number", disk_stat_data.getMinorDeviceNumber());
  std::invoke(serializer, "Reads Completed/sec", disk_stat_data.getReadsCompleted()/duration.count());
  std::invoke(serializer, "Reads Merged/sec", disk_stat_data.getReadsMerged()/duration.count());
  std::invoke(serializer, "Sectors Read/sec", disk_stat_data.getSectorsRead()/duration.count());
  std::invoke(serializer, "Writes Completed/sec", disk_stat_data.getWritesCompleted()/duration.count());
  std::invoke(serializer, "Writes Merged/sec", disk_stat_data.getWritesMerged()/duration.count());
  std::invoke(serializer, "Sectors Written/sec", disk_stat_data.getSectorsWritten()/duration.count());
  std::invoke(serializer, "IOs in progress", disk_stat_data.getIosInProgress()/duration.count());
}

template<class Serializer>
requires std::invocable<Serializer, const char*, uint64_t>
void SerializeMemInfo(const MemInfo& mem_info,
                      Serializer serializer) {
  std::invoke(serializer, "MemTotal", mem_info.getTotalMemory());
  std::invoke(serializer, "MemFree", mem_info.getFreeMemory());
  std::invoke(serializer, "MemAvailable", mem_info.getAvailableMemory());
  std::invoke(serializer, "SwapTotal", mem_info.getTotalSwap());
  std::invoke(serializer, "SwapFree", mem_info.getFreeSwap());
}

template<class Serializer>
requires std::invocable<Serializer, const char*, uint64_t>
void SerializeNetDevData(const NetDevData& net_dev_data,
                         Serializer serializer) {
  std::invoke(serializer, "Bytes Received", net_dev_data.getBytesReceived());
  std::invoke(serializer, "Packets Received", net_dev_data.getPacketsReceived());
  std::invoke(serializer, "Receive Errors", net_dev_data.getReceiveErrors());
  std::invoke(serializer, "Receive Drop Errors", net_dev_data.getReceiveDropErrors());
  std::invoke(serializer, "Receive Fifo Errors", net_dev_data.getReceiveFifoErrors());
  std::invoke(serializer, "Receive Frame Errors", net_dev_data.getReceiveFrameErrors());
  std::invoke(serializer, "Compressed Packets Received", net_dev_data.getCompressedPacketsReceived());
  std::invoke(serializer, "Multicast Frames Received", net_dev_data.getMulticastFramesReceived());

  std::invoke(serializer, "Bytes Transmitted", net_dev_data.getBytesTransmitted());
  std::invoke(serializer, "Packets Transmitted", net_dev_data.getPacketsTransmitted());
  std::invoke(serializer, "Transmit errors", net_dev_data.getTransmitErrors());
  std::invoke(serializer, "Transmit drop errors", net_dev_data.getTransmitDropErrors());
  std::invoke(serializer, "Transmit fifo errors", net_dev_data.getTransmitFifoErrors());
  std::invoke(serializer, "Transmit collisions", net_dev_data.getTransmitCollisions());
  std::invoke(serializer, "Transmit carrier losses", net_dev_data.getTransmitCarrierLosses());
  std::invoke(serializer, "Compressed Packets Transmitted", net_dev_data.getCompressedPacketsTransmitted());
}

template<class Serializer>
requires std::invocable<Serializer, const char*, double>
void SerializeNetDevDataPerSec(const NetDevData& net_dev_data,
                               const std::chrono::duration<double> duration,
                               Serializer serializer) {
  gsl_Expects(duration > 0ms);
  std::invoke(serializer, "Bytes Received/sec", net_dev_data.getBytesReceived()/duration.count());
  std::invoke(serializer, "Packets Received/sec", net_dev_data.getPacketsReceived()/duration.count());
  std::invoke(serializer, "Receive Errors/sec", net_dev_data.getReceiveErrors()/duration.count());
  std::invoke(serializer, "Receive Drop Errors/sec", net_dev_data.getReceiveDropErrors()/duration.count());
  std::invoke(serializer, "Receive Fifo Errors/sec", net_dev_data.getReceiveFifoErrors()/duration.count());
  std::invoke(serializer, "Receive Frame Errors/sec", net_dev_data.getReceiveFrameErrors()/duration.count());
  std::invoke(serializer, "Compressed Packets Received/sec", net_dev_data.getCompressedPacketsReceived()/duration.count());
  std::invoke(serializer, "Multicast Frames Received/sec", net_dev_data.getMulticastFramesReceived()/duration.count());

  std::invoke(serializer, "Bytes Transmitted/sec", net_dev_data.getBytesTransmitted()/duration.count());
  std::invoke(serializer, "Packets Transmitted/sec", net_dev_data.getPacketsTransmitted()/duration.count());
  std::invoke(serializer, "Transmit errors/sec", net_dev_data.getTransmitErrors()/duration.count());
  std::invoke(serializer, "Transmit drop errors/sec", net_dev_data.getTransmitDropErrors()/duration.count());
  std::invoke(serializer, "Transmit fifo errors/sec", net_dev_data.getTransmitFifoErrors()/duration.count());
  std::invoke(serializer, "Transmit collisions/sec", net_dev_data.getTransmitCollisions()/duration.count());
  std::invoke(serializer, "Transmit carrier losses/sec", net_dev_data.getTransmitCarrierLosses()/duration.count());
  std::invoke(serializer, "Compressed Packets Transmitted/sec", net_dev_data.getCompressedPacketsTransmitted()/duration.count());
}

template<class Serializer>
requires std::invocable<Serializer, const  char*, uint64_t> &&
         std::invocable<Serializer, const char*, std::string_view>
void SerializeProcessStat(const ProcessStat& process_stat,
                          Serializer serializer) {
  std::invoke(serializer, "COMM", process_stat.getComm());
  std::invoke(serializer, "RES", process_stat.getMemory());
  std::invoke(serializer, "CPUTIME", process_stat.getCpuTime().count());
}

template<class Serializer>
requires std::invocable<Serializer, const char*, double> &&
         std::invocable<Serializer, const char*, uint64_t> &&
         std::invocable<Serializer, const char*, std::string_view>
void SerializeNormalizedProcessStat(const ProcessStat& process_stat_start,
                                    const ProcessStat& process_stat_end,
                                    const std::chrono::duration<double> all_cpu_time,
                                    Serializer serializer) {
  gsl_Expects(all_cpu_time > 0ms);
  gsl_Expects(process_stat_start.getComm() == process_stat_end.getComm());
  gsl_Expects(process_stat_end.getCpuTime() >= process_stat_start.getCpuTime());
  auto cpu_time_diff = process_stat_end.getCpuTime()-process_stat_start.getCpuTime();
  std::invoke(serializer, "COMM", process_stat_start.getComm());
  std::invoke(serializer, "RES", process_stat_end.getMemory());
  std::invoke(serializer, "CPU%", 100*(cpu_time_diff/all_cpu_time));
}

}  // namespace org::apache::nifi::minifi::extensions::procfs
