// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "core/Core.h"
#include <memory>
#include <string>

namespace org::apache::nifi::minifi::core {

CoreComponentImpl::CoreComponentImpl(std::string_view name, const utils::Identifier& uuid, const std::shared_ptr<utils::IdGenerator>& idGenerator)
    : name_(name) {
  if (uuid.isNil()) {
    uuid_ = idGenerator->generate();
  } else {
    uuid_ = uuid;
  }
}

void CoreComponentImpl::setUUID(const utils::Identifier& uuid) {
  uuid_ = uuid;
}

utils::Identifier CoreComponentImpl::getUUID() const {
  return uuid_;
}

void CoreComponentImpl::setName(std::string name) {
  name_ = std::move(name);
}

std::string CoreComponentImpl::getName() const {
  return name_;
}

} /* namespace org::apache::nifi::minifi::core */
