// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <stdexcept>
#include <memory>
#include <utility>
#include <string>

#include "LuaInputStream.h"

namespace org::apache::nifi::minifi::extensions::lua {

LuaInputStream::LuaInputStream(std::shared_ptr<io::InputStream> stream)
    : stream_(std::move(stream)) {
}

std::string LuaInputStream::read(size_t len) {
  if (len == 0) {
    len = stream_->size();
  }

  if (len <= 0) {
    return std::string{};
  }

  std::string buffer;
  buffer.resize(len);

  // Here, we write directly to the string data, which is safe starting in C++ 11:
  //
  //     The char-like objects in a basic_string object shall be stored contiguously. That is, for any basic_string
  //     object s, the identity &*(s.begin() + n) == &*s.begin() + n shall hold for all values of n such that
  //     0 <= n < s.size()."
  //
  // http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3337.pdf
  const auto read = stream_->read(as_writable_bytes(std::span(buffer)));
  if (!io::isError(read) && read != len) {
    buffer.resize(read);
  }
  return io::isError(read) ? std::string{} : buffer;
}

}  // namespace org::apache::nifi::minifi::extensions::lua
