// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <type_traits>

namespace org::apache::nifi::minifi::utils {

template<typename T, typename = void>
struct is_equality_comparable : std::false_type {};

template<typename T>
struct is_equality_comparable<T,
    typename std::enable_if<
      std::is_convertible<decltype(std::declval<const T>() == std::declval<const T>()), bool>::value
    >::type> : std::true_type {};

}  // namespace org::apache::nifi::minifi::utils
