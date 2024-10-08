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
#ifndef LIBMINIFI_INCLUDE_C2_HEARTBEATREPORTER_H_
#define LIBMINIFI_INCLUDE_C2_HEARTBEATREPORTER_H_

#include <memory>
#include <string>
#include <utility>

#include "C2Protocol.h"
#include "C2Payload.h"
#include "core/controller/ControllerServiceProvider.h"
#include "properties/Configure.h"
#include "core/Connectable.h"

namespace org::apache::nifi::minifi::c2 {

/**
 * Defines a heartbeat reporting interface. Note that this differs from
 * C2Protocol as heartbeats can be any interface which provides only one way communication.
 */
class HeartbeatReporter : public core::ConnectableImpl {
 public:
  HeartbeatReporter(std::string_view name, const utils::Identifier& uuid)
      : core::ConnectableImpl(name, uuid),
        controller_(nullptr),
        update_sink_(nullptr),
        configuration_(nullptr) {
  }

  virtual void initialize(core::controller::ControllerServiceProvider* controller, state::StateMonitor* updateSink,
                          const std::shared_ptr<Configure> &configure) {
    controller_ = controller;
    update_sink_ = updateSink;
    configuration_ = configure;
  }

  ~HeartbeatReporter() override = default;

  /**
   * Send a C2 payload to the provided URI. The direction indicates to the protocol whether or not this a transmit or receive operation.
   * Depending on the protocol this may mean different things.
   *
   * @param url url.
   * @param operation payload to perform and/or send
   * @param direction direction of the C2 operation.
   * @param async whether or not this is an asynchronous operation
   * @return result of the heartbeat operation
   */
  virtual int16_t heartbeat(const C2Payload &heartbeat) = 0;

  /**
   * Determines if we are connected and operating
   */
  bool isRunning() const override {
    return true;
  }

  void yield() override {
  }

  /**
   * Determines if work is available by this connectable
   * @return boolean if work is available.
   */
  bool isWorkAvailable() override {
    return true;
  }

 protected:
  core::controller::ControllerServiceProvider* controller_;
  state::StateMonitor* update_sink_;
  std::shared_ptr<Configure> configuration_;
};

}  // namespace org::apache::nifi::minifi::c2

#endif  // LIBMINIFI_INCLUDE_C2_HEARTBEATREPORTER_H_
