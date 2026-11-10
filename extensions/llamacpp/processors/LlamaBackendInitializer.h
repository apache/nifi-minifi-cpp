// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

namespace org::apache::nifi::minifi::extensions::llamacpp::processors {

class LlamaBackendInitializer {
 public:
  static LlamaBackendInitializer& get() {
    static LlamaBackendInitializer instance;
    return instance;
  }

  ~LlamaBackendInitializer();
  LlamaBackendInitializer(const LlamaBackendInitializer&) = delete;
  LlamaBackendInitializer& operator=(const LlamaBackendInitializer&) = delete;
  LlamaBackendInitializer(LlamaBackendInitializer&&) = delete;
  LlamaBackendInitializer& operator=(LlamaBackendInitializer&&) = delete;

 private:
  LlamaBackendInitializer();
};

}  // namespace org::apache::nifi::minifi::extensions::llamacpp::processors
