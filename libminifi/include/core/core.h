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
#ifndef LIBMINIFI_INCLUDE_CORE_CORE_H_
#define LIBMINIFI_INCLUDE_CORE_CORE_H_
/**
 * namespace aliasing
 */
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
}
namespace processors {
}
namespace provenance {

}
namespace core {

template<typename T>
static inline std::string getClassName() {
  std::string name = abi::__cxa_demangle(typeid(T).name(), 0, 0, 0);
  return name;
}

/**
 * Base component within MiNiFi
 * Purpose: Many objects store a name and UUID, therefore
 * the functionality is localized here to avoid duplication
 */
class CoreComponent {

 public:

  /**
   * Constructor that sets the name and uuid.
   */
  explicit CoreComponent(const std::string name, uuid_t uuid = 0)
      : logger_(logging::Logger::getLogger()),
        name_(name) {
    if (!uuid)
      // Generate the global UUID for the flow record
      uuid_generate(uuid_);
    else
      uuid_copy(uuid_, uuid);

    char uuidStr[37];
    uuid_unparse_lower(uuid_, uuidStr);
    uuidStr_ = uuidStr;
  }

  /**
   * Move Constructor.
   */
  explicit CoreComponent(const CoreComponent &&other)
      : name_(std::move(other.name_)),
        logger_(logging::Logger::getLogger()) {
    uuid_copy(uuid_, other.uuid_);
  }

  // Get component name Name
  std::string getName() const;

  /**
   * Set name.
   * @param name
   */
  void setName(const std::string name);

  /**
   * Set UUID in this instance
   * @param uuid uuid to apply to the internal representation.
   */
  void setUUID(uuid_t uuid);

  /**
   * Returns the UUID through the provided object.
   * @param uuid uuid struct to which we will copy the memory
   * @return success of request
   */
  bool getUUID(uuid_t uuid) const;

  unsigned const char *getUUID() const;
  /**
   * Return the UUID string
   * @param constant reference to the UUID str
   */
  const std::string & getUUIDStr() const {
    return uuidStr_;
  }

 protected:
  // A global unique identifier
  uuid_t uuid_;
  // UUID string
  std::string uuidStr_;

  // logger shared ptr
  std::shared_ptr<logging::Logger> logger_;

  // Connectable's name
  std::string name_;
};

namespace logging {
}
}
}
}
}
}

namespace minifi = org::apache::nifi::minifi;

namespace core = org::apache::nifi::minifi::core;

namespace processors = org::apache::nifi::minifi::processors;

namespace logging = org::apache::nifi::minifi::core::logging;

namespace utils = org::apache::nifi::minifi::utils;

namespace provenance = org::apache::nifi::minifi::provenance;

#endif /* LIBMINIFI_INCLUDE_CORE_CORE_H_ */
