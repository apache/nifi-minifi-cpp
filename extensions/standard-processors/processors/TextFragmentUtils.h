// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include "minifi-cpp/utils/Export.h"

namespace org::apache::nifi::minifi::processors::textfragmentutils {
  constexpr const char* BASE_NAME_ATTRIBUTE = "TextFragmentAttribute.base_name";
  constexpr const char* POST_NAME_ATTRIBUTE = "TextFragmentAttribute.post_name";
  constexpr const char* OFFSET_ATTRIBUTE = "TextFragmentAttribute.offset";

  inline std::string createFileName(const std::string& base_name, const std::string& post_name, const size_t offset, const size_t size) {
    return base_name + "." + std::to_string(offset) + "-" + std::to_string(offset + size - 1) + "." + post_name;
  }
}  // namespace org::apache::nifi::minifi::processors::textfragmentutils
