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

#undef NDEBUG
#include <cassert>
#include <cstdlib>
#include "utils/FileMutex.h"
#include "utils/file/FileUtils.h"
#include "../TestBase.h"

namespace minifi = org::apache::nifi::minifi;

int main(int argc, char* argv[]) {
  if (argc == 2) {
    std::cout << "Trying to lock file a second time '" << argv[1] << "'" << std::endl;
    minifi::utils::FileMutex mtx{argv[1]};
    std::unique_lock lock{mtx};
    return 0;
  }

  TestController controller;
  auto lock_file = controller.createTempDirectory() / "LOCK";

  std::cout << "Locking file the first time '" << lock_file << "'" << std::endl;
  minifi::utils::FileMutex mtx{lock_file};
  std::unique_lock lock{mtx};

  int second_lock = std::system((utils::file::get_executable_path().string() + " " + lock_file.string()).c_str());

  assert(second_lock != 0);
}
