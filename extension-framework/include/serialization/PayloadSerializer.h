// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include "minifi-cpp/io/OutputStream.h"
#include "FlowFileSerializer.h"

namespace org::apache::nifi::minifi {

class PayloadSerializer : public FlowFileSerializer {
 public:
  using FlowFileSerializer::FlowFileSerializer;

  io::IoResult serialize(const std::shared_ptr<core::FlowFile>& flowFile, const std::shared_ptr<io::OutputStream>& out) override;
};

}  // namespace org::apache::nifi::minifi
