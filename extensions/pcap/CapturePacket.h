/**
 * CapturePacket class declaration
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
#pragma once

#include <memory>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include "PcapLiveDeviceList.h"
#include "PcapFilter.h"
#include "PcapFileDevice.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"
#include "concurrentqueue.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Id.h"

namespace org::apache::nifi::minifi::processors {

class CapturePacketMechanism {
 public:
  explicit CapturePacketMechanism(const std::string &base_path, const std::string &file, int64_t *max_size)
      : writer_(nullptr),
        path_(base_path),
        file_(file),
        max_size_(max_size) {
    atomic_count_.store(0);
  }

  ~CapturePacketMechanism() {
    delete writer_;
  }

  bool inline incrementAndCheck() {
    return ++atomic_count_ >= *max_size_;
  }

  int64_t *getMaxSize() {
    return max_size_;
  }

  pcpp::PcapFileWriterDevice *writer_;

  const std::string &getBasePath() {
    return path_;
  }

  const std::string &getFile() {
    return file_;
  }

  int64_t getSize() const {
    return atomic_count_;
  }

 protected:
  CapturePacketMechanism &operator=(const CapturePacketMechanism &other) = delete;
  std::string path_;
  std::string file_;
  int64_t *max_size_;
  std::atomic<int64_t> atomic_count_;
};

struct PacketMovers {
  moodycamel::ConcurrentQueue<CapturePacketMechanism*> source;
  moodycamel::ConcurrentQueue<CapturePacketMechanism*> sink;
};

class CapturePacket : public core::Processor {
 public:
  explicit CapturePacket(std::string name, const utils::Identifier& uuid = {})
      : Processor(std::move(name), uuid) {
    mover = std::make_unique<PacketMovers>();
  }
  ~CapturePacket() override;

  EXTENSIONAPI static constexpr const char* Description = "CapturePacket captures and writes one or more packets into a PCAP file that will be used as the content of a flow file."
    " Configuration options exist to adjust the batching of PCAP files. PCAP batching will place a single PCAP into a flow file. "
    "A regular expression selects network interfaces. Bluetooth network interfaces can be selected through a separate option.";

  static const core::Property BatchSize;
  static const core::Property NetworkControllers;
  static const core::Property BaseDir;
  static const core::Property CaptureBluetooth;
  static auto properties() {
    return std::array{
      BatchSize,
      NetworkControllers,
      BaseDir,
      CaptureBluetooth
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  static auto relationships() { return std::array{Success}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;

  static void packet_callback(pcpp::RawPacket* packet, pcpp::PcapLiveDevice* dev, void* data);

 protected:
  void notifyStop() override {
    logger_->log_debug("Stopping capture");
    for (auto dev : device_list_) {
      dev->stopCapture();
      dev->close();
    }
    logger_->log_trace("Stopped device capture. clearing queues");
    CapturePacketMechanism *capture;
    while (mover->source.try_dequeue(capture)) {
      std::remove(capture->getFile().c_str());
      delete capture;
    }
    logger_->log_trace("Cleared source queue");
    while (mover->sink.try_dequeue(capture)) {
      std::remove(capture->getFile().c_str());
      delete capture;
    }
    device_list_.clear();
    logger_->log_trace("Cleared sink queue");
  }

  static std::string generate_new_pcap(const std::string &base_path);

  static CapturePacketMechanism *create_new_capture(const std::string &base_path, int64_t *max_size);

 private:
  inline std::string getPath() {
    return base_dir_ + "/" + base_path_;
  }
  bool capture_bluetooth_ = false;
  std::string base_dir_;
  std::vector<std::string> attached_controllers_;
  std::string base_path_;
  int64_t pcap_batch_size_ = 50;
  std::unique_ptr<PacketMovers> mover;
  static std::atomic<int> num_;
  std::vector<pcpp::PcapLiveDevice*> device_list_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<CapturePacket>::getLogger();
  static std::shared_ptr<utils::IdGenerator> id_generator_;
};

}  // namespace org::apache::nifi::minifi::processors
