// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
