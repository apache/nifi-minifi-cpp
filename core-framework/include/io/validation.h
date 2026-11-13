// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once
#include <type_traits>
#include <string>
#include <memory>

#include "utils/meta/detected.h"

namespace org::apache::nifi::minifi {
namespace detail {
template<typename T>
concept emptiness_checkable = requires(T t) {  // NOLINT
  { t.empty() };  // NOLINT
};  // NOLINT

template<typename T>
concept size_checkable = requires(T t) {  // NOLINT
  { t.size() };  // NOLINT
};  // NOLINT

template<typename T>
concept only_size_checkable = !emptiness_checkable<T> && size_checkable<T>;

template<typename T>
concept not_size_or_emptiness_checkable = !emptiness_checkable<T> && !size_checkable<T>;

}  // namespace detail

/**
* Determines if the variable is null or ::empty()
*/
static bool IsNullOrEmpty(detail::emptiness_checkable auto& object) {
  return object.empty();
}
/**
 * Determines if the variable is null or ::empty()
 */
static bool IsNullOrEmpty(detail::emptiness_checkable auto* object) {
  return (nullptr == object || object->empty());
}

/**
 * Determines if the variable is null or ::size() == 0
 */
static bool IsNullOrEmpty(detail::only_size_checkable auto* object) {
  return (nullptr == object || object->size() == 0);
}

/**
 * Determines if the variable is null
 */
static bool IsNullOrEmpty(detail::not_size_or_emptiness_checkable auto* object) {
  return (nullptr == object);
}

/**
* Determines if the variable is null or ::empty()
*/
template<typename T>
requires (!detail::emptiness_checkable<T>)  // NOLINT
static bool IsNullOrEmpty(std::shared_ptr<T> object) {
  return (nullptr == object || nullptr == object.get());
}

template<detail::only_size_checkable T>
static bool IsNullOrEmpty(std::shared_ptr<T> object) {
  return (nullptr == object || nullptr == object.get() || object->size() == 0);
}

}  // namespace org::apache::nifi::minifi

// TODO(szaszm): clean up this file
using org::apache::nifi::minifi::IsNullOrEmpty;
