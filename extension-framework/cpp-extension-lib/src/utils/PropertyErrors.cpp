// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "utils/PropertyErrors.h"

namespace org::apache::nifi::minifi::core {

const minifi::core::PropertyErrorCategory& property_error_category() noexcept {
  static minifi::core::PropertyErrorCategory category;
  return category;
};

std::error_code make_error_code(minifi::core::PropertyErrorCode c) {
  return {static_cast<int>(c), property_error_category()};
}

}  // namespace org::apache::nifi::minifi::core
