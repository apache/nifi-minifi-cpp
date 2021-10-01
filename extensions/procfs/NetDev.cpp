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

namespace org::apache::nifi::minifi::procfs {

std::optional<NetDevData> NetDevData::parseNetDevLine(std::istringstream& iss) {
  NetDevData net_dev_data;
  iss >> net_dev_data.bytes_received_ >> net_dev_data.packets_received_ >> net_dev_data.errs_received_ >> net_dev_data.drop_errors_received_
      >> net_dev_data.fifo_errors_received_ >> net_dev_data.frame_errors_received_ >> net_dev_data.compressed_packets_received_
      >> net_dev_data.multicast_frames_received_>> net_dev_data.bytes_transmitted_ >> net_dev_data.packets_transmitted_
      >> net_dev_data.errs_transmitted_ >> net_dev_data.drop_errors_transmitted_ >> net_dev_data.fifo_errors_transmitted_
      >> net_dev_data.collisions_transmitted_ >> net_dev_data.carrier_losses_transmitted_ >> net_dev_data.compressed_packets_transmitted_;
  if (iss.fail())
    return std::nullopt;
  return net_dev_data;
}

bool NetDevData::operator>=(const NetDevData& rhs) const {
  return bytes_received_ >= rhs.bytes_received_
         && packets_received_ >= rhs.packets_received_
         && errs_received_ >= rhs.errs_received_
         && drop_errors_received_ >= rhs.drop_errors_received_
         && fifo_errors_received_ >= rhs.fifo_errors_received_
         && frame_errors_received_ >= rhs.frame_errors_received_
         && compressed_packets_received_ >= rhs.compressed_packets_received_
         && multicast_frames_received_ >= rhs.multicast_frames_received_
         && bytes_transmitted_ >= rhs.bytes_transmitted_
         && packets_transmitted_ >= rhs.packets_transmitted_
         && errs_transmitted_ >= rhs.errs_transmitted_
         && drop_errors_transmitted_ >= rhs.drop_errors_transmitted_
         && fifo_errors_transmitted_ >= rhs.fifo_errors_transmitted_
         && collisions_transmitted_ >= rhs.collisions_transmitted_
         && carrier_losses_transmitted_ >= rhs.carrier_losses_transmitted_
         && compressed_packets_transmitted_ >= rhs.compressed_packets_transmitted_;
}

NetDevData NetDevData::operator-(const NetDevData& rhs) const {
  NetDevData result;
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

void NetDev::addToJson(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) const {
  rapidjson::Value entry_name;
  entry_name.SetString(interface_name_.c_str(), interface_name_.length(), alloc);
  body.AddMember(entry_name, rapidjson::Value(rapidjson::kObjectType), alloc);
  rapidjson::Value& entry = body[interface_name_.c_str()];
  entry.AddMember(BYTES_RECEIVED_STR, data_.getBytesReceived(), alloc);
  entry.AddMember(PACKETS_RECEIVED_STR, data_.getPacketsReceived(), alloc);
  entry.AddMember(RECEIVE_ERRORS_STR, data_.getReceiveErrors(), alloc);
  entry.AddMember(RECEIVE_DROP_ERRORS_STR, data_.getReceiveDropErrors(), alloc);
  entry.AddMember(RECEIVE_FIFO_ERRORS_STR, data_.getReceiveFifoErrors(), alloc);
  entry.AddMember(RECEIVE_FRAME_ERRORS_STR, data_.getReceiveFrameErrors(), alloc);
  entry.AddMember(COMPRESSED_PACKETS_RECEIVED_STR, data_.getCompressedPacketsReceived(), alloc);
  entry.AddMember(MULTICAST_FRAMES_RECEIVED_STR, data_.getMulticastFramesReceived(), alloc);

  entry.AddMember(BYTES_TRANSMITTED_STR, data_.getBytesTransmitted(), alloc);
  entry.AddMember(PACKETS_TRANSMITTED_STR, data_.getPacketsTransmitted(), alloc);
  entry.AddMember(TRANSMIT_ERRORS_STR, data_.getTransmitErrors(), alloc);
  entry.AddMember(TRANSMIT_DROP_ERRORS_STR, data_.getTransmitDropErrors(), alloc);
  entry.AddMember(TRANSMIT_FIFO_ERRORS_STR, data_.getTransmitFifoErrors(), alloc);
  entry.AddMember(TRANSMIT_COLLISIONS_DETECTED_STR, data_.getTransmitCollisions(), alloc);
  entry.AddMember(TRANSMIT_CARRIER_LOSSES_STR, data_.getTransmitCarrierLosses(), alloc);
  entry.AddMember(COMPRESSED_PACKETS_TRANSMITTED_STR, data_.getCompressedPacketsTransmitted(), alloc);
}

void NetDevPeriod::addToJson(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) const {
  rapidjson::Value entry_name;
  entry_name.SetString(interface_name_.c_str(), interface_name_.length(), alloc);
  body.AddMember(entry_name, rapidjson::Value(rapidjson::kObjectType), alloc);
  rapidjson::Value& entry = body[interface_name_.c_str()];
  entry.AddMember(BYTES_RECEIVED_PER_SEC_STR, getBytesReceivedPerSec(), alloc);
  entry.AddMember(PACKETS_RECEIVED_PER_SEC_STR, getPacketsReceivedPerSec(), alloc);
  entry.AddMember(RECEIVE_ERRORS_PER_SEC_STR, getErrsReceivedPerSec(), alloc);
  entry.AddMember(RECEIVE_DROP_ERRORS_PER_SEC_STR, getDropErrorsReceivedPerSec(), alloc);
  entry.AddMember(RECEIVE_FIFO_ERRORS_PER_SEC_STR, getFifoErrorsReceivedPerSec(), alloc);
  entry.AddMember(RECEIVE_FRAME_ERRORS_PER_SEC_STR, getFrameErrorsReceivedPerSec(), alloc);
  entry.AddMember(COMPRESSED_PACKETS_RECEIVED_PER_SEC_STR, getCompressedPacketsReceivedPerSec(), alloc);
  entry.AddMember(MULTICAST_FRAMES_RECEIVED_PER_SEC_STR, getMulticastFramesReceivedPerSec(), alloc);

  entry.AddMember(BYTES_TRANSMITTED_PER_SEC_STR, getBytesTransmittedPerSec(), alloc);
  entry.AddMember(PACKETS_TRANSMITTED_PER_SEC_STR, getPacketsTransmittedPerSec(), alloc);
  entry.AddMember(TRANSMIT_ERRORS_PER_SEC_STR, getErrsTransmittedPerSec(), alloc);
  entry.AddMember(TRANSMIT_DROP_ERRORS_PER_SEC_STR, getDropErrorsTransmittedPerSec(), alloc);
  entry.AddMember(TRANSMIT_FIFO_ERRORS_PER_SEC_STR, getFifoErrorsTransmittedPerSec(), alloc);
  entry.AddMember(TRANSMIT_COLLISIONS_DETECTED_PER_SEC_STR, getCollisionsTransmittedPerSec(), alloc);
  entry.AddMember(TRANSMIT_CARRIER_LOSSES_PER_SEC_STR, getCarrierLossesTransmittedPerSec(), alloc);
  entry.AddMember(COMPRESSED_PACKETS_TRANSMITTED_PER_SEC_STR, getCompressedPacketsTransmittedPerSec(), alloc);
}

std::optional<NetDevPeriod> NetDevPeriod::create(const NetDev& net_dev_start, const NetDev& net_dev_end) {
  if (net_dev_start.getInterfaceName() != net_dev_end.getInterfaceName())
    return std::nullopt;
  if (!(net_dev_end.getData() >= net_dev_start.getData()))
    return std::nullopt;
  if (!(net_dev_end.getTimePoint() > net_dev_start.getTimePoint()))
    return std::nullopt;
  return NetDevPeriod(net_dev_start.getInterfaceName(),
                      net_dev_end.getData()-net_dev_start.getData(),
                      net_dev_end.getTimePoint() - net_dev_start.getTimePoint());
}

}  // namespace org::apache::nifi::minifi::procfs
