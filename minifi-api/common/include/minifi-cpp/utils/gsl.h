// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <type_traits>

#include <gsl-lite/gsl-lite.hpp>

namespace gsl = ::gsl_lite;

namespace org::apache::nifi::minifi {

namespace utils {

template<typename T>
struct is_not_null : std::false_type {};
template<typename T>
struct is_not_null<gsl::not_null<T>> : std::true_type {};
template<typename T>
inline constexpr bool is_not_null_v = is_not_null<T>::value;

}  // namespace utils

}  // namespace org::apache::nifi::minifi
