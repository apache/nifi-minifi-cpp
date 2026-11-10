// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "core/flow/Node.h"
#include "fmt/format.h"

namespace org::apache::nifi::minifi::core::flow {

Node::Iterator::Value Node::Iterator::operator*() const {
  Value value = impl_->operator*();
  if (value) {
    // sequence iterator
    value.impl_->path_ = utils::string::join_pack(impl_->path_, "/", std::to_string(idx_));
  } else if (value.second) {
    // map iterator
    value.second.impl_->path_ = utils::string::join_pack(impl_->path_, "/", value.first.getString().value());
  }
  return value;
}

}  // namespace org::apache::nifi::minifi::core::flow
