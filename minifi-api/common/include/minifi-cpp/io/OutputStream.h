// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <stdexcept>
#include <vector>
#include <string>
#include "Stream.h"
#include "minifi-cpp/utils/gsl.h"
#include "utils/SmallString.h"
#include "minifi-cpp/utils/Id.h"

namespace org::apache::nifi::minifi::io {

/**
 * Serializable instances provide base functionality to
 * write certain objects/primitives to a data stream.
 *
 */
class OutputStream : public virtual Stream {
 public:
  /**
   * write buffer to stream
   * @param value non encoded value
   * @param len length of value
   * @return resulting write size
   **/
  virtual size_t write(const uint8_t *value, size_t len) = 0;

  size_t write(const std::span<const std::byte> buffer) {
    return write(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size());
  }

  /**
   * write: resolve nullptr ambiguity
   * call: write(nullptr, 0)
   * candidate1: write(const uint8_t*, size_t): conversion from std::nullptr_t to const uint8_t* and from int to size_t
   * candidate2: write(const char*, bool): conversion from std::nullptr_t to const char* and from int to bool
   * @param len
   * @return
   */
  size_t write(std::nullptr_t, size_t len) {
    return write(static_cast<const uint8_t*>(nullptr), len);
  }

  size_t write(const std::vector<uint8_t>& buffer, size_t len);

  /**
   * write bool to stream
   * @param value non encoded value
   * @return resulting write size
   **/
  size_t write(bool value);

  /**
   * write Identifier to stream
   * @param value non encoded value
   * @return resulting write size
   **/
  size_t write(const utils::Identifier& value);

  /**
   * write string to stream
   * @param str string to write
   * @return resulting write size
   **/
  size_t write(const std::string& str, bool widen = false);

  /**
   * write string to stream
   * @param str string to write
   * @return resulting write size
   **/
  size_t write(const char* str, bool widen = false);

  template<size_t N>
  size_t write(const utils::SmallString<N>& str, bool widen = false) {
    return write(str.c_str(), widen);
  }

  /**
  * writes sizeof(Integral) bytes to the stream
  * @param value to write
  * @return resulting write size
  **/
  template<typename Integral, typename = std::enable_if_t<std::is_unsigned<Integral>::value && !std::is_same<Integral, bool>::value>>
  size_t write(Integral value) {
    uint8_t buffer[sizeof(Integral)]{};

    for (std::size_t byteIdx = 0; byteIdx < sizeof(Integral); ++byteIdx) {
      buffer[byteIdx] = gsl::narrow_cast<uint8_t>(value >> (8*(sizeof(Integral) - 1) - 8*byteIdx));
    }

    return write(buffer, sizeof(Integral));
  }

 private:
  size_t write_str(const char* str, uint32_t len, bool widen);
};

}  // namespace org::apache::nifi::minifi::io
