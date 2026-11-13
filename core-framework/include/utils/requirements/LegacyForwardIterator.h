// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <iterator>
#include "LegacyInputIterator.h"

namespace org::apache::nifi::minifi::utils {

template<typename T>
struct assert_legacy_forward_iterator : assert_legacy_input_iterator<T> {
  static_assert(std::is_default_constructible<T>::value, "T is not default constructible");
  static_assert(
      std::is_same<typename std::iterator_traits<T>::reference, typename std::iterator_traits<T>::value_type&>::value
      || std::is_same<typename std::iterator_traits<T>::reference, const typename std::iterator_traits<T>::value_type&>::value
      , "reference should be either value_type& or const value_type& depending on if we want a mutable iterator");

  static_assert(std::is_same<decltype(std::declval<T>()++), T>::value, "Expected an iterator");
  static_assert(std::is_same<decltype(*std::declval<T>()++), typename std::iterator_traits<T>::reference>::value, "Expected a reference");
};

}  // namespace org::apache::nifi::minifi::utils
