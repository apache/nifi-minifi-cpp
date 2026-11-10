// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "core/Repository.h"

namespace org::apache::nifi::minifi::core {

bool RepositoryImpl::Delete(std::vector<std::shared_ptr<core::SerializableComponent>> &storedValues) {
  bool found = true;
  for (const auto& storedValue : storedValues) {
    found &= Delete(storedValue->getName());
  }
  return found;
}

}  // namespace org::apache::nifi::minifi::core
