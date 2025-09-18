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

#include "NetDev.h"
#include "minifi-cpp/utils/gsl.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::extensions::procfs {

std::optional<std::pair<std::string, NetDevData>> NetDevData::parseNetDevLine(std::istream& iss) {
  NetDevData net_dev_data{};
  std::string entry_name;
  iss >> entry_name
      >> net_dev_data.bytes_received_
      >> net_dev_data.packets_received_
      >> net_dev_data.errs_received_
      >> net_dev_data.drop_errors_received_
      >> net_dev_data.fifo_errors_received_
      >> net_dev_data.frame_errors_received_
      >> net_dev_data.compressed_packets_received_
      >> net_dev_data.multicast_frames_received_
      >> net_dev_data.bytes_transmitted_
      >> net_dev_data.packets_transmitted_
      >> net_dev_data.errs_transmitted_
      >> net_dev_data.drop_errors_transmitted_
      >> net_dev_data.fifo_errors_transmitted_
      >> net_dev_data.collisions_transmitted_
      >> net_dev_data.carrier_losses_transmitted_
      >> net_dev_data.compressed_packets_transmitted_;
  if (iss.fail())
    return std::nullopt;
  if (!entry_name.empty()) {
    gsl_Expects(entry_name.back() == ':');
    entry_name.pop_back();  // remove the ':' from the end of 'eth0:' etc
  }
  return std::make_pair(entry_name, net_dev_data);
}

NetDevData NetDevData::operator-(const NetDevData& rhs) const {
  NetDevData result{};
  result.bytes_received_ = bytes_received_-rhs.bytes_received_;
  result.packets_received_ = packets_received_-rhs.packets_received_;
  result.errs_received_ = errs_received_-rhs.errs_received_;
  result.drop_errors_received_ = drop_errors_received_-rhs.drop_errors_received_;
  result.fifo_errors_received_ = fifo_errors_received_-rhs.fifo_errors_received_;
  result.frame_errors_received_ = frame_errors_received_-rhs.frame_errors_received_;
  result.compressed_packets_received_ = compressed_packets_received_-rhs.compressed_packets_received_;
  result.multicast_frames_received_ = multicast_frames_received_-rhs.multicast_frames_received_;

  result.bytes_transmitted_ = bytes_transmitted_-rhs.bytes_transmitted_;
  result.packets_transmitted_ = packets_transmitted_-rhs.packets_transmitted_;
  result.errs_transmitted_ = errs_transmitted_-rhs.errs_transmitted_;
  result.drop_errors_transmitted_ = drop_errors_transmitted_-rhs.drop_errors_transmitted_;
  result.fifo_errors_transmitted_ = fifo_errors_transmitted_-rhs.fifo_errors_transmitted_;
  result.collisions_transmitted_ = collisions_transmitted_-rhs.collisions_transmitted_;
  result.carrier_losses_transmitted_ = carrier_losses_transmitted_-rhs.carrier_losses_transmitted_;
  result.compressed_packets_transmitted_ = compressed_packets_transmitted_-rhs.compressed_packets_transmitted_;
  return result;
}

}  // namespace org::apache::nifi::minifi::extensions::procfs
