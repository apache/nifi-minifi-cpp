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
#include <vector>
#include <string>

#include "PcapLiveDeviceList.h"
#include "PcapFilter.h"
#include "PcapFileDevice.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"
#include "core/Resource.h"
#include "concurrentqueue.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Id.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

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

// CapturePacket Class
class CapturePacket : public core::Processor {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  explicit CapturePacket(const std::string& name, const utils::Identifier& uuid = {})
      : Processor(name, uuid),
        capture_bluetooth_(false),
        pcap_batch_size_(50),
        logger_(logging::LoggerFactory<CapturePacket>::getLogger()) {
    mover = std::unique_ptr<PacketMovers>(new PacketMovers());
  }
  // Destructor
  virtual ~CapturePacket();
  // Processor Name
  static const char *ProcessorName;
  static core::Property BatchSize;
  static core::Property NetworkControllers;
  static core::Property BaseDir;
  static core::Property CaptureBluetooth;
  // Supported Relationships
  static core::Relationship Success;

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
  bool capture_bluetooth_;
  std::string base_dir_;
  std::vector<std::string> attached_controllers_;
  std::string base_path_;
  int64_t pcap_batch_size_;
  std::unique_ptr<PacketMovers> mover;
  static std::atomic<int> num_;
  std::vector<pcpp::PcapLiveDevice*> device_list_;
  std::shared_ptr<logging::Logger> logger_;
  static std::shared_ptr<utils::IdGenerator> id_generator_;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
