// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "core/logging/LoggerFactory.h"
#include "io/BaseStream.h"
#include "LmdbWrapper.h"

namespace org::apache::nifi::minifi::extensions::lmdb {

class LmdbStream : public io::BaseStreamImpl {
 public:
  explicit LmdbStream(std::string path, LmdbWrapper& lmdb_wrapper, bool write_enable, bool append = true);

  ~LmdbStream() override { close(); }

  void close() final;
  void seek(size_t offset) override;

  size_t tell() const override;

  size_t size() const override { return value_.size(); }

  using BaseStream::read;
  using BaseStream::write;

  size_t read(std::span<std::byte> buf) override;
  size_t write(const uint8_t* value, size_t size) override;

  bool commit();

 private:
  bool loadValue();

  LmdbWrapper& lmdb_wrapper_;
  std::string path_;
  bool write_enable_;
  std::string value_;
  bool exists_;
  size_t offset_ = 0;
  bool dirty_ = false;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<LmdbStream>::getLogger();
};

}  // namespace org::apache::nifi::minifi::extensions::lmdb
