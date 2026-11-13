// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include "minifi-cpp/io/InputStream.h"
#include "Stream.h"

namespace org::apache::nifi::minifi::io {

class InputStreamImpl : public virtual InputStream, public StreamImpl {
 public:
  [[nodiscard]] virtual size_t size() const {
    throw std::runtime_error("Querying size is not supported");
  }
};

}  // namespace org::apache::nifi::minifi::io
