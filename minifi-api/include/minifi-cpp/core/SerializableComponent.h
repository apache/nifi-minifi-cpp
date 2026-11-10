// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string_view>
#include <utility>

#include "core/Core.h"
#include "minifi-cpp/utils/gsl.h"
#include "minifi-cpp/io/InputStream.h"
#include "minifi-cpp/io/OutputStream.h"

namespace org::apache::nifi::minifi::core {

class SerializableComponent : public virtual core::CoreComponent {
 public:
  ~SerializableComponent() override = default;

  virtual bool serialize(io::OutputStream& output_stream) = 0;
  virtual bool deserialize(io::InputStream &input_stream) = 0;
};

}  // namespace org::apache::nifi::minifi::core
