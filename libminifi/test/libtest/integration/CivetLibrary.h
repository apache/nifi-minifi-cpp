// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <atomic>
#include "civetweb.h"

class CivetLibrary {
 public:
  CivetLibrary() {
    if (getCounter()++ == 0) {
      mg_init_library(0);
    }
  }
  ~CivetLibrary() {
    if (--getCounter() == 0) {
      mg_exit_library();
    }
  }
 private:
  static std::atomic<int>& getCounter() {
    static std::atomic<int> counter{0};
    return counter;
  }
};
