/**
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
#ifndef LIBMINIFI_INCLUDE_UTILS_DIFFUTILS_H_
#define LIBMINIFI_INCLUDE_UTILS_DIFFUTILS_H_

#include <sstream>
#include <fstream>
#ifdef BOOST_VERSION
#include <boost/filesystem.hpp>
#else
#include <cstdlib>
#include <sys/stat.h>
#endif
#include <cstdio>
#include <fcntl.h>
#ifdef WIN32
#define stat _stat
#endif

#ifdef BDIFF

extern "C"
{
#include "bsdiff.h"
#include "bspatch.h"
}
#else
int apply_bsdiff_patch(const char *oldfile, const char *newfile, const char *patch) {
  return -1;
}

#endif

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace file {

/**
 * Simple implementation of some file system utilities.
 *
 */
class DiffUtils {
 public:

  DiffUtils() = delete;

  static int apply_binary_diff(const char *file_original, const char *file_new, const char *result_file) {
    return apply_bsdiff_patch(file_original, file_new, result_file);
  }

  static int binary_diff(const char *file_original, const char *file_other, const char *patchfile) {
    return -1;
  }

};

} /* namespace file */
} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_UTILS_DIFFUTILS_H_ */
