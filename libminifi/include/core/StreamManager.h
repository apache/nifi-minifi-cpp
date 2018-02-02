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
#ifndef LIBMINIFI_INCLUDE_CORE_STREAMMANAGER_H_
#define LIBMINIFI_INCLUDE_CORE_STREAMMANAGER_H_

#include "properties/Configure.h"
#include "ResourceClaim.h"
#include "io/DataStream.h"
#include "io/BaseStream.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

/**
 * Purpose: Provides a base for all stream based managers. The goal here is to provide
 * a small set of interfaces that provide a small set of operations to provide state 
 * management for streams.
 */
template<typename T>
class StreamManager {
 public:
  virtual ~StreamManager() {

  }

  virtual std::string getStoragePath() = 0;

  /**
   * Create a write stream using the streamId as a reference.
   * @param streamId stream identifier
   * @return stream pointer.
   */
  virtual std::shared_ptr<io::BaseStream> write(const std::shared_ptr<T> &streamId) = 0;

  /**
   * Create a read stream using the streamId as a reference.
   * @param streamId stream identifier
   * @return stream pointer.
   */
  virtual std::shared_ptr<io::BaseStream> read(const std::shared_ptr<T> &streamId) = 0;

  /**
   * Closes the stream
   * @param streamId stream identifier
   * @return result of operation.
   */
  virtual bool close(const std::shared_ptr<T> &streamId) = 0;

  /**
   * Removes the stream from this stream manager. The end result
   * is dependent on the stream manager implementation.
   * @param streamId stream identifier
   * @return result of operation.
   */
  virtual bool remove(const std::shared_ptr<T> &streamId) = 0;

  /**
   * Removes an item if it was orphan
   */
  virtual bool removeIfOrphaned(const std::shared_ptr<T> &streamId) = 0;

  virtual uint32_t getStreamCount(const std::shared_ptr<T> &streamId) = 0;

  virtual void incrementStreamCount(const std::shared_ptr<T> &streamId) = 0;

  virtual void decrementStreamCount(const std::shared_ptr<T> &streamId) = 0;

  virtual bool exists(const std::shared_ptr<T> &streamId) = 0;

};

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_STREAMMANAGER_H_ */
