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
#include <istream>
#include <optional>

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::extensions::procfs {

class NetDevData {
 private:
  NetDevData() = default;

 public:
  static std::optional<std::pair<std::string, NetDevData>> parseNetDevLine(std::istream& iss);

  auto operator<=>(const NetDevData& rhs) const = default;
  NetDevData operator-(const NetDevData& rhs) const;

  [[nodiscard]] uint64_t getBytesReceived() const noexcept  { return bytes_received_; }
  [[nodiscard]] uint64_t getPacketsReceived() const noexcept  { return packets_received_; }
  [[nodiscard]] uint64_t getReceiveErrors() const noexcept  { return errs_received_; }
  [[nodiscard]] uint64_t getReceiveDropErrors() const noexcept  { return drop_errors_received_; }
  [[nodiscard]] uint64_t getReceiveFifoErrors() const noexcept  { return fifo_errors_received_; }
  [[nodiscard]] uint64_t getReceiveFrameErrors() const noexcept  { return frame_errors_received_; }
  [[nodiscard]] uint64_t getCompressedPacketsReceived() const noexcept  { return compressed_packets_received_; }
  [[nodiscard]] uint64_t getMulticastFramesReceived() const noexcept  { return multicast_frames_received_; }
  [[nodiscard]] uint64_t getBytesTransmitted() const noexcept  { return bytes_transmitted_; }
  [[nodiscard]] uint64_t getPacketsTransmitted() const noexcept  { return packets_transmitted_; }
  [[nodiscard]] uint64_t getTransmitErrors() const noexcept  { return errs_transmitted_; }
  [[nodiscard]] uint64_t getTransmitDropErrors() const noexcept  { return drop_errors_transmitted_; }
  [[nodiscard]] uint64_t getTransmitFifoErrors() const noexcept  { return fifo_errors_transmitted_; }
  [[nodiscard]] uint64_t getTransmitCollisions() const noexcept  { return collisions_transmitted_; }
  [[nodiscard]] uint64_t getTransmitCarrierLosses() const noexcept  { return carrier_losses_transmitted_; }
  [[nodiscard]] uint64_t getCompressedPacketsTransmitted() const noexcept  { return compressed_packets_transmitted_; }

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
}  // namespace org::apache::nifi::minifi::extensions::procfs
