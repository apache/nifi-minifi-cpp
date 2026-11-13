// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>

#include "civetweb.h"
#include "minifi-cpp/utils/gsl.h"
#include "io/InputStream.h"

namespace org::apache::nifi::minifi::io {

class CivetStream : public io::InputStreamImpl {
 public:
  /**
   * File Stream constructor that accepts an fstream shared pointer.
   * It must already be initialized for read and write.
   */
  explicit CivetStream(struct mg_connection *conn) : conn(conn) {}

  ~CivetStream() override = default;

  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   */
  size_t read(std::span<std::byte> buf) override {
    const auto ret = mg_read(conn, buf.data(), buf.size());
    if (ret < 0) return STREAM_ERROR;
    return gsl::narrow<size_t>(ret);
  }

 protected:
  struct mg_connection *conn;

 private:
  std::shared_ptr<minifi::core::logging::Logger> logger_;
};
}  // namespace org::apache::nifi::minifi::io
