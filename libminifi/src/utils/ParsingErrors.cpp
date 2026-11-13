// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "utils/ParsingErrors.h"

namespace org::apache::nifi::minifi::core {

const ParsingErrorCategory& parsing_error_category() noexcept {
  static ParsingErrorCategory category;
  return category;
};

std::error_code make_error_code(ParsingErrorCode c) {
  return {static_cast<int>(c), parsing_error_category()};
}

}  // namespace org::apache::nifi::minifi::core
