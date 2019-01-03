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
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <uuid/uuid.h>
#include <properties/Configure.h>

#ifdef WIN32
#pragma comment(lib, "shlwapi.lib")
#endif

#if defined _WIN32 || defined __CYGWIN__
#ifdef BUILDING_DLL
#ifdef __GNUC__
#define DLL_PUBLIC __attribute__ ((dllexport))
#else
#define DLL_PUBLIC __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
#endif
#else
#ifdef __GNUC__
#define DLL_PUBLIC __attribute__ ((dllimport))
#else
#define DLL_PUBLIC __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
#endif
#endif
#define DLL_LOCAL
#else
#if __GNUC__ >= 4
#define DLL_PUBLIC __attribute__ ((visibility ("default")))
#define DLL_LOCAL  __attribute__ ((visibility ("hidden")))
#else
#define DLL_PUBLIC
#define DLL_LOCAL
#endif
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN 
#define WIN32_LEAN_AND_MEAN 1
#endif
// can't include cxxabi
#else
#include <cxxabi.h>
#endif

#include "utils/Id.h"

/**
 * namespace aliasing
 */
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace file {
}
}
namespace processors {
}
namespace provenance {

}
namespace core {

template<typename T>
static inline std::string getClassName() {
#ifndef WIN32
  char *b = abi::__cxa_demangle(typeid(T).name(), 0, 0, 0);
  if (b == nullptr)
    return std::string();
  std::string name = b;
  std::free(b);
  return name;
#else
  return typeid(T).name();
#endif
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
  if (name.length() == 0) {
    return std::make_shared<T>();
  } else {
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

  explicit CoreComponent(const std::string &name, utils::Identifier uuid)
      : name_(name) {
    if (uuid == nullptr) {
      // Generate the global UUID for the flow record
      id_generator_->generate(uuid_);
    } else {
      uuid_ = uuid;
    }
    uuidStr_ = uuid_.to_string();
  }

  explicit CoreComponent(const std::string &name)
      : name_(name) {
    // Generate the global UUID for the flow record
    id_generator_->generate(uuid_);
    uuidStr_ = uuid_.to_string();
  }

  explicit CoreComponent(const CoreComponent &other) = default;
  /**
   * Move Constructor.
   */

  explicit CoreComponent(CoreComponent &&other) = default;

  virtual ~CoreComponent() {

  }

  // Get component name Name
  virtual std::string getName() const;

  /**
   * Set name.
   * @param name
   */
  void setName(const std::string &name);

  /**
   * Set UUID in this instance
   * @param uuid uuid to apply to the internal representation.
   */
  void setUUID(utils::Identifier &uuid);

  void setUUIDStr(const std::string &uuidStr);

  /**
   * Returns the UUID through the provided object.
   * @param uuid uuid struct to which we will copy the memory
   * @return success of request
   */
  bool getUUID(utils::Identifier &uuid) const;

  //unsigned const char *getUUID();
  /**
   * Return the UUID string
   * @param constant reference to the UUID str
   */
  const std::string & getUUIDStr() const {
    return uuidStr_;
  }

  virtual void configure(const std::shared_ptr<Configure> &configuration) {

  }

  void loadComponent() {
  }

protected:
  // A global unique identifier
  utils::Identifier uuid_;
  // UUID string
  std::string uuidStr_;

  // Connectable's name
  std::string name_;

private:
  static std::shared_ptr<utils::IdGenerator> id_generator_;
};

namespace logging {
}
}
}
}
}
}

namespace fileutils = org::apache::nifi::minifi::utils::file;

namespace minifi = org::apache::nifi::minifi;

namespace core = org::apache::nifi::minifi::core;

namespace processors = org::apache::nifi::minifi::processors;

namespace logging = org::apache::nifi::minifi::core::logging;

namespace utils = org::apache::nifi::minifi::utils;

namespace provenance = org::apache::nifi::minifi::provenance;

#endif /* LIBMINIFI_INCLUDE_CORE_CORE_H_ */
