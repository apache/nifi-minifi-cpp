/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "utils/tls/TLSUtils.h"

#include <cstring>
#include <string>

#include "utils/gsl.h"
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
