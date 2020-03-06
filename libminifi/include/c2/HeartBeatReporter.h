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

#include "C2Protocol.h"
#include "C2Payload.h"
#include "core/controller/ControllerServiceProvider.h"
#include "properties/Configure.h"
#include "core/Connectable.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

/**
 * Defines a heart beat reporting interface. Note that this differs from
 * C2Protocol as heartbeats can be any interface which provides only one way communication.
 */
class HeartBeatReporter : public core::Connectable {
 public:

  HeartBeatReporter(std::string name, utils::Identifier & uuid)
      : core::Connectable(name, uuid),
        controller_(nullptr),
        update_sink_(nullptr),
        configuration_(nullptr) {
  }

  virtual void initialize(const std::shared_ptr<core::controller::ControllerServiceProvider> &controller, const std::shared_ptr<state::StateMonitor> &updateSink,
                          const std::shared_ptr<Configure> &configure) {
    controller_ = controller;
    update_sink_ = updateSink;
    configuration_ = configure;
  }
  virtual ~HeartBeatReporter() {
  }
  /**
   * Send a C2 payloadd to the provided URI. The direction indicates to the protocol whether or not this a transmit or receive operation.
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
  virtual bool isRunning() {
    return true;
  }

  /**
   * Block until work is available on any input connection, or the given duration elapses
   * @param timeoutMs timeout in milliseconds
   */
  void waitForWork(uint64_t timeoutMs);

  virtual void yield() {

  }

  /**
   * Determines if work is available by this connectable
   * @return boolean if work is available.
   */
  virtual bool isWorkAvailable() {
    return true;
  }

 protected:

  std::shared_ptr<core::controller::ControllerServiceProvider> controller_;

  std::shared_ptr<state::StateMonitor> update_sink_;

  std::shared_ptr<Configure> configuration_;
};

} /* namesapce c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_C2_HEARTBEATREPORTER_H_ */
