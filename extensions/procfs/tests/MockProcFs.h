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

#pragma once

#include <filesystem>
#include "utils/file/FileUtils.h"
#include "../ProcFs.h"

namespace org::apache::nifi::minifi::extensions::procfs::tests {

std::filesystem::path get_procfs_test_dir() {
  return std::filesystem::path(org::apache::nifi::minifi::utils::file::FileUtils::get_executable_dir()) / "procfs-test";
}

ProcFs mock_proc_fs_t0() {
  return ProcFs(get_procfs_test_dir() / "mockprocfs_t0");
}

ProcFs mock_proc_fs_t1() {
  return ProcFs(get_procfs_test_dir() / "mockprocfs_t1");
}

}  // namespace org::apache::nifi::minifi::extensions::procfs::tests
