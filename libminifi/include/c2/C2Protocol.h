// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "C2Payload.h"
#include "core/controller/ControllerServiceProvider.h"
#include "properties/Configure.h"
#include "core/Connectable.h"
namespace org::apache::nifi::minifi::c2 {

/**
 * Defines a protocol to perform state management of the minifi agent.
 */
class C2Protocol : public core::ConnectableImpl {
 public:
  C2Protocol(std::string_view name, const utils::Identifier &uuid)
      : core::ConnectableImpl(name, uuid),
        running_(true) {
  }

  virtual void initialize(core::controller::ControllerServiceProvider* controller, const std::shared_ptr<Configure> &configure) {
    controller_ = controller;
    configuration_ = configure;
  }
  virtual ~C2Protocol() = default;

  /**
   * Update the configuration.
   */
  virtual void update(const std::shared_ptr<Configure> &configure) = 0;

  /**
   * Send a C2 payload to the provided URI. The direction indicates to the protocol whether or not this a transmit or receive operation.
   * Depending on the protocol this may mean different things.
   *
   * @param url url.
   * @param operation payload to perform and/or send
   * @param direction direction of the C2 operation.
   * @param async whether or not this is an asynchronous operation
   * @return payload from the response or a response to come back in the face of an asynchronous operation.
   */
  virtual C2Payload consumePayload(const std::string &url, const C2Payload &operation, Direction direction = TRANSMIT, bool async = false) = 0;

  /**
   * Send a C2 payload . The direction indicates to the protocol whether or not this a transmit or receive operation.
   * Depending on the protocol this may mean different things.
   *
   * @param operation payload to perform and/or send
   * @param direction direction of the C2 operation.
   * @param async whether or not this is an asynchronous operation
   * @return payload from the response or a response to come back in the face of an asynchronous operation.
   */
  virtual C2Payload consumePayload(const C2Payload &operation, Direction direction = TRANSMIT, bool async = false) = 0;

  virtual bool fetch(const std::string& url, const std::vector<std::string>& accepted_formats, std::function<bool(std::span<const char> chunk)> chunk_callback) = 0;

  bool fetch(const std::string& url, std::function<bool(std::span<const char> chunk)> chunk_callback) {
    return fetch(url, {}, std::move(chunk_callback));
  }

  /**
   * Determines if we are connected and operating
   */
  bool isRunning() const override {
    return running_.load();
  }

  using core::ConnectableImpl::waitForWork;
  /**
   * Block until work is available on any input connection, or the given duration elapses
   * @param timeoutMs timeout in milliseconds
   */
  void waitForWork(uint64_t timeoutMs);

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
  std::atomic<bool> running_;

  core::controller::ControllerServiceProvider* controller_;

  std::shared_ptr<Configure> configuration_;
};

}  // namespace org::apache::nifi::minifi::c2
