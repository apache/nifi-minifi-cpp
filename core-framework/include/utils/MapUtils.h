// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <set>

namespace org::apache::nifi::minifi::utils::MapUtils {

/**
 * Return a set of keys from a map
 */
template<template <typename...> class Map, typename T, typename U>
std::set<T> getKeys(const Map<T, U>& m) {
  std::set<T> keys;
  for (const auto& pair : m) {
    keys.insert(pair.first);
  }
  return keys;
}

}  // namespace org::apache::nifi::minifi::utils::MapUtils
