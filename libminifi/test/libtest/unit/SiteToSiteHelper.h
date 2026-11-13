// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <queue>
#include "io/BufferStream.h"
#include "core/Core.h"
#include "minifi-cpp/utils/gsl.h"

namespace minifi = org::apache::nifi::minifi;

/**
 * Test repository
 */
class SiteToSiteResponder : public minifi::io::BaseStreamImpl {
 private:
  minifi::io::BufferStream server_responses_;
  std::queue<std::string> client_responses_;

 public:
  SiteToSiteResponder() = default;
  // initialize
  int initialize() override {
    return 1;
  }

  void push_response(const std::string& resp) {
    server_responses_.write(reinterpret_cast<const uint8_t*>(resp.data()), resp.length());
  }

  size_t write(const uint8_t *value, size_t size) override {
    client_responses_.push(std::string(reinterpret_cast<const char*>(value), size));
    return size;
  }

  std::string get_next_client_response() {
    std::string ret = client_responses_.front();
    client_responses_.pop();
    return ret;
  }

  /**
   * reads a byte array from the stream
   * @param out_buffer reference in which will set the result
   * @param len length to read
   * @return resulting read size
   **/
  size_t read(std::span<std::byte> out_buffer) override {
    return server_responses_.read(out_buffer);
  }
};
