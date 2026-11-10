// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "utils/AttributeErrors.h"

namespace org::apache::nifi::minifi::core {

const AttributeErrorCategory& attribute_error_category() noexcept {
  static AttributeErrorCategory category;
  return category;
};

std::error_code make_error_code(AttributeErrorCode c) {
  return {static_cast<int>(c), attribute_error_category()};
}

}  // namespace org::apache::nifi::minifi::core
