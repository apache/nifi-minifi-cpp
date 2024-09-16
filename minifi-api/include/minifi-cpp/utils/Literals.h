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

#pragma once

constexpr unsigned long long operator "" _KiB(unsigned long long n) {  // NOLINT
  return 1024 * n;
}

constexpr unsigned long long operator "" _MiB(unsigned long long n) {  // NOLINT
  return 1024_KiB * n;
}

constexpr unsigned long long operator "" _GiB(unsigned long long n) {  // NOLINT
  return 1024_MiB * n;
}

constexpr unsigned long long operator "" _TiB(unsigned long long n) {  // NOLINT
  return 1024_GiB * n;
}

constexpr unsigned long long operator "" _PiB(unsigned long long n) {  // NOLINT
  return 1024_TiB * n;
}

constexpr unsigned long long operator "" _KB(unsigned long long n) {  // NOLINT
  return 1000 * n;
}

constexpr unsigned long long operator "" _MB(unsigned long long n) {  // NOLINT
  return 1000_KB * n;
}

constexpr unsigned long long operator "" _GB(unsigned long long n) {  // NOLINT
  return 1000_MB * n;
}

constexpr unsigned long long operator "" _TB(unsigned long long n) {  // NOLINT
  return 1000_GB * n;
}

constexpr unsigned long long operator "" _PB(unsigned long long n) {  // NOLINT
  return 1000_TB * n;
}
