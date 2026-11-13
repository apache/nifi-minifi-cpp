// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include "io/OutputStream.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::io {

size_t OutputStream::write(const std::vector<uint8_t>& buffer, size_t len) {
  if (buffer.size() < len) {
    return STREAM_ERROR;
  }
  return write(buffer.data(), len);
}

size_t OutputStream::write(bool value) {
  uint8_t temp = value;
  return write(&temp, 1);
}

size_t OutputStream::write(const utils::Identifier &value) {
  return write(value.to_string());
}

size_t OutputStream::write(const std::string& str, bool widen) {
  return write_str(str.c_str(), gsl::narrow<uint32_t>(str.length()), widen);
}

size_t OutputStream::write(const char* str, bool widen) {
  return write_str(str, gsl::narrow<uint32_t>(std::strlen(str)), widen);
}

size_t OutputStream::write_str(const char* str, uint32_t len, bool widen) {
  size_t ret = 0;
  if (!widen) {
    const auto shortLen = gsl::narrow_cast<uint16_t>(len);
    if (len != shortLen) {
      return STREAM_ERROR;
    }
    ret = write(shortLen);
  } else {
    ret = write(len);
  }

  if (ret == 0 || isError(ret)) {
    return ret;
  }

  if (len == 0) {
    return ret;
  }

  return ret + write(reinterpret_cast<const uint8_t *>(str), len);
}

}  // namespace org::apache::nifi::minifi::io
