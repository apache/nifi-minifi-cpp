// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <string>

#include "minifi-cpp/properties/Configure.h"
#include "minifi-cpp/ResourceClaim.h"
#include "io/BufferStream.h"
#include "minifi-cpp/io/BaseStream.h"

namespace org::apache::nifi::minifi::core {

class StreamAppendLock {
 public:
  virtual ~StreamAppendLock() = default;
};

/**
 * Purpose: Provides a base for all stream based managers. The goal here is to provide
 * a small set of interfaces that provide a small set of operations to provide state
 * management for streams.
 */
template<typename T>
class StreamManager {
 public:
  enum class StreamState{
    Deleted,
    Alive
  };
  virtual ~StreamManager() = default;

  virtual std::string getStoragePath() const = 0;

  /**
   * Create a write stream using the streamId as a reference.
   * @param streamId stream identifier
   * @return stream pointer.
   */
  virtual std::shared_ptr<io::BaseStream> write(const T &streamId, bool append) = 0;

  /**
   * Queries the stream and locks it to be appended to
   */
  virtual std::unique_ptr<StreamAppendLock> lockAppend(const T &streamId, size_t offset) = 0;

  /**
   * Create a read stream using the streamId as a reference.
   * @param streamId stream identifier
   * @return stream pointer.
   */
  virtual std::shared_ptr<io::BaseStream> read(const T &streamId) = 0;

  virtual size_t size(const T &streamId) = 0;

  /**
   * Closes the stream
   * @param streamId stream identifier
   * @return result of operation.
   */
  virtual bool close(const T &streamId) = 0;

  /**
   * Removes the stream from this stream manager. The end result
   * is dependent on the stream manager implementation.
   * @param streamId stream identifier
   * @return result of operation.
   */
  virtual bool remove(const T &streamId) = 0;

  virtual uint32_t getStreamCount(const T &streamId) = 0;

  virtual void incrementStreamCount(const T &streamId) = 0;

  virtual StreamState decrementStreamCount(const T &streamId) = 0;

  virtual bool exists(const T &streamId) = 0;
};

}  // namespace org::apache::nifi::minifi::core
