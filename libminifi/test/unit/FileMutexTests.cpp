// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#undef NDEBUG
#include <cassert>
#include <cstdlib>
#include "utils/FileMutex.h"
#include "utils/file/FileUtils.h"
#include "utils/OsUtils.h"
#include "unit/TestBase.h"

namespace minifi = org::apache::nifi::minifi;

int main(int argc, char* argv[]) {
  if (argc == 2) {
    std::cout << "Trying to lock file a second time '" << argv[1] << "', from pid: " << utils::OsUtils::getCurrentProcessId() << std::endl;
    minifi::utils::FileMutex mtx{argv[1]};
    try {
      std::unique_lock lock{mtx};
    } catch (const std::exception& ex) {
      std::cerr << ex.what() << std::endl;
      throw;
    }
    return 0;
  }

  TestController controller;
  auto lock_file = controller.createTempDirectory() / "LOCK";

  minifi::utils::FileMutex mtx{lock_file};
  {
    std::cout << "Locking file the first time '" << lock_file << "', from pid: " << utils::OsUtils::getCurrentProcessId() << std::endl;
    std::unique_lock lock{mtx};

    int second_lock = std::system((utils::file::get_executable_path().string() + " " + lock_file.string()).c_str());  // NOLINT

    assert(second_lock != 0);
  }
  // unlocked file the other process can lock now
  {
    int second_lock = std::system((utils::file::get_executable_path().string() + " " + lock_file.string()).c_str());  // NOLINT

    assert(second_lock == 0);
  }
}
