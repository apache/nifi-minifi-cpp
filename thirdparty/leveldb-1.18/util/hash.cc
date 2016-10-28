// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <string.h>
#include "util/coding.h"
#include "util/hash.h"

// The FALLTHROUGH_INTENDED macro can be used to annotate implicit fall-through
// between switch labels. The real definition should be provided externally.
// This one is a fallback version for unsupported compilers.
#ifndef FALLTHROUGH_INTENDED
#define FALLTHROUGH_INTENDED do { } while (0)
#endif

namespace leveldb {

uint32_t Hash(const void * data, size_t n, uint32_t seed) {

  const unsigned char * p = static_cast<const unsigned char *>(data);

  // Similar to murmur hash
  const uint32_t m = 0xc6a4a793;
  const uint32_t r = 24;
  const unsigned char * limit = p + n;
  uint32_t h = seed ^ (static_cast<uint32_t>(n) * m);

  // Pick up four bytes at a time
  while (p + 4 <= limit) {
    uint32_t w = DecodeFixed32(p);
    p += 4;
    h += w;
    h *= m;
    h ^= (h >> 16);
  }

  // Pick up remaining bytes
  switch (limit - p) {
    case 3:
      h += p[2] << 16;
      FALLTHROUGH_INTENDED;
    case 2:
      h += p[1] << 8;
      FALLTHROUGH_INTENDED;
    case 1:
      h += p[0];
      h *= m;
      h ^= (h >> r);
      break;
  }
  return h;
}


}  // namespace leveldb
