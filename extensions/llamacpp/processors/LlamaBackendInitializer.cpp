// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "LlamaBackendInitializer.h"
#include "llama.h"

namespace org::apache::nifi::minifi::extensions::llamacpp::processors {

LlamaBackendInitializer::LlamaBackendInitializer() {
  llama_backend_init();
}

LlamaBackendInitializer::~LlamaBackendInitializer() {
  llama_backend_free();
}

}  // namespace org::apache::nifi::minifi::extensions::llamacpp::processors
