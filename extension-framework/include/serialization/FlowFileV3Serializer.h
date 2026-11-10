// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <limits>
#include <string>
#include <memory>
#include "io/OutputStream.h"
#include "FlowFileSerializer.h"

namespace org::apache::nifi::minifi {

class FlowFileV3Serializer : public FlowFileSerializer {
  static constexpr uint8_t MAGIC_HEADER[] = {'N', 'i', 'F', 'i', 'F', 'F', '3'};

  static constexpr uint16_t MAX_2_BYTE_VALUE = (std::numeric_limits<uint16_t>::max)();

  static size_t writeLength(std::size_t length, const std::shared_ptr<io::OutputStream>& out);

  static size_t writeString(const std::string& str, const std::shared_ptr<io::OutputStream>& out);

 public:
  using FlowFileSerializer::FlowFileSerializer;

  io::IoResult serialize(const std::shared_ptr<core::FlowFile>& flowFile, const std::shared_ptr<io::OutputStream>& out) override;
};

}  // namespace org::apache::nifi::minifi
