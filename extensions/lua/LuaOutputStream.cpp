// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <memory>
#include <utility>
#include <string>

#include "LuaOutputStream.h"

namespace org::apache::nifi::minifi::extensions::lua {

LuaOutputStream::LuaOutputStream(std::shared_ptr<io::OutputStream> stream)
    : stream_(std::move(stream)) {
}

size_t LuaOutputStream::write(std::string buf) {
  return stream_->write(reinterpret_cast<const uint8_t*>(buf.data()), buf.length());
}

}  // namespace org::apache::nifi::minifi::extensions::lua
