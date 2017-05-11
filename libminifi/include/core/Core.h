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

#include <memory>
#include <uuid/uuid.h>
#include <cxxabi.h>
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
  char *b = abi::__cxa_demangle(typeid(T).name(), 0, 0, 0);
  if (b == nullptr)
    return std::string();
  std::string name = b;
  std::free(b);
  return name;
}

template<typename T>
struct class_operations {

  template<typename Q = T>
  static std::true_type canDestruct(decltype(std::declval<Q>().~Q()) *) {
    return std::true_type();
  }

  template<typename Q = T>
  static std::false_type canDestruct(...) {
    return std::false_type();
  }

  typedef decltype(canDestruct<T>(0)) type;

  static const bool value = type::value;
};

template<typename T>
typename std::enable_if<!class_operations<T>::value, std::shared_ptr<T>>::type instantiate(const std::string name = "") {
  throw std::runtime_error("Cannot instantiate class");
}

template<typename T>
typename std::enable_if<class_operations<T>::value, std::shared_ptr<T>>::type instantiate(const std::string name = "") {
  if (name.length() == 0){
    return std::make_shared<T>();
  }
  else{
    return std::make_shared<T>(name);
  }
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
      : name_(name) {
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
      : name_(std::move(other.name_)) {
    uuid_copy(uuid_, other.uuid_);
  }

  // Get component name Name
  std::string getName();

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
  bool getUUID(uuid_t uuid);

  unsigned const char *getUUID();
  /**
   * Return the UUID string
   * @param constant reference to the UUID str
   */
  const std::string & getUUIDStr() {
    return uuidStr_;
  }

  void loadComponent() {
  }

 protected:
  // A global unique identifier
  uuid_t uuid_;
  // UUID string
  std::string uuidStr_;

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
