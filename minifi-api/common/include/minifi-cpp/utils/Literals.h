// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <cstdint>

constexpr uint64_t operator""_KiB(const unsigned long long n) {  // NOLINT(runtime/int)
  return 1024 * n;
}

constexpr uint64_t operator""_MiB(const unsigned long long n) {  // NOLINT(runtime/int)
  return 1024_KiB * n;
}

constexpr uint64_t operator""_GiB(const unsigned long long n) {  // NOLINT(runtime/int)
  return 1024_MiB * n;
}

constexpr uint64_t operator""_TiB(const unsigned long long n) {  // NOLINT(runtime/int)
  return 1024_GiB * n;
}

constexpr uint64_t operator""_PiB(const unsigned long long n) {  // NOLINT(runtime/int)
  return 1024_TiB * n;
}

constexpr uint64_t operator""_KB(const unsigned long long n) {  // NOLINT(runtime/int)
  return 1000 * n;
}

constexpr uint64_t operator""_MB(const unsigned long long n) {  // NOLINT(runtime/int)
  return 1000_KB * n;
}

constexpr uint64_t operator""_GB(const unsigned long long n) {  // NOLINT(runtime/int)
  return 1000_MB * n;
}

constexpr uint64_t operator""_TB(const unsigned long long n) {  // NOLINT(runtime/int)
  return 1000_GB * n;
}

constexpr uint64_t operator""_PB(const unsigned long long n) {  // NOLINT(runtime/int)
  return 1000_TB * n;
}
