// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <iostream>
#include <filesystem>

namespace org::apache::nifi::minifi::docs {

class AgentDocs {
 public:
  static void generate(const std::filesystem::path& docs_dir);
  static void generateManifest();
};

}  // namespace org::apache::nifi::minifi::docs
