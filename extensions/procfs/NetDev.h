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

#include <string>
#include <utility>
#include <chrono>
#include <sstream>
#include <optional>
#include "rapidjson/document.h"

namespace org::apache::nifi::minifi::procfs {

class NetDevData {
  NetDevData() = default;

 public:
  NetDevData(const NetDevData& src) = default;
  NetDevData(NetDevData&& src) noexcept = default;
  static std::optional<NetDevData> parseNetDevLine(std::istringstream& iss);

  bool operator>=(const NetDevData& rhs) const;
  NetDevData operator-(const NetDevData& rhs) const;

  uint64_t getBytesReceived() const { return bytes_received_; }
  uint64_t getPacketsReceived() const { return packets_received_; }
  uint64_t getReceiveErrors() const { return errs_received_; }
  uint64_t getReceiveDropErrors() const { return drop_errors_received_; }
  uint64_t getReceiveFifoErrors() const { return fifo_errors_received_; }
  uint64_t getReceiveFrameErrors() const { return frame_errors_received_; }
  uint64_t getCompressedPacketsReceived() const { return compressed_packets_received_; }
  uint64_t getMulticastFramesReceived() const { return multicast_frames_received_; }
  uint64_t getBytesTransmitted() const { return bytes_transmitted_; }
  uint64_t getPacketsTransmitted() const { return packets_transmitted_; }
  uint64_t getTransmitErrors() const { return errs_transmitted_; }
  uint64_t getTransmitDropErrors() const { return drop_errors_transmitted_; }
  uint64_t getTransmitFifoErrors() const { return fifo_errors_transmitted_; }
  uint64_t getTransmitCollisions() const { return collisions_transmitted_; }
  uint64_t getTransmitCarrierLosses() const { return carrier_losses_transmitted_; }
  uint64_t getCompressedPacketsTransmitted() const { return compressed_packets_transmitted_; }

 private:
  uint64_t bytes_received_;
  uint64_t packets_received_;
  uint64_t errs_received_;
  uint64_t drop_errors_received_;
  uint64_t fifo_errors_received_;
  uint64_t frame_errors_received_;
  uint64_t compressed_packets_received_;
  uint64_t multicast_frames_received_;

  uint64_t bytes_transmitted_;
  uint64_t packets_transmitted_;
  uint64_t errs_transmitted_;
  uint64_t drop_errors_transmitted_;
  uint64_t fifo_errors_transmitted_;
  uint64_t collisions_transmitted_;
  uint64_t carrier_losses_transmitted_;
  uint64_t compressed_packets_transmitted_;
};

class NetDev {
 public:
  static constexpr char BYTES_RECEIVED_STR[] = "Bytes Received";
  static constexpr char PACKETS_RECEIVED_STR[] = "Packets Received";
  static constexpr char RECEIVE_ERRORS_STR[] = "Receive Errors";
  static constexpr char RECEIVE_DROP_ERRORS_STR[] = "Receive Drop Errors";
  static constexpr char RECEIVE_FIFO_ERRORS_STR[] = "Receive Fifo Errors";
  static constexpr char RECEIVE_FRAME_ERRORS_STR[] = "Receive Frame Errors";
  static constexpr char COMPRESSED_PACKETS_RECEIVED_STR[] = "Compressed Packets Received";
  static constexpr char MULTICAST_FRAMES_RECEIVED_STR[] = "Multicast Frames Received";

  static constexpr char BYTES_TRANSMITTED_STR[] = "Bytes Transmitted";
  static constexpr char PACKETS_TRANSMITTED_STR[] = "Packets Transmitted";
  static constexpr char TRANSMIT_ERRORS_STR[] = "Transmit errors";
  static constexpr char TRANSMIT_DROP_ERRORS_STR[] = "Transmit drop errors";
  static constexpr char TRANSMIT_FIFO_ERRORS_STR[] = "Transmit fifo errors";
  static constexpr char TRANSMIT_COLLISIONS_DETECTED_STR[] = "Transmit collisions";
  static constexpr char TRANSMIT_CARRIER_LOSSES_STR[] = "Transmit carrier losses";
  static constexpr char COMPRESSED_PACKETS_TRANSMITTED_STR[] = "Compressed Packets Transmitted";

  NetDev(const NetDev& src) = default;
  NetDev(NetDev&& src) noexcept = default;

  NetDev(std::string interface_name, NetDevData data, std::chrono::steady_clock::time_point time_point)
      : interface_name_(std::move(interface_name)), data_(std::move(data)), time_point_(time_point) {}

  void addToJson(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) const;

  const std::string& getInterfaceName() const { return interface_name_; }
  const NetDevData& getData() const { return data_; }
  const std::chrono::steady_clock::time_point& getTimePoint() const { return time_point_; }

 protected:
  std::string interface_name_;
  NetDevData data_;
  std::chrono::steady_clock::time_point time_point_;
};

class NetDevPeriod {
  NetDevPeriod(std::string interface_name, NetDevData data_diff, std::chrono::duration<double> time_duration)
      : interface_name_(std::move(interface_name)), data_diff_(data_diff), time_duration_(time_duration) {
  }

 public:
  static constexpr const char BYTES_RECEIVED_PER_SEC_STR[] = "Bytes Received/sec";
  static constexpr const char PACKETS_RECEIVED_PER_SEC_STR[] = "Packets Received/sec";
  static constexpr const char RECEIVE_ERRORS_PER_SEC_STR[] = "Receive Errors/sec";
  static constexpr const char RECEIVE_DROP_ERRORS_PER_SEC_STR[] = "Receive Drop Errors/sec";
  static constexpr const char RECEIVE_FIFO_ERRORS_PER_SEC_STR[] = "Receive Fifo Errors/sec";
  static constexpr const char RECEIVE_FRAME_ERRORS_PER_SEC_STR[] = "Receive Frame Errors/sec";
  static constexpr const char COMPRESSED_PACKETS_RECEIVED_PER_SEC_STR[] = "Compressed Packets Received/sec";
  static constexpr const char MULTICAST_FRAMES_RECEIVED_PER_SEC_STR[] = "Multicast Frames Received/sec";

  static constexpr const char BYTES_TRANSMITTED_PER_SEC_STR[] = "Bytes Transmitted/sec";
  static constexpr const char PACKETS_TRANSMITTED_PER_SEC_STR[] = "Packets Transmitted/sec";
  static constexpr const char TRANSMIT_ERRORS_PER_SEC_STR[] = "Transmit Errors/sec";
  static constexpr const char TRANSMIT_DROP_ERRORS_PER_SEC_STR[] = "Transmit Drop Errors/sec";
  static constexpr const char TRANSMIT_FIFO_ERRORS_PER_SEC_STR[] = "Transmit Fifo Errors/sec";
  static constexpr const char TRANSMIT_COLLISIONS_DETECTED_PER_SEC_STR[] = "Transmit Collisions/sec";
  static constexpr const char TRANSMIT_CARRIER_LOSSES_PER_SEC_STR[] = "Transmit Carrier Losses/sec";
  static constexpr const char COMPRESSED_PACKETS_TRANSMITTED_PER_SEC_STR[] = "Compressed Packets Transmitted/sec";


  NetDevPeriod(const NetDevPeriod& src) = default;
  NetDevPeriod(NetDevPeriod&& src) noexcept = default;

  static std::optional<NetDevPeriod> create(const NetDev& net_dev_start, const NetDev& net_dev_end);

  void addToJson(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) const;

  const std::string& getInterfaceName() const { return interface_name_; }

  double getBytesReceivedPerSec() const { return static_cast<double>(data_diff_.getBytesReceived()) / time_duration_.count(); }
  double getPacketsReceivedPerSec() const { return static_cast<double>(data_diff_.getPacketsReceived()) / time_duration_.count(); }
  double getErrsReceivedPerSec() const { return static_cast<double>(data_diff_.getReceiveErrors()) / time_duration_.count(); }
  double getDropErrorsReceivedPerSec() const { return static_cast<double>(data_diff_.getReceiveDropErrors()) / time_duration_.count(); }
  double getFifoErrorsReceivedPerSec() const { return static_cast<double>(data_diff_.getReceiveFifoErrors()) / time_duration_.count(); }
  double getFrameErrorsReceivedPerSec() const { return static_cast<double>(data_diff_.getReceiveFrameErrors()) / time_duration_.count(); }
  double getCompressedPacketsReceivedPerSec() const { return static_cast<double>(data_diff_.getCompressedPacketsReceived()) / time_duration_.count(); }
  double getMulticastFramesReceivedPerSec() const { return static_cast<double>(data_diff_.getMulticastFramesReceived()) / time_duration_.count(); }
  double getBytesTransmittedPerSec() const { return static_cast<double>(data_diff_.getBytesTransmitted()) / time_duration_.count(); }
  double getPacketsTransmittedPerSec() const { return static_cast<double>(data_diff_.getPacketsTransmitted()) / time_duration_.count(); }
  double getErrsTransmittedPerSec() const { return static_cast<double>(data_diff_.getTransmitErrors()) / time_duration_.count(); }
  double getDropErrorsTransmittedPerSec() const { return static_cast<double>(data_diff_.getTransmitDropErrors()) / time_duration_.count(); }
  double getFifoErrorsTransmittedPerSec() const { return static_cast<double>(data_diff_.getTransmitFifoErrors()) / time_duration_.count(); }
  double getCollisionsTransmittedPerSec() const { return static_cast<double>(data_diff_.getTransmitCollisions()) / time_duration_.count(); }
  double getCarrierLossesTransmittedPerSec() const { return static_cast<double>(data_diff_.getTransmitCarrierLosses()) / time_duration_.count(); }
  double getCompressedPacketsTransmittedPerSec() const { return static_cast<double>(data_diff_.getCompressedPacketsTransmitted()) / time_duration_.count(); }

 protected:
  std::string interface_name_;
  NetDevData data_diff_;
  std::chrono::duration<double> time_duration_;
};
}  // namespace org::apache::nifi::minifi::procfs
