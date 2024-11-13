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
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

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

#include "utils/ArrayUtils.h"
#include "utils/Id.h"
#include "properties/Configure.h"
#include "utils/StringUtils.h"

#if defined(_MSC_VER)
constexpr std::string_view removeStructOrClassPrefix(std::string_view input) {
  using namespace std::literals;
  for (auto prefix : { "struct "sv, "class "sv }) {
    if (input.find(prefix) == 0) {
      return input.substr(prefix.size());
    }
  }
  return input;
}
#endif

// based on https://bitwizeshift.github.io/posts/2021/03/09/getting-an-unmangled-type-name-at-compile-time/
template<typename T>
constexpr auto typeNameArray() {  // In root namespace to avoid gcc-13 optimizing out namespaces from __PRETTY_FUNCTION__
#if defined(__clang__)
  constexpr auto prefix   = std::string_view{"[T = "};
  constexpr auto suffix   = std::string_view{"]"};
  constexpr auto function = std::string_view{__PRETTY_FUNCTION__};
#elif defined(__GNUC__)
  constexpr auto prefix   = std::string_view{"with T = "};
  constexpr auto suffix   = std::string_view{"]"};
  constexpr auto function = std::string_view{__PRETTY_FUNCTION__};
#elif defined(_MSC_VER)
  constexpr auto prefix   = std::string_view{"typeNameArray<"};
  constexpr auto suffix   = std::string_view{">(void)"};
  constexpr auto function = std::string_view{__FUNCSIG__};
#else
# error Unsupported compiler
#endif

  constexpr auto start = function.find(prefix) + prefix.size();
  constexpr auto end = function.rfind(suffix);
  static_assert(start < end);

#if defined(_MSC_VER)
  constexpr auto name = removeStructOrClassPrefix(function.substr(start, end - start));
#else
  constexpr auto name = function.substr(start, end - start);
#endif

  return org::apache::nifi::minifi::utils::string_view_to_array<name.length()>(name);
}

namespace org::apache::nifi::minifi::core {

template<typename T>
struct TypeNameHolder {
  static constexpr auto value = typeNameArray<T>();
};

template<typename T>
constexpr std::string_view className() {
  return utils::array_to_string_view(TypeNameHolder<std::remove_reference_t<T>>::value);
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
  explicit CoreComponent(std::string_view name, const utils::Identifier &uuid = {}, const std::shared_ptr<utils::IdGenerator> &idGenerator = utils::IdGenerator::getIdGenerator());
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

 protected:
  // A global unique identifier
  utils::Identifier uuid_;

  // CoreComponent's name
  std::string name_;
};

}  // namespace org::apache::nifi::minifi::core
