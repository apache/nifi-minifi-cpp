// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

namespace org::apache::nifi::minifi::utils::internal {

template<class T, class U>
bool cast_if_in_range(T in, U& out) {
  U result = static_cast<U>(in);
  T result_back = static_cast<T>(result);
  constexpr const bool is_different_signedness = (std::is_signed<T>::value != std::is_signed<U>::value);
  if (result_back != in || (is_different_signedness && ((in < T{}) != (result < U{})))) {
    return false;
  }
  out = result;
  return true;
}

}  // namespace org::apache::nifi::minifi::utils::internal
