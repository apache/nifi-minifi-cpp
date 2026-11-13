// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <utility>
#include <functional>
#include "minifi-cpp/io/StreamCallback.h"

namespace org::apache::nifi::minifi {
namespace io {

class OutputStream;

} /* namespace io */

namespace core {

class FlowFile;

} /* namespace core */

class FlowFileSerializer {
 public:
  using FlowFileReader = std::function<io::IoResult(const std::shared_ptr<core::FlowFile>&, const io::InputStreamCallback&)>;

  explicit FlowFileSerializer(FlowFileReader reader) : reader_(std::move(reader)) {}

  virtual io::IoResult serialize(const std::shared_ptr<core::FlowFile>& flowFile, const std::shared_ptr<io::OutputStream>& out) = 0;

  virtual ~FlowFileSerializer() = default;

 protected:
  FlowFileReader reader_;
};

}  // namespace org::apache::nifi::minifi
