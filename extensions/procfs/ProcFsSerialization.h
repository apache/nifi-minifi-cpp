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
#include <stdint.h>
#include <concepts>
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
  serializer("user time", cpu_stat_data.getUser().count());
  serializer("nice time", cpu_stat_data.getNice().count());
  serializer("system time", cpu_stat_data.getSystem().count());
  serializer("idle time", cpu_stat_data.getIdle().count());
  serializer("io wait time", cpu_stat_data.getIoWait().count());
  serializer("irq time", cpu_stat_data.getIrq().count());
  serializer("soft irq time", cpu_stat_data.getSoftIrq().count());
  serializer("steal time", cpu_stat_data.getSteal().count());
  serializer("guest time", cpu_stat_data.getGuest().count());
  serializer("guest nice time", cpu_stat_data.getGuestNice().count());
}

template<class Serializer>
requires std::invocable<Serializer, const char*, double>
void SerializeNormalizedCPUStat(const CpuStatData& cpu_stat_data,
                                Serializer serializer) {
  gsl_Expects(cpu_stat_data.getTotal() > 0ms);
  serializer("user time %", cpu_stat_data.getUser()/cpu_stat_data.getTotal());
  serializer("nice time %", cpu_stat_data.getNice()/cpu_stat_data.getTotal());
  serializer("system time %", cpu_stat_data.getSystem()/cpu_stat_data.getTotal());
  serializer("idle time %", cpu_stat_data.getIdle()/cpu_stat_data.getTotal());
  serializer("io wait time %", cpu_stat_data.getIoWait()/cpu_stat_data.getTotal());
  serializer("irq time %", cpu_stat_data.getIrq()/cpu_stat_data.getTotal());
  serializer("soft irq %", cpu_stat_data.getSoftIrq()/cpu_stat_data.getTotal());
  serializer("steal time %", cpu_stat_data.getSteal()/cpu_stat_data.getTotal());
  serializer("guest time %", cpu_stat_data.getGuest()/cpu_stat_data.getTotal());
  serializer("guest nice time %", cpu_stat_data.getGuestNice()/cpu_stat_data.getTotal());
}

template<class Serializer>
requires std::invocable<Serializer, const char*, uint64_t>
void SerializeDiskStatData(const DiskStatData& disk_stat_data,
                           Serializer serializer) {
  serializer("Major Device Number", disk_stat_data.getMajorDeviceNumber());
  serializer("Minor Device Number", disk_stat_data.getMinorDeviceNumber());
  serializer("Reads Completed", disk_stat_data.getReadsCompleted());
  serializer("Reads Merged", disk_stat_data.getReadsMerged());
  serializer("Sectors Read", disk_stat_data.getSectorsRead());
  serializer("Writes Completed", disk_stat_data.getWritesCompleted());
  serializer("Writes Merged", disk_stat_data.getWritesMerged());
  serializer("Sectors Written", disk_stat_data.getSectorsWritten());
  serializer("IOs in progress", disk_stat_data.getIosInProgress());
}

template<class Serializer>
requires std::invocable<Serializer, const char*, double>
void SerializeDiskStatDataPerSec(const DiskStatData& disk_stat_data,
                                 const std::chrono::duration<double> duration,
                                 Serializer serializer) {
  gsl_Expects(duration > 0ms);
  serializer("Major Device Number", disk_stat_data.getMajorDeviceNumber());
  serializer("Minor Device Number", disk_stat_data.getMinorDeviceNumber());
  serializer("Reads Completed/sec", disk_stat_data.getReadsCompleted()/duration.count());
  serializer("Reads Merged/sec", disk_stat_data.getReadsMerged()/duration.count());
  serializer("Sectors Read/sec", disk_stat_data.getSectorsRead()/duration.count());
  serializer("Writes Completed/sec", disk_stat_data.getWritesCompleted()/duration.count());
  serializer("Writes Merged/sec", disk_stat_data.getWritesMerged()/duration.count());
  serializer("Sectors Written/sec", disk_stat_data.getSectorsWritten()/duration.count());
  serializer("IOs in progress", disk_stat_data.getIosInProgress()/duration.count());
}

template<class Serializer>
requires std::invocable<Serializer, const char*, uint64_t>
void SerializeMemInfo(const MemInfo& mem_info,
                      Serializer serializer) {
  serializer("MemTotal", mem_info.getTotalMemory());
  serializer("MemFree", mem_info.getFreeMemory());
  serializer("MemAvailable", mem_info.getAvailableMemory());
  serializer("SwapTotal", mem_info.getTotalSwap());
  serializer("SwapFree", mem_info.getFreeSwap());
}

template<class Serializer>
requires std::invocable<Serializer, const char*, uint64_t>
void SerializeNetDevData(const NetDevData& net_dev_data,
                         Serializer serializer) {
  serializer("Bytes Received", net_dev_data.getBytesReceived());
  serializer("Packets Received", net_dev_data.getPacketsReceived());
  serializer("Receive Errors", net_dev_data.getReceiveErrors());
  serializer("Receive Drop Errors", net_dev_data.getReceiveDropErrors());
  serializer("Receive Fifo Errors", net_dev_data.getReceiveFifoErrors());
  serializer("Receive Frame Errors", net_dev_data.getReceiveFrameErrors());
  serializer("Compressed Packets Received", net_dev_data.getCompressedPacketsReceived());
  serializer("Multicast Frames Received", net_dev_data.getMulticastFramesReceived());

  serializer("Bytes Transmitted", net_dev_data.getBytesTransmitted());
  serializer("Packets Transmitted", net_dev_data.getPacketsTransmitted());
  serializer("Transmit errors", net_dev_data.getTransmitErrors());
  serializer("Transmit drop errors", net_dev_data.getTransmitDropErrors());
  serializer("Transmit fifo errors", net_dev_data.getTransmitFifoErrors());
  serializer("Transmit collisions", net_dev_data.getTransmitCollisions());
  serializer("Transmit carrier losses", net_dev_data.getTransmitCarrierLosses());
  serializer("Compressed Packets Transmitted", net_dev_data.getCompressedPacketsTransmitted());
}

template<class Serializer>
requires std::invocable<Serializer, const char*, double>
void SerializeNetDevDataPerSec(const NetDevData& net_dev_data,
                               const std::chrono::duration<double> duration,
                               Serializer serializer) {
  gsl_Expects(duration > 0ms);
  serializer("Bytes Received/sec", net_dev_data.getBytesReceived()/duration.count());
  serializer("Packets Received/sec", net_dev_data.getPacketsReceived()/duration.count());
  serializer("Receive Errors/sec", net_dev_data.getReceiveErrors()/duration.count());
  serializer("Receive Drop Errors/sec", net_dev_data.getReceiveDropErrors()/duration.count());
  serializer("Receive Fifo Errors/sec", net_dev_data.getReceiveFifoErrors()/duration.count());
  serializer("Receive Frame Errors/sec", net_dev_data.getReceiveFrameErrors()/duration.count());
  serializer("Compressed Packets Received/sec", net_dev_data.getCompressedPacketsReceived()/duration.count());
  serializer("Multicast Frames Received/sec", net_dev_data.getMulticastFramesReceived()/duration.count());

  serializer("Bytes Transmitted/sec", net_dev_data.getBytesTransmitted()/duration.count());
  serializer("Packets Transmitted/sec", net_dev_data.getPacketsTransmitted()/duration.count());
  serializer("Transmit errors/sec", net_dev_data.getTransmitErrors()/duration.count());
  serializer("Transmit drop errors/sec", net_dev_data.getTransmitDropErrors()/duration.count());
  serializer("Transmit fifo errors/sec", net_dev_data.getTransmitFifoErrors()/duration.count());
  serializer("Transmit collisions/sec", net_dev_data.getTransmitCollisions()/duration.count());
  serializer("Transmit carrier losses/sec", net_dev_data.getTransmitCarrierLosses()/duration.count());
  serializer("Compressed Packets Transmitted/sec", net_dev_data.getCompressedPacketsTransmitted()/duration.count());
}

template<class Serializer>
requires std::invocable<Serializer, const  char*, uint64_t> &&
         std::invocable<Serializer, const char*, std::string_view>
void SerializeProcessStat(const ProcessStat& process_stat,
                          Serializer serializer) {
  serializer("COMM", process_stat.getComm());
  serializer("RES", process_stat.getMemory());
  serializer("CPUTIME", process_stat.getCpuTime().count());
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
  serializer("COMM", process_stat_start.getComm());
  serializer("RES", process_stat_end.getMemory());
  serializer("CPU%", 100*(cpu_time_diff/all_cpu_time));
}

}  // namespace org::apache::nifi::minifi::extensions::procfs
