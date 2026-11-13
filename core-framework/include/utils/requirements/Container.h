// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <iterator>
#include <utility>
#include <type_traits>
#include "LegacyForwardIterator.h"
#include "EqualityComparable.h"

namespace org::apache::nifi::minifi::utils {

template<typename T>
struct assert_container :
    assert_legacy_forward_iterator<typename T::iterator>,
    assert_legacy_forward_iterator<typename T::const_iterator> {
  static_assert(std::is_same<typename T::reference, typename T::value_type&>::value, "");
  static_assert(std::is_same<typename T::const_reference, const typename T::value_type&>::value, "");
  static_assert(std::is_same<typename std::iterator_traits<typename T::iterator>::value_type, typename T::value_type>::value, "");
  static_assert(std::is_convertible<typename T::iterator, typename T::const_iterator>::value, "");
  static_assert(std::is_same<typename std::iterator_traits<typename T::const_iterator>::value_type, const typename T::value_type>::value, "");
  static_assert(std::is_signed<typename T::difference_type>::value && std::is_integral<typename T::difference_type>::value, "");
  static_assert(std::is_same<typename T::difference_type, typename std::iterator_traits<typename T::iterator>::difference_type>::value, "");
  static_assert(std::is_same<typename T::difference_type, typename std::iterator_traits<typename T::const_iterator>::difference_type>::value, "");
  static_assert(std::is_unsigned<typename T::size_type>::value && !std::is_same<typename T::size_type, bool>::value, "");

  static_assert(std::is_default_constructible<T>::value, "");
  static_assert(std::is_copy_constructible<T>::value, "");
  static_assert(std::is_move_constructible<T>::value, "");
  static_assert(std::is_copy_assignable<T>::value, "");
  static_assert(std::is_move_assignable<T>::value, "");
  static_assert(std::is_destructible<T>::value, "");

  static_assert(std::is_same<decltype(std::declval<T&>().begin()), typename T::iterator>::value, "");
  static_assert(std::is_same<decltype(std::declval<const T&>().begin()), typename T::const_iterator>::value, "");
  static_assert(std::is_same<decltype(std::declval<T&>().end()), typename T::iterator>::value, "");
  static_assert(std::is_same<decltype(std::declval<const T&>().end()), typename T::const_iterator>::value, "");
  static_assert(std::is_same<decltype(std::declval<T&>().cbegin()), typename T::const_iterator>::value, "");
  static_assert(std::is_same<decltype(std::declval<T&>().cend()), typename T::const_iterator>::value, "");

  static_assert(is_equality_comparable<typename T::value_type>::value, "");
  static_assert(std::is_convertible<decltype(std::declval<T&>() == std::declval<T&>()), bool>::value, "");
  static_assert(std::is_convertible<decltype(std::declval<T&>() != std::declval<T&>()), bool>::value, "");

  static_assert(std::is_same<decltype(std::declval<T&>().swap(std::declval<T&>())), void>::value, "Member swap");
  static_assert(std::is_same<decltype(swap(std::declval<T&>(), std::declval<T&>())), void>::value, "ADL swap");
  static_assert(std::is_same<decltype(std::declval<T&>().size()), typename T::size_type>::value, "");
  static_assert(std::is_same<decltype(std::declval<T&>().max_size()), typename T::size_type>::value, "");

  static_assert(std::is_convertible<decltype(std::declval<T&>().empty()), bool>::value, "");
};

}  // namespace org::apache::nifi::minifi::utils
