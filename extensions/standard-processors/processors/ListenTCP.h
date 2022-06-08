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

#include "core/Processor.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Enum.h"

// #include "asio/ts/buffer.hpp"
// #include "asio/ts/internet.hpp"
// #include "asio/streambuf.hpp"

namespace org::apache::nifi::minifi::processors {

class ListenTCP : public core::Processor {
 public:
  explicit ListenTCP(const std::string& name, const utils::Identifier& uuid = {})
      : core::Processor(name, uuid) {
  }
  ListenTCP(const ListenTCP&) = delete;
  ListenTCP(ListenTCP&&) = delete;
  ListenTCP& operator=(const ListenTCP&) = delete;
  ListenTCP& operator=(ListenTCP&&) = delete;

  EXTENSIONAPI static const core::Property LocalNetworkInterface;
  EXTENSIONAPI static const core::Property Port;
  EXTENSIONAPI static const core::Property MaxSizeOfMessageQueue;
  EXTENSIONAPI static const core::Property MaxBatchSize;
  EXTENSIONAPI static const core::Property BatchingMessageDelimiter;

  EXTENSIONAPI static const core::Relationship Success;

  void onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) override;
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) override;

  bool isSingleThreaded() const override {
    return false;
  }

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_FORBIDDEN;
  }

 private:
  uint64_t max_batch_size_ = 500;
  std::optional<uint64_t> max_queue_size_;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ListenTCP>::getLogger();
};

}  // namespace org::apache::nifi::minifi::processors
