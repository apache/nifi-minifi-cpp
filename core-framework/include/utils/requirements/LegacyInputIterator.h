// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <utility>
#include <iterator>
#include "LegacyIterator.h"
#include "EqualityComparable.h"

namespace org::apache::nifi::minifi::utils {

template<typename T>
struct assert_legacy_input_iterator : assert_legacy_iterator<T> {
  static_assert(is_equality_comparable<T>::value, "");
  static_assert(std::is_same<decltype(*std::declval<const T>()), typename std::iterator_traits<T>::reference>::value, "");
  static_assert(std::is_same<decltype(*std::declval<const T>().T::operator->()), typename std::iterator_traits<T>::reference>::value, "");
  static_assert(std::is_convertible<decltype(*std::declval<T>()++), typename std::iterator_traits<T>::value_type>::value, "");

 private:
  // checking for contextual convertibility
  using _ = decltype(true || (std::declval<const T>() != std::declval<const T>()));
};

}  // namespace org::apache::nifi::minifi::utils
