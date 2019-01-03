/**
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
#ifndef LIBMINIFI_INCLUDE_UTILS_ID_H_
#define LIBMINIFI_INCLUDE_UTILS_ID_H_

#include <cstddef>
#include <atomic>
#include <memory>
#include <string>
#include "uuid/uuid.h"

#include "core/logging/Logger.h"
#include "properties/Properties.h"

#define UNSIGNED_CHAR_MAX 255
#define UUID_TIME_IMPL 0
#define UUID_RANDOM_IMPL 1
#define UUID_DEFAULT_IMPL 2
#define MINIFI_UID_IMPL 3

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

template<typename T, typename C>
class IdentifierBase {
 public:

  IdentifierBase(T myid) {
    copyInto(myid);
  }

  IdentifierBase(const IdentifierBase &other) {
    copyInto(other.id_);
  }

  IdentifierBase(IdentifierBase &&other)
      : converted_(std::move(other.converted_)) {
    copyInto(other.id_);
  }

  IdentifierBase() {
  }

  IdentifierBase &operator=(const IdentifierBase &other) {
    copyInto(other.id_);
    return *this;
  }

  IdentifierBase &operator=(T o) {
    copyInto(o);
    return *this;
  }

  void getIdentifier(T other) const {
    copyOutOf(other);
  }

  C convert() const {
    return converted_;
  }

 protected:

  void copyInto(const IdentifierBase &other) {
    memcpy(id_, other.id_, sizeof(T));
  }

  void copyInto(const void *other) {
    memcpy(id_, other, sizeof(T));
  }

  void copyOutOf(void *other) {
    memcpy(other, id_, sizeof(T));
  }

  C converted_;

  T id_;
};

class Identifier : public IdentifierBase<UUID_FIELD, std::string> {
 public:
  Identifier(UUID_FIELD u);
  Identifier();
  Identifier(const Identifier &other);
  Identifier(Identifier &&other);

  /**
   * I believe these exist to make windows builds happy -- need more testing
   * to ensure this doesn't cause any issues.
   */
  Identifier(const IdentifierBase &other);
  Identifier &operator=(const IdentifierBase &other);

  Identifier &operator=(const Identifier &other);
  Identifier &operator=(UUID_FIELD o);

  Identifier &operator=(std::string id);
  bool operator==(const std::nullptr_t nullp) const;

  bool operator!=(std::nullptr_t nullp) const;

  bool operator!=(const Identifier &other) const;
  bool operator==(const Identifier &other) const;

  std::string to_string() const;

  const unsigned char * const toArray() const;

 protected:

  void build_string();

};

class IdGenerator {
 public:
  void generate(Identifier &output);
  Identifier generate();
  void initialize(const std::shared_ptr<Properties> & properties);

  static std::shared_ptr<IdGenerator> getIdGenerator() {
    static std::shared_ptr<IdGenerator> generator = std::shared_ptr<IdGenerator>(new IdGenerator());
    return generator;
  }
 protected:
  uint64_t getDeviceSegmentFromString(const std::string & str, int numBits) const;
  uint64_t getRandomDeviceSegment(int numBits) const;
 private:
  IdGenerator();
  int implementation_;
  std::shared_ptr<minifi::core::logging::Logger> logger_;
  unsigned char deterministic_prefix_[8];
  std::atomic<uint64_t> incrementor_;
};

class NonRepeatingStringGenerator {
 public:
  NonRepeatingStringGenerator();
  std::string generate() {
    return prefix_ + std::to_string(incrementor_++);
  }
    uint64_t generateId() {
      return incrementor_++;
    }
 private:
  std::atomic<uint64_t> incrementor_;
  std::string prefix_;
};

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_UTILS_ID_H_ */
