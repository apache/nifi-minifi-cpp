/**
 *
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

#include <memory>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "PcapLiveDeviceList.h"
#include "PcapFilter.h"
#include "PcapPlusPlusVersion.h"
#include "PcapFileDevice.h"
#include "core/FlowFile.h"
#include "core/logging/Logger.h"
#include "core/ProcessContext.h"
#include "core/PropertyBuilder.h"
#include "core/Relationship.h"
#include "core/Resource.h"
#include "CapturePacket.h"
#include "ResourceClaim.h"
#include "utils/StringUtils.h"
#include "utils/ByteArrayCallback.h"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::processors {

std::shared_ptr<utils::IdGenerator> CapturePacket::id_generator_ = utils::IdGenerator::getIdGenerator();

const core::Property CapturePacket::BaseDir(core::PropertyBuilder::createProperty("Base Directory")
    ->withDescription("Scratch directory for PCAP files")
    ->withDefaultValue<std::string>("/tmp/")->build());
const core::Property CapturePacket::BatchSize(core::PropertyBuilder::createProperty("Batch Size")
    ->withDescription("The number of packets to combine within a given PCAP")
    ->withDefaultValue<uint64_t>(50)->build());
const core::Property CapturePacket::NetworkControllers("Network Controllers", "Regular expression of the network controller(s) to which we will attach", ".*");
const core::Property CapturePacket::CaptureBluetooth(core::PropertyBuilder::createProperty("Capture Bluetooth")
    ->withDescription("True indicates that we support bluetooth interfaces")
    ->withDefaultValue<bool>(false)->build());

const core::Relationship CapturePacket::Success("success", "All files are routed to success");

std::string CapturePacket::generate_new_pcap(const std::string &base_path) {
  std::string path = base_path;
  // can use relaxed for a counter
  int cnt = num_.fetch_add(1, std::memory_order_relaxed);
  std::string filename = std::to_string(cnt);
  path.append(filename);
  return path;
}

void CapturePacket::packet_callback(pcpp::RawPacket* packet, pcpp::PcapLiveDevice* /*dev*/, void* data) {
  // parse the packet
  auto capture_mechanism = reinterpret_cast<PacketMovers*>(data);

  CapturePacketMechanism *capture;

  if (capture_mechanism->source.try_dequeue(capture)) {
    // if needed - write the packet to the output pcap file

    if (capture->writer_ != nullptr) {
      capture->writer_->writePacket(*packet);

      if (capture->incrementAndCheck()) {
        capture->writer_->close();

        auto new_capture = create_new_capture(capture->getBasePath(), capture->getMaxSize());

        capture_mechanism->sink.enqueue(capture);

        capture_mechanism->source.enqueue(new_capture);
      } else {
        capture_mechanism->source.enqueue(capture);
      }
    }
  }
}

CapturePacketMechanism *CapturePacket::create_new_capture(const std::string &base_path, int64_t *max_size) {
  auto new_capture = new CapturePacketMechanism(base_path, generate_new_pcap(base_path), max_size);
  new_capture->writer_ = new pcpp::PcapFileWriterDevice(new_capture->getFile());
  if (!new_capture->writer_->open())
    throw std::runtime_error{utils::StringUtils::join_pack("Failed to open PcapFileWriterDevice with file ", new_capture->getFile())};

  return new_capture;
}

std::atomic<int> CapturePacket::num_(0);

void CapturePacket::initialize() {
  logger_->log_info("Initializing CapturePacket");

  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void CapturePacket::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory>& /*sessionFactory*/) {
  std::string value;
  if (context->getProperty(BatchSize.getName(), value)) {
    core::Property::StringToInt(value, pcap_batch_size_);
  }

  value = "";
  if (context->getProperty(BaseDir.getName(), value)) {
    base_dir_ = value;
  }

  value = "";
  if (context->getProperty(CaptureBluetooth.getName(), value)) {
    capture_bluetooth_ = utils::StringUtils::toBool(value).value_or(false);
  }

  core::Property attached_controllers("Network Controllers", "List of network controllers to attach to -- each may be a regex", ".*");

  getProperty(attached_controllers.getName(), attached_controllers);

  std::vector<std::string> allowed_interfaces = attached_controllers.getValues();

  if (IsNullOrEmpty(base_dir_)) {
    base_dir_ = "/tmp/";
  }

  utils::Identifier dir_ext = id_generator_->generate();

  base_path_ = dir_ext.to_string();

  const std::vector<pcpp::PcapLiveDevice*>& devList = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDevicesList();
  for (auto iter : devList) {
    const std::string name = iter->getName();

    if (!allowed_interfaces.empty()) {
      bool found_match = false;
      std::string matching_regex;
      for (const auto &filter : allowed_interfaces) {
        utils::Regex r(filter);
        utils::SMatch m;
        if (utils::regexMatch(name, m, r)) {
          matching_regex = filter;
          found_match = true;
          break;
        }
      }
      if (!found_match) {
        logger_->log_debug("Skipping %s because it does not match any regex", name);
        continue;
      } else {
        logger_->log_trace("Accepting %s because it matches %s", name, matching_regex);
      }
    }

    if (!iter->open()) {
      logger_->log_error("Could not open device %s", name);
      continue;
    }

    if (!capture_bluetooth_) {
      if (name.find("bluetooth") != std::string::npos) {
        logger_->log_error("Skipping %s because blue tooth capture is not enabled", name);
        continue;
      }
    }

    if (name.find("dbus") != std::string::npos) {
      logger_->log_error("Skipping %s because dbus capture is disabled", name);
      continue;
    }

    if (iter->startCapture(packet_callback, mover.get())) {
      logger_->log_debug("Starting capture on %s", iter->getName());
      CapturePacketMechanism *aa = create_new_capture(getPath(), &pcap_batch_size_);
      logger_->log_trace("Creating packet capture in %s", aa->getFile());
      mover->source.enqueue(aa);
      device_list_.push_back(iter);
    }
  }

  if (IsNullOrEmpty(devList)) {
    logger_->log_error("Could not open any devices");
    throw std::runtime_error{"Pcap: could not open any devices"};
  }
}

CapturePacket::~CapturePacket() = default;

void CapturePacket::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  CapturePacketMechanism *capture;
  if (mover->sink.try_dequeue(capture)) {
    auto ff = session->create();
    session->import(capture->getFile(), ff, false, 0);
    logger_->log_debug("Received packet capture in file %s %d for %s", capture->getFile(), capture->getSize(), ff->getResourceClaim()->getContentFullPath());
    session->transfer(ff, Success);
    delete capture;
  } else {
    context->yield();
  }
}

REGISTER_RESOURCE(CapturePacket, Processor);

}  // namespace org::apache::nifi::minifi::processors
