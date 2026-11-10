// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "utils/tls/TLSUtils.h"

#include <cstring>
#include <string>

#include "minifi-cpp/utils/gsl.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::utils::tls {

int pemPassWordCb(char *buf, int size, int /*rwflag*/, void *userdata) {
  const std::string * const origPass = reinterpret_cast<std::string*>(userdata);
  // make copy the password, trimming it
  const auto pass = utils::string::trimRight(*origPass);
  /**
   * 1) Ensure that password is trimmed.
   * 2) Don't attempt a larger copy than the buffer we are allowed.
   * validation of the key will subsequently occur, so any truncation
   * would be caught, but we should be defensive here *
   */
  if (size > 0 && pass.size() <= static_cast<size_t>(size)) {
    // Leaving this paradigm here. I don't think this is necessary
    // while this string will be null terminated, OpenSSL does not expect
    // a null terminator. Per their documentation:
    // "The actual length of the password must be returned to the calling function. "
    memset(buf, 0x00, size);
    return gsl::narrow<int>(pass.copy(buf, size));
  }
  return -1;
}

}  // namespace org::apache::nifi::minifi::utils::tls
