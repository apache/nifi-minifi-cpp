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

#include <memory>
#include <string>
#include <string_view>

#ifdef WIN32
#pragma comment(lib, "shlwapi.lib")
#endif

#if defined _WIN32 || defined __CYGWIN__
#ifdef BUILDING_DLL
#ifdef __GNUC__
#define DLL_PUBLIC __attribute__ ((dllexport))
#else
#define DLL_PUBLIC __declspec(dllexport)  // Note: actually gcc seems to also supports this syntax.
#endif
#else
#ifdef __GNUC__
#define DLL_PUBLIC __attribute__ ((dllimport))
#else
#define DLL_PUBLIC __declspec(dllimport)  // Note: actually gcc seems to also supports this syntax.
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
#include "properties/Configure.h"
#include "utils/StringUtils.h"

/**
 * namespace aliasing
 */
namespace org::apache::nifi::minifi::core {

template<typename T>
static inline std::string getClassName() {
#ifndef WIN32
  char *b = abi::__cxa_demangle(typeid(T).name(), 0, 0, 0);
  if (b == nullptr)
    return {};
  std::string name = b;
  std::free(b);
  return name;
#else
  std::string_view name = typeid(T).name();
  const std::string_view class_prefix = "class ";
  const std::string_view struct_prefix = "struct ";

  if (utils::StringUtils::startsWith(name, class_prefix)) {
    name.remove_prefix(class_prefix.length());
  } else if (utils::StringUtils::startsWith(name, struct_prefix)) {
    name.remove_prefix(struct_prefix.length());
  }
  return std::string{name};
#endif
}

template<typename T>
std::unique_ptr<T> instantiate(const std::string name = {}) {
  if (name.empty()) {
    return std::make_unique<T>();
  } else {
    return std::make_unique<T>(name);
  }
}

/**
 * Base component within MiNiFi
 * Purpose: Many objects store a name and UUID, therefore
 * the functionality is localized here to avoid duplication
 */
class CoreComponent {
 public:
  explicit CoreComponent(std::string name, const utils::Identifier &uuid = {}, const std::shared_ptr<utils::IdGenerator> &idGenerator = utils::IdGenerator::getIdGenerator());
  CoreComponent(const CoreComponent &other) = default;
  CoreComponent(CoreComponent &&other) = default;
  CoreComponent& operator=(const CoreComponent&) = default;
  CoreComponent& operator=(CoreComponent&&) = default;

  virtual ~CoreComponent() = default;

  // Get component name
  [[nodiscard]] virtual std::string getName() const;

  /**
   * Set name.
   * @param name
   */
  virtual void setName(std::string name);

  /**
   * Set UUID in this instance
   * @param uuid uuid to apply to the internal representation.
   */
  virtual void setUUID(const utils::Identifier& uuid);

  /**
   * Returns the UUID.
   * @return the uuid of the component
   */
  [[nodiscard]] utils::Identifier getUUID() const;

  /**
   * Return the UUID string
   */
  [[nodiscard]] utils::SmallString<36> getUUIDStr() const {
    return uuid_.to_string();
  }

  virtual void configure(const std::shared_ptr<Configure>& /*configuration*/) {
  }

  void loadComponent() {
  }

 protected:
  // A global unique identifier
  utils::Identifier uuid_;

  // CoreComponent's name
  std::string name_;
};

}  // namespace org::apache::nifi::minifi::core

#endif  // LIBMINIFI_INCLUDE_CORE_CORE_H_
