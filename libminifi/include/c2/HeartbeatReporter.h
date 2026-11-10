// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

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
