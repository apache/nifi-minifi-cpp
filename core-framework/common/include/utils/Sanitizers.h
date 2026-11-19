// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

namespace org::apache::nifi::minifi::utils {

#if defined(__has_feature)
#  if __has_feature(address_sanitizer)  // Clang and GCC >= 14
constexpr bool AddressSanitizerEnabled = true;
#  else
constexpr bool AddressSanitizerEnabled = false;
#  endif
#elif defined(__SANITIZE_ADDRESS__)  // GCC <= 13 and MSVC
constexpr bool AddressSanitizerEnabled = true;
#else
constexpr bool AddressSanitizerEnabled = false;
#endif

#if defined(__has_feature)
#  if __has_feature(thread_sanitizer)  // Clang and GCC >= 14
constexpr bool ThreadSanitizerEnabled = true;
#  else
constexpr bool ThreadSanitizerEnabled = false;
#  endif
#elif defined(__SANITIZE_THREAD__)  // GCC <= 13 and MSVC
constexpr bool ThreadSanitizerEnabled = true;
#else
constexpr bool ThreadSanitizerEnabled = false;
#endif

}  // namespace org::apache::nifi::minifi::utils
