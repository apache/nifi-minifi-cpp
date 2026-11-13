// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <iostream>
#include <cstdint>
#include <string>
#include <memory>
#include "database/RocksDatabase.h"
#include "io/BaseStream.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::io {

/**
 * Purpose: File Stream Base stream extension. This is intended to be a thread safe access to
 * read/write to the local file system.
 *
 * Design: Simply extends BaseStream and overrides readData/writeData to allow a sink to the
 * fstream object.
 */
class RocksDbStream : public io::BaseStreamImpl {
 public:
  /**
   * File Stream constructor that accepts an fstream shared pointer.
   * It must already be initialized for read and write.
   */
  explicit RocksDbStream(std::string path, gsl::not_null<minifi::internal::RocksDatabase*> db, bool write_enable = false,
    minifi::internal::WriteBatch* batch = nullptr, bool use_synchronous_writes = true, bool verify_checksums = false);

  ~RocksDbStream() override {
    close();
  }

  void close() final;
  /**
   * Skip to the specified offset.
   * @param offset offset to which we will skip
   */
  void seek(size_t offset) override;

  size_t tell() const override;

  size_t size() const override {
    return size_;
  }

  using BaseStream::write;
  using BaseStream::read;

  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   */
  size_t read(std::span<std::byte> buf) override;

  /**
   * writes value to stream
   * @param value value to write
   * @param size size of value
   */
  size_t write(const uint8_t *value, size_t size) override;

 protected:
  std::string path_;
  bool write_enable_;
  gsl::not_null<minifi::internal::RocksDatabase*> db_;
  std::string value_;
  bool exists_;
  size_t offset_;
  minifi::internal::WriteBatch* batch_;
  size_t size_;
  bool use_synchronous_writes_;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<RocksDbStream>::getLogger();
};

}  // namespace org::apache::nifi::minifi::io
